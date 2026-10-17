#include "RecoilAssistHandler.h"
#include "controller.h"
#include "SessionContext.h"
#include "keymap.h"
#include "fastmsg.h"
#include "ConfigCenter.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#define LOG_TAG "RecoilAssistHandler"
#include "Logger.h"

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kCurvePeriodSec = 0.9;    // 曲线摆动周期（秒）
constexpr double kCurveAmplitude = 0.30;   // 曲线横向幅度（相对垂直速率的比例）
constexpr double kMicroJitter = 0.0002;    // 绝对微抖幅度（归一化屏高）
constexpr double kMaxStepMs = 100.0;       // 卡顿保护：单步最多积分 100ms

inline double clamp01(double v)
{
    return std::clamp(v, 0.0, 1.0);
}

inline uint64_t nowUs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

}  // namespace

RecoilAssistHandler::RecoilAssistHandler()
    : m_rng(std::random_device{}())
{
}

RecoilAssistHandler::~RecoilAssistHandler()
{
    reset();
}

void RecoilAssistHandler::init(Controller* controller, SessionContext* context)
{
    IInputHandler::init(controller, context);

    // 周期回调固定在主线程执行（NativeTimer 内部经 dispatch::postToMain 投递）
    m_tickTimer.setSingleShot(false);
    m_tickTimer.setCallback([this]() { onTick(); });

    // 松键宽限定时器（单次）
    m_releaseGraceTimer.setSingleShot(true);
    m_releaseGraceTimer.setCallback([this]() { onReleaseGraceExpired(); });
}

double RecoilAssistHandler::randUnit()
{
    return std::uniform_real_distribution<double>(0.0, 1.0)(m_rng);
}

bool RecoilAssistHandler::handleKeyEvent(const InputEvent& event, const Size& frameSize, const Size& showSize)
{
    // 责任链调用：这里拿不到修饰键，可能找不到正确的配置。
    // 正常入口是 processKeyEvent（由 InputDispatcher 传入已解析好的 node）。
    if (!m_keyMap) return false;

    const KeyMap::KeyMapNode& node = m_keyMap->getKeyMapNodeKey(event.key);
    if (node.type != KeyMap::KMT_RECOIL_ASSIST) {
        return false;
    }

    processKeyEvent(node, event, frameSize, showSize);
    return true;
}

bool RecoilAssistHandler::handleMouseEvent(const InputEvent& event, const Size& frameSize, const Size& showSize)
{
    (void)event;
    (void)frameSize;
    (void)showSize;
    // 鼠标热键由 InputDispatcher::processMouseClick 转换成等效键盘事件后调用 processKeyEvent
    return false;
}

bool RecoilAssistHandler::handleWheelEvent(const InputEvent& event, const Size& frameSize, const Size& showSize)
{
    (void)event;
    (void)frameSize;
    (void)showSize;
    return false;
}

void RecoilAssistHandler::processKeyEvent(const KeyMap::KeyMapNode& node, const InputEvent& event,
                                          const Size& frameSize, const Size& showSize)
{
    (void)frameSize;
    (void)showSize;

    const bool isPress = (event.type == InputEventType::KeyPress);
    const bool isRelease = (event.type == InputEventType::KeyRelease);

    if (isPress) {
        if (m_state.active) {
            // 正在等宽限窗口时又收到按下 ⇒ 这是一次「释放→按下」抖动，
            // 撤销待执行的抬指，开火触点全程保持按下，游戏不会看到松手。
            if (m_pendingStop) {
                m_pendingStop = false;
                m_releaseGraceTimer.stop();
                LOGI() << "[recoil] 吸收一次抖动释放（<" << qsc::ConfigCenter::instance().recoilReleaseGraceMs()
                       << "ms），开火触点保持按下";
            }
            return;
        }
        startRecoil(node);
        return;
    }

    if (isRelease && m_state.active) {
        const int grace = std::clamp(qsc::ConfigCenter::instance().recoilReleaseGraceMs(), 0, 500);
        if (grace > 0) {
            // 先不抬指：若 grace 毫秒内又来按下，就当成抖动忽略；
            // 否则宽限到期才真正停止（真人松开只会多按住 grace 毫秒）。
            m_pendingStop = true;
            m_releaseGraceTimer.start(grace);
        } else {
            stopRecoil("release");
        }
    }
}

void RecoilAssistHandler::onReleaseGraceExpired()
{
    if (!m_pendingStop) {
        return;
    }
    m_pendingStop = false;
    if (m_state.active) {
        stopRecoil("release-grace");
    }
}

void RecoilAssistHandler::startRecoil(const KeyMap::KeyMapNode& node)
{
    qsc::ConfigCenter& cc = qsc::ConfigCenter::instance();

    m_state.active = true;
    m_state.fireSeqId = 0;
    m_state.dragSeqId = 0;
    m_state.firePos = node.data.recoilAssist.keyNode.pos;  // 节点位置 = 开火按钮位置
    m_state.currentPos = m_state.firePos;
    m_state.travelled = 0.0;
    m_state.elapsedMs = 0.0;
    m_state.curvePhase = 0.0;

    // 1) 按住开火按钮：与手按住开火键完全等价
    if (m_controller) {
        m_state.fireSeqId = FastTouchSeq::next();
        sendTouch(m_state.fireSeqId, FTA_DOWN, m_state.firePos);
    }

    if (!cc.recoilEnabled()) {
        LOGI() << "[recoil] 压枪总开关已关闭：只按住开火按钮，不注入补偿拖拽";
        return;
    }

    // 2) 落下压枪触点并启动主线程周期注入
    m_state.dragStart = PointF(clamp01(cc.recoilDragX() / 100.0),
                               clamp01(cc.recoilDragY() / 100.0));
    m_state.currentPos = m_state.dragStart;

    if (m_controller) {
        m_state.dragSeqId = FastTouchSeq::next();
        sendTouch(m_state.dragSeqId, FTA_DOWN, m_state.currentPos);
    }

    // 注入频率：每次按压整体抖动一次（运行时改 setInterval 对已启动的定时器无效）
    const int baseInterval = std::clamp(cc.recoilStepIntervalMs(), 2, 50);
    int interval = baseInterval;
    const int randPct = std::clamp(cc.recoilRandomizeStep(), 0, 100);
    if (randPct > 0) {
        const double f = 1.0 + (randUnit() * 2.0 - 1.0) * (randPct / 100.0) * 0.5;
        interval = std::clamp(static_cast<int>(std::lround(baseInterval * f)), 2, 100);
    }

    m_state.lastTickUs = nowUs();
    m_tickTimer.setInterval(interval);
    m_tickTimer.start();

    LOGI() << "[recoil] 开始压枪: 开火点=(" << m_state.firePos.x << "," << m_state.firePos.y
           << ") 起点=(" << m_state.dragStart.x << "," << m_state.dragStart.y
           << ") 间隔=" << interval << "ms"
           << " 垂直速率=" << cc.recoilVerticalRate()
           << " 强度=" << cc.recoilStrength();
}

void RecoilAssistHandler::stopRecoil(const char* reason)
{
    // 停表：阻塞到正在执行的定时器回调结束，避免与下面的抬指竞争
    m_tickTimer.stop();
    m_releaseGraceTimer.stop();
    m_pendingStop = false;

    if (m_controller) {
        if (m_state.dragSeqId != 0) {
            sendTouch(m_state.dragSeqId, FTA_UP, m_state.currentPos);
        }
        if (m_state.fireSeqId != 0) {
            sendTouch(m_state.fireSeqId, FTA_UP, m_state.firePos);
        }
    }

    LOGI() << "[recoil] 停止压枪: 原因=" << (reason ? reason : "unknown")
           << " 时长=" << static_cast<int>(m_state.elapsedMs)
           << "ms 累计位移=" << m_state.travelled;

    m_state.active = false;
    m_state.fireSeqId = 0;
    m_state.dragSeqId = 0;
    m_state.travelled = 0.0;
    m_state.elapsedMs = 0.0;
    m_state.curvePhase = 0.0;
    m_state.lastTickUs = 0;
}

void RecoilAssistHandler::onTick()
{
    if (!m_state.active || m_state.dragSeqId == 0 || !m_controller) {
        return;
    }

    qsc::ConfigCenter& cc = qsc::ConfigCenter::instance();

    // ---- 真实步进时间：定时器可能被拖后，用真实 dt 积分更接近游戏里的实际位移 ----
    const uint64_t now = nowUs();
    if (m_state.lastTickUs == 0) {
        m_state.lastTickUs = now;
        return;
    }
    double dtMs = static_cast<double>(now - m_state.lastTickUs) / 1000.0;
    m_state.lastTickUs = now;
    if (dtMs <= 0.0) {
        return;
    }
    dtMs = std::min(dtMs, kMaxStepMs);
    m_state.elapsedMs += dtMs;
    const double dt = dtMs / 1000.0;

    const double strength = std::clamp(cc.recoilStrength(), 0, 300) / 100.0;
    // 正 = 手指向下拖 => 视角向下 => 压住上跳的枪口
    const double vRate = std::clamp(cc.recoilVerticalRate(), -300, 300) / 100.0;
    const double hRate = std::clamp(cc.recoilHorizontalRate(), -100, 100) / 100.0;

    // ---- 力度包络：起手渐入 -> 满力 ->（可选）后段衰减 ----
    const double rampUpMs = std::clamp(cc.recoilRampUpMs(), 0, 3000);
    const double decayMs = std::clamp(cc.recoilDecayMs(), 0, 3000);
    const double startRatio = std::clamp(cc.recoilRampUpStartRatio(), 0, 100) / 100.0;
    const double endRatio = std::clamp(cc.recoilDecayEndRatio(), 0, 100) / 100.0;

    double envelope = 1.0;
    if (rampUpMs > 0.0 && m_state.elapsedMs < rampUpMs) {
        const double t = m_state.elapsedMs / rampUpMs;
        envelope = startRatio + (1.0 - startRatio) * t;
    } else if (decayMs > 0.0) {
        const double t = (m_state.elapsedMs - rampUpMs) / decayMs;
        envelope = (t >= 1.0) ? endRatio : (1.0 + (endRatio - 1.0) * t);
    }

    // ---- 曲线：横向小幅摆动，避免死板的纯直线下拖 ----
    double curveV = 0.0;
    const double curveLevel = std::clamp(cc.recoilCurve(), 0, 100) / 100.0;
    if (curveLevel > 0.0) {
        curveV = std::sin(m_state.curvePhase) * curveLevel * kCurveAmplitude * std::fabs(vRate);
        m_state.curvePhase += dt * 2.0 * kPi / kCurvePeriodSec;
        if (m_state.curvePhase > 2.0 * kPi) {
            m_state.curvePhase -= 2.0 * kPi;
        }
    }

    // ---- 本步位移（归一化屏幕比例） ----
    const double jitter = std::clamp(cc.recoilJitter(), 0, 100) / 100.0;
    double stepY = vRate * strength * envelope * dt;
    double stepX = (hRate * strength * envelope + curveV * strength) * dt;

    if (jitter > 0.0) {
        const double f = 1.0 + (randUnit() * 2.0 - 1.0) * jitter * 0.5;
        stepY *= f;
        stepX *= f;
        stepX += (randUnit() * 2.0 - 1.0) * kMicroJitter * jitter;
        stepY += (randUnit() * 2.0 - 1.0) * kMicroJitter * jitter;
    }

    // ---- 位移上限：到顶就停手（保持开火），避免手指拖出屏幕后空转 ----
    const double maxDist = std::clamp(cc.recoilMaxDistance(), 1, 100) / 100.0;
    if (m_state.travelled + std::fabs(stepY) + std::fabs(stepX) > maxDist) {
        LOGI() << "[recoil] 达到最大补偿位移 " << maxDist << "，停止补偿（保持开火）";
        sendTouch(m_state.dragSeqId, FTA_UP, m_state.currentPos);
        m_state.dragSeqId = 0;
        m_tickTimer.stop();
        return;
    }
    m_state.travelled += std::fabs(stepY) + std::fabs(stepX);

    PointF newPos(m_state.currentPos.x + stepX, m_state.currentPos.y + stepY);

    // ---- 出界保护：手指已到屏幕边缘，继续拖不会再有位移 ----
    if (newPos.x < 0.0 || newPos.x > 1.0 || newPos.y < 0.0 || newPos.y > 1.0) {
        LOGI() << "[recoil] 拖拽触点到达屏幕边缘，停止补偿（保持开火）";
        sendTouch(m_state.dragSeqId, FTA_UP, m_state.currentPos);
        m_state.dragSeqId = 0;
        m_tickTimer.stop();
        return;
    }

    sendTouch(m_state.dragSeqId, FTA_MOVE, newPos);
    m_state.currentPos = newPos;
}

void RecoilAssistHandler::onFocusLost()
{
    reset();
}

void RecoilAssistHandler::reset()
{
    if (m_state.active) {
        stopRecoil("reset");
        return;
    }

    m_tickTimer.stop();
    m_releaseGraceTimer.stop();
    m_pendingStop = false;

    m_state.fireSeqId = 0;
    m_state.dragSeqId = 0;
    m_state.travelled = 0.0;
    m_state.elapsedMs = 0.0;
    m_state.curvePhase = 0.0;
    m_state.lastTickUs = 0;
}

void RecoilAssistHandler::sendTouch(uint32_t seqId, uint8_t action, const PointF& pos)
{
    if (!m_controller) return;

    const uint16_t nx = static_cast<uint16_t>(std::clamp(pos.x, 0.0, 1.0) * 65535);
    const uint16_t ny = static_cast<uint16_t>(std::clamp(pos.y, 0.0, 1.0) * 65535);

    // 栈缓冲区序列化，避免堆分配
    char buf[10];
    FastTouchEvent evt(seqId, action, nx, ny);
    const int len = FastMsg::serializeTouchInto(buf, evt);
    m_controller->postFastMsg(buf, len);
}
