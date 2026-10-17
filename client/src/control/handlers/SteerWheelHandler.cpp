#define _USE_MATH_DEFINES
#include <cmath>
#include "SteerWheelHandler.h"
#include "controller.h"
#include "SessionContext.h"
#include "fastmsg.h"
#include "ConfigCenter.h"
#define LOG_TAG "SteerWheelHandler"
#include "Logger.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

static thread_local std::mt19937 t_rng{std::random_device{}()};

static double randomDouble() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(t_rng);
}

static int randomBounded(int max) {
    std::uniform_int_distribution<int> dist(0, max - 1);
    return dist(t_rng);
}

// 标准正态分布
static double gaussian() {
    static std::normal_distribution<double> dist(0.0, 1.0);
    return dist(t_rng);
}

// 一阶 OU 过程（AR(1) 精确离散化）：n <- a*n + sqrt(1-a^2)*sigma*N(0,1)
// tauMs 为相关时间：tau 越小抖动越快，越小越接近白噪声。
static double ouStep(double n, double dtMs, double tauMs, double sigma) {
    if (sigma <= 0.0 || tauMs <= 0.0) {
        return 0.0;
    }
    double a = std::exp(-dtMs / tauMs);
    return a * n + std::sqrt(1.0 - a * a) * sigma * gaussian();
}

// 单调墙钟（毫秒），用于微颤 tick 的自节流
static uint64_t steadyNowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

// 整像素量化：把归一化坐标吸附到物理像素网格。
// 否则亚像素残差会被下游 scaleX 放大成"每帧 +1/-1 像素"的轴抖动。
static double quantizeAxis(double v, double quant) {
    if (quant <= 0.0) {
        return v;
    }
    return std::round(v / quant) * quant;
}

// 静态辅助函数：应用随机偏移
static PointF applyRandomOffset(const PointF& pos, const Size& targetSize,
                                double& quantW, double& quantH) {
    quantW = 0.0;
    quantH = 0.0;

    int offsetLevel = qsc::ConfigCenter::instance().randomOffset();
    if (offsetLevel <= 0 || targetSize.isEmpty()) {
        return pos;
    }

    // offsetLevel 0~100 映射到 0~50 像素
    double maxPixelOffset = offsetLevel * 0.5;

    // 圆盘分布：r = R*sqrt(u) 保证面积均匀。
    // 原来 X/Y 各自独立均匀分布会形成方形分布（且中心过密），
    // 真人落指在目标周围的散布是各向同性的圆盘/二维正态。
    double theta = randomDouble() * 2.0 * M_PI;
    double r = maxPixelOffset * std::sqrt(randomDouble());

    // 生成随机偏移（像素）
    double offsetX = r * std::cos(theta);
    double offsetY = r * std::sin(theta);

    // 转换为归一化偏移量
    double normalizedOffsetX = offsetX / targetSize.width;
    double normalizedOffsetY = offsetY / targetSize.height;

    // 整像素量化网格：发送时取整到物理像素，避免残留亚像素抖动
    quantW = 1.0 / targetSize.width;
    quantH = 1.0 / targetSize.height;

    PointF result = pos;
    result.x += normalizedOffsetX;
    result.y += normalizedOffsetY;

    // 确保结果在 0~1 范围内
    result.x = std::clamp(result.x, 0.001, 0.999);
    result.y = std::clamp(result.y, 0.001, 0.999);

    return result;
}

// 静态辅助函数：获取目标尺寸
static Size getTargetSize(const Size& frameSize, const Size& showSize, const Size& mobileSize) {
    if (mobileSize.isValid() && !mobileSize.isEmpty()) return mobileSize;
    if (frameSize.isValid() && !frameSize.isEmpty()) return frameSize;
    return showSize;
}

SteerWheelHandler::SteerWheelHandler()
{
    // 初始化定时器
    m_state.delayData.timer.setSingleShot(true);
    m_state.delayData.timer.setCallback([this]() { onSteerWheelTimer(); });

    m_state.humanizeTimer.setSingleShot(true);
    m_state.humanizeTimer.setCallback([this]() { onHumanizeTimer(); });

    // 高频微颤：周期定时器，按住期间持续输出 ~100Hz 触摸流
    m_state.jitterTimer.setSingleShot(false);
    m_state.jitterTimer.setInterval(m_state.jitterIntervalMs);
    m_state.jitterTimer.setCallback([this]() { onJitterTimer(); });

    m_state.resetRepressTimer.setSingleShot(true);
    m_state.resetRepressTimer.setInterval(20);
    m_state.resetRepressTimer.setCallback([this]() { onResetRepressTimer(); });
}

SteerWheelHandler::~SteerWheelHandler()
{
    reset();
}

void SteerWheelHandler::init(Controller* controller, SessionContext* context)
{
    IInputHandler::init(controller, context);
}

bool SteerWheelHandler::handleKeyEvent(const InputEvent& event,
                                       const Size& frameSize,
                                       const Size& showSize)
{
    if (!m_keyMap) return false;

    m_frameSize = frameSize;
    m_showSize = showSize;

    int key = event.key;
    const KeyMap::KeyMapNode& node = m_keyMap->getKeyMapNodeKey(key);

    // 只处理轮盘类型
    if (node.type != KeyMap::KMT_STEER_WHEEL) {
        return false;
    }

    // 检查是否是轮盘的按键
    bool isSteerKey = (key == node.data.steerWheel.up.key ||
                       key == node.data.steerWheel.down.key ||
                       key == node.data.steerWheel.left.key ||
                       key == node.data.steerWheel.right.key);

    if (!isSteerKey) {
        return false;
    }

    processSteerWheel(node, event);
    return true;  // 消费事件
}

void SteerWheelHandler::onFocusLost()
{
    reset();
}

void SteerWheelHandler::reset()
{
    m_state.jitterTimer.stop();
    m_state.humanizeTimer.stop();
    m_state.delayData.timer.stop();
    m_state.resetRepressTimer.stop();
    m_state.waitingForResetRepress = false;

    if (m_state.fastTouchSeqId != 0) {
        sendFastTouch(FTA_UP, m_state.delayData.currentPos);
        m_state.fastTouchSeqId = 0;
    }

    m_state.pressedUp = false;
    m_state.pressedDown = false;
    m_state.pressedLeft = false;
    m_state.pressedRight = false;
    m_state.delayData.pressedNum = 0;
    m_state.delayData.queuePos.clear();
    m_state.delayData.stepTimerMs = 0;

    // 拟人化状态也要一起复位，否则切换场景后会残留上一次的角度/长度/噪声
    m_state.currentAngleOffset = 0.0;
    m_state.currentLengthFactor = 1.0;
    m_state.targetAngleOffset = 0.0;
    m_state.targetLengthFactor = 1.0;
    m_state.lastPressedState = 0;
    m_state.noiseAngleFast = 0.0;
    m_state.noiseAngleSlow = 0.0;
    m_state.noiseRadiusFast = 0.0;
    m_state.noiseRadiusSlow = 0.0;
    m_state.lastJitterTickMs = 0;
    m_state.pressBaseValid = false;
    m_state.quantW = 0.0;
    m_state.quantH = 0.0;
}

void SteerWheelHandler::setCoefficient(double up, double down, double left, double right)
{
    if (m_keyMap) {
        m_keyMap->setSteerWheelCoefficient(up, down, left, right);

        // 如果轮盘正在活动，立即触发重新计算
        if (m_state.fastTouchSeqId != 0 && m_state.delayData.pressedNum > 0) {
            const KeyMap::KeyMapNode* node = m_keyMap->getSteerWheelNode();
            if (node) {
                m_state.delayData.timer.stop();
                m_state.delayData.queuePos.clear();
                m_state.delayData.stepTimerMs = 0;
                executeMove(*node);
            }
        }
    }
}

void SteerWheelHandler::resetCoefficient()
{
    if (m_keyMap) {
        m_keyMap->setSteerWheelCoefficient(1.0, 1.0, 1.0, 1.0);
    }
}

void SteerWheelHandler::resetWheel()
{
    // 用于场景切换后重置轮盘状态（如跑步时按F进车）
    // 游戏内的轮盘已经被重置，但用户仍然按着方向键

    // 如果已经在等待 resetRepress，只需确保定时器继续即可
    if (m_state.waitingForResetRepress) {
        return;
    }

    // 1. 停止所有定时器和清空队列
    m_state.jitterTimer.stop();
    if (m_state.delayData.timer.isActive()) {
        m_state.delayData.timer.stop();
    }
    m_state.delayData.queuePos.clear();
    m_state.delayData.stepTimerMs = 0;

    // 2. 释放当前触摸（如果有）
    if (m_state.fastTouchSeqId != 0) {
        sendFastTouch(FTA_UP, m_state.delayData.currentPos);
        m_state.fastTouchSeqId = 0;
    }

    // 3. 如果仍有方向键按住，延迟 re-trigger（给游戏时间处理 UP）
    int pressedNum = 0;
    if (m_state.pressedUp) ++pressedNum;
    if (m_state.pressedRight) ++pressedNum;
    if (m_state.pressedDown) ++pressedNum;
    if (m_state.pressedLeft) ++pressedNum;

    if (pressedNum > 0 && m_keyMap) {
        m_state.delayData.pressedNum = pressedNum;
        // 延迟 20ms 再 DOWN，让游戏至少处理一帧 UP
        m_state.waitingForResetRepress = true;
        m_state.resetRepressTimer.start();
    }
}

void SteerWheelHandler::onSteerWheelTimer()
{
    if (m_state.delayData.queuePos.empty()) {
        return;
    }

    int batchSize = 1;
    if (m_state.delayData.queuePos.size() > 12) {
        batchSize = 3;
    } else if (m_state.delayData.queuePos.size() > 6) {
        batchSize = 2;
    }

    for (int i = 0; i < batchSize && !m_state.delayData.queuePos.empty(); ++i) {
        m_state.delayData.currentPos = m_state.delayData.queuePos.front();
        m_state.delayData.queuePos.pop_front();
        sendFastTouch(FTA_MOVE, m_state.delayData.currentPos);
    }

    if (m_state.delayData.queuePos.empty() && m_state.delayData.pressedNum == 0) {
        sendFastTouch(FTA_UP, m_state.delayData.currentPos);
        m_state.fastTouchSeqId = 0;
        return;
    }

    if (!m_state.delayData.queuePos.empty()) {
        m_state.delayData.timer.start(static_cast<int>(std::max<uint32_t>(1, m_state.delayData.stepTimerMs)));
    }
}

// 高频微颤：按住期间以 ~100Hz 输出触摸，用 OU 噪声叠加在基础偏移上。
// 这是真人操作与"恒定向量"最容易被感知的差别——手指按住方向键时，
// 轮盘读数始终在目标值附近小幅游走，而不是锁死在一个常数值上。
void SteerWheelHandler::onJitterTimer()
{
    if (m_state.fastTouchSeqId == 0 || m_state.delayData.pressedNum <= 0) {
        return;
    }

    // 主线程拥塞时丢弃过期的 tick，避免恢复后瞬间连发几十个 MOVE
    uint64_t now = steadyNowMs();
    if (m_state.lastJitterTickMs != 0 &&
        now - m_state.lastJitterTickMs > static_cast<uint64_t>(m_state.jitterIntervalMs * 4)) {
        m_state.lastJitterTickMs = now;
        return;
    }
    double dtMs = (m_state.lastJitterTickMs == 0)
                      ? static_cast<double>(m_state.jitterIntervalMs)
                      : static_cast<double>(now - m_state.lastJitterTickMs);
    m_state.lastJitterTickMs = now;

    int humanize = qsc::ConfigCenter::instance().steerWheelHumanize();
    if (humanize <= 0) {
        return;
    }

    const double h = humanize / 100.0;

    // 角度噪声：快分量（τ=50ms，覆盖 8~12Hz 生理性震颤）+ 慢分量（τ=700ms，按住时缓慢调整方向）
    m_state.noiseAngleFast = ouStep(m_state.noiseAngleFast, dtMs, 50.0, 1.0 * h);
    m_state.noiseAngleSlow = ouStep(m_state.noiseAngleSlow, dtMs, 700.0, 0.6 * h);

    // 长度噪声：幅值更小，模拟手指按压力度/位置的自然变化
    m_state.noiseRadiusFast = ouStep(m_state.noiseRadiusFast, dtMs, 100.0, 0.45 * h);
    m_state.noiseRadiusSlow = ouStep(m_state.noiseRadiusSlow, dtMs, 900.0, 0.8 * h);

    // 噪声以"增量"形式叠加在上一次下发的位置上（围绕按下基准点旋转/缩放）。
    // 若每个 tick 都从基础偏移重新加噪声，方向会随噪声整个跳变（抽动感）；
    // 增量式叠加得到的是连续游走，与手指震颤的频谱一致。
    double newAngleNoise = m_state.noiseAngleFast + m_state.noiseAngleSlow;
    double newRadiusNoise = m_state.noiseRadiusFast + m_state.noiseRadiusSlow;

    double dAngle = std::clamp((newAngleNoise - m_state.noiseAngleApplied) * 0.0022 * M_PI,
                               -0.02, 0.02);
    double dRadius = std::clamp((newRadiusNoise - m_state.noiseRadiusApplied) * 0.004,
                                -0.03, 0.03);
    m_state.noiseAngleApplied = newAngleNoise;
    m_state.noiseRadiusApplied = newRadiusNoise;

    PointF rel = m_state.delayData.currentPos - m_state.pressBasePos;
    double cosA = std::cos(dAngle);
    double sinA = std::sin(dAngle);
    PointF moved(rel.x * cosA - rel.y * sinA, rel.x * sinA + rel.y * cosA);
    moved *= (1.0 + dRadius);

    PointF targetPos = m_state.pressBasePos + moved;
    targetPos.x = std::clamp(targetPos.x, 0.0, 1.0);
    targetPos.y = std::clamp(targetPos.y, 0.0, 1.0);

    m_state.delayData.currentPos = targetPos;
    sendFastTouch(FTA_MOVE, targetPos);
}

void SteerWheelHandler::onResetRepressTimer()
{
    if (!m_state.waitingForResetRepress) return;
    m_state.waitingForResetRepress = false;

    // 重新计算当前按下的键数（可能在延迟期间有变化）
    int pressedNum = 0;
    if (m_state.pressedUp) ++pressedNum;
    if (m_state.pressedRight) ++pressedNum;
    if (m_state.pressedDown) ++pressedNum;
    if (m_state.pressedLeft) ++pressedNum;
    m_state.delayData.pressedNum = pressedNum;

    if (pressedNum > 0 && m_keyMap) {
        const KeyMap::KeyMapNode* node = m_keyMap->getSteerWheelNode();
        if (node) {
            executeMove(*node);
        }
    }
}

void SteerWheelHandler::onHumanizeTimer()
{
    // 只有轮盘活动时才触发波动
    if (m_state.fastTouchSeqId == 0 || m_state.delayData.pressedNum <= 0) {
        return;
    }

    // 生成新的轻微波动目标（比状态变化时更小的波动）
    // 角度：±10%的轻微波动（相比状态变化时的±30%）
    double level = std::clamp(qsc::ConfigCenter::instance().steerWheelHumanize() / 100.0, 0.0, 1.0);
    double angleVariation = 0.10 * level;
    m_state.targetAngleOffset = (randomDouble() * 2.0 - 1.0) * angleVariation * M_PI / 4.0;

    // 长度：±5%的轻微波动（相比状态变化时的±10%）
    double lengthVariation = 0.05 * level;
    m_state.targetLengthFactor = 1.0 + (randomDouble() * 2.0 - 1.0) * lengthVariation;

    // 触发轮盘重新计算（会平滑过渡到新目标）
    if (m_keyMap) {
        const KeyMap::KeyMapNode* node = m_keyMap->getSteerWheelNode();
        if (node) {
            executeMove(*node);
        }
    }

    // 设置下次波动时间（2-8秒随机）
    int nextInterval = 2000 + randomBounded(6000);
    m_state.humanizeTimer.start(nextInterval);
}

void SteerWheelHandler::processSteerWheel(const KeyMap::KeyMapNode& node, const InputEvent& event)
{
    int key = event.key;
    bool flag = event.isPress();

    // 更新按键状态
    if (key == node.data.steerWheel.up.key) {
        m_state.pressedUp = flag;
    } else if (key == node.data.steerWheel.right.key) {
        m_state.pressedRight = flag;
    } else if (key == node.data.steerWheel.down.key) {
        m_state.pressedDown = flag;
    } else {
        m_state.pressedLeft = flag;
    }

    // 计算当前按下的键数
    int pressedNum = 0;
    if (m_state.pressedUp) ++pressedNum;
    if (m_state.pressedRight) ++pressedNum;
    if (m_state.pressedDown) ++pressedNum;
    if (m_state.pressedLeft) ++pressedNum;
    m_state.delayData.pressedNum = pressedNum;

    // 所有键都松开了
    if (pressedNum == 0) {
        m_state.jitterTimer.stop();
        m_state.humanizeTimer.stop();
        m_state.lastJitterTickMs = 0;
        m_state.pressBaseValid = false;

        // 取消 resetWheel 延迟 re-trigger
        if (m_state.waitingForResetRepress) {
            m_state.resetRepressTimer.stop();
            m_state.waitingForResetRepress = false;
        }

        if (m_state.delayData.timer.isActive()) {
            m_state.delayData.timer.stop();
        }
        m_state.delayData.queuePos.clear();
        m_state.delayData.stepTimerMs = 0;

        if (m_state.fastTouchSeqId != 0) {
            sendFastTouch(FTA_UP, m_state.delayData.currentPos);
            m_state.fastTouchSeqId = 0;
        }
        return;
    }

    // resetWheel 延迟中：按键状态已更新，timer callback 会用最新状态
    if (m_state.waitingForResetRepress) {
        return;
    }

    // 首次按下：直接执行，不等待组合键检测
    executeMove(node);

    // 按住期间启动高频微颤（拟人强度为 0 时不启动，行为与改造前一致）
    if (m_state.delayData.pressedNum > 0 &&
        qsc::ConfigCenter::instance().steerWheelHumanize() > 0) {
        if (!m_state.jitterTimer.isActive()) {
            m_state.lastJitterTickMs = 0;
            m_state.jitterTimer.start();
        }
    }
}

void SteerWheelHandler::executeMove(const KeyMap::KeyMapNode& node)
{
    // 计算当前按键状态哈希
    int currentState = (m_state.pressedUp ? 1 : 0) |
                       (m_state.pressedDown ? 2 : 0) |
                       (m_state.pressedLeft ? 4 : 0) |
                       (m_state.pressedRight ? 8 : 0);

    // 检测按键状态变化
    bool stateChanged = (currentState != m_state.lastPressedState);
    if (stateChanged) {
        m_state.lastPressedState = currentState;

        // 抖动幅度随拟人强度缩放：强度为 0 时目标就是真值，currentAngleOffset 收敛到 0
        double level = std::clamp(qsc::ConfigCenter::instance().steerWheelHumanize() / 100.0, 0.0, 1.0);

        double angleVariation = 0.30 * level;
        m_state.targetAngleOffset = (randomDouble() * 2.0 - 1.0) * angleVariation * M_PI / 4.0;

        double lengthVariation = 0.10 * level;
        m_state.targetLengthFactor = 1.0 + (randomDouble() * 2.0 - 1.0) * lengthVariation;

        if (!m_state.humanizeTimer.isActive() && currentState != 0) {
            int nextInterval = 2000 + randomBounded(6000);
            m_state.humanizeTimer.start(nextInterval);
        }
    }

    // 平滑过渡（宏观慢漂移，~2-8s 时间尺度）
    double smoothFactor = 0.2;
    m_state.currentAngleOffset += (m_state.targetAngleOffset - m_state.currentAngleOffset) * smoothFactor;
    m_state.currentLengthFactor += (m_state.targetLengthFactor - m_state.currentLengthFactor) * smoothFactor;

    int pressedNum = m_state.delayData.pressedNum;
    if (pressedNum <= 0) {
        return;
    }

    PointF baseOffset = computeBaseOffset(node);

    m_state.delayData.timer.stop();
    m_state.delayData.queuePos.clear();
    m_state.delayData.stepTimerMs = 0;

    // 判断是否需要插值动画
    int smoothLevel = qsc::ConfigCenter::instance().steerWheelSmooth();
    int curveLevel = qsc::ConfigCenter::instance().steerWheelCurve();
    bool directMode = (smoothLevel == 0 && curveLevel == 0);
    double speedScale = node.data.steerWheel.speedMultiplier;

    // 如果还没开始触摸，先按下（应用随机偏移）
    if (m_state.fastTouchSeqId == 0) {
        Size ms = m_sessionContext ? m_sessionContext->mobileSize() : Size();
        Size targetSize = getTargetSize(m_frameSize, m_showSize, ms);
        PointF pressBase = applyRandomOffset(node.data.steerWheel.centerPos, targetSize,
                                             m_state.quantW, m_state.quantH);

        // 采样即冻结：按住期间所有 MOVE 都以这个点为原点。
        // 改造前每次 executeMove 都重算一次随机零位，而下发时用的是 centerPos，
        // 两个基准不同心，等于按住期间持续做真实平移（最多可达轮盘半径的 31%）。
        m_state.pressBasePos = pressBase;
        m_state.pressBaseValid = true;
        m_state.noiseAngleApplied = 0.0;
        m_state.noiseRadiusApplied = 0.0;

        m_state.fastTouchSeqId = FastTouchSeq::next();
        m_state.delayData.currentPos = pressBase;
        sendFastTouch(FTA_DOWN, pressBase);

        PointF targetPos = pressBase + applyHumanize(baseOffset, pressedNum);
        if (directMode) {
            if (!buildHumanRamp(pressBase, targetPos,
                                m_state.delayData.queuePos, m_state.delayData.stepTimerMs)) {
                sendFastTouch(FTA_MOVE, targetPos);
                m_state.delayData.currentPos = targetPos;
            }
        } else {
            getDelayQueue(pressBase, targetPos,
                          0.01, 0.002, 10, 80, speedScale,
                          m_state.delayData.queuePos,
                          m_state.delayData.stepTimerMs);
        }
    } else {
        if (!m_state.pressBaseValid) {
            m_state.pressBasePos = m_state.delayData.currentPos;
            m_state.pressBaseValid = true;
        }

        PointF targetPos = m_state.pressBasePos + applyHumanize(baseOffset, pressedNum);

        // 状态切换时的预测性过冲：手指换方向不会精确停在目标上，而是先冲过一点再回正
        // （二阶欠阻尼）。过冲量按本次位移比例取，并设绝对值上限，避免大步长时甩飞。
        bool overshooting = false;
        PointF pathEnd = targetPos;
        if (stateChanged) {
            PointF d = targetPos - m_state.delayData.currentPos;
            double dlen = std::sqrt(d.x * d.x + d.y * d.y);
            if (dlen > 1e-6) {
                double overshoot = std::min(0.15, 0.02 / dlen);
                if (overshoot > 0.0) {
                    pathEnd = targetPos + d * overshoot;
                    overshooting = true;
                }
            }
        }

        if (directMode) {
            if (!buildHumanRamp(m_state.delayData.currentPos, pathEnd,
                                m_state.delayData.queuePos, m_state.delayData.stepTimerMs)) {
                sendFastTouch(FTA_MOVE, pathEnd);
                m_state.delayData.currentPos = pathEnd;
            }
        } else {
            getDelayQueue(m_state.delayData.currentPos, pathEnd,
                          0.01, 0.002, 10, 80, speedScale,
                          m_state.delayData.queuePos,
                          m_state.delayData.stepTimerMs);
        }

        // 过冲回正：附加 2 个短点把过冲的量收回来（均匀采样，收尾很快）
        if (overshooting && !m_state.delayData.queuePos.empty()) {
            for (int i = 1; i <= 2; ++i) {
                double t = i / 2.0;
                m_state.delayData.queuePos.push_back(PointF(
                    pathEnd.x + (targetPos.x - pathEnd.x) * t,
                    pathEnd.y + (targetPos.y - pathEnd.y) * t));
            }
        }
    }

    if (!m_state.delayData.queuePos.empty()) {
        m_state.delayData.timer.start(static_cast<int>(std::max<uint32_t>(1, m_state.delayData.stepTimerMs)));
    }

    // 所有按键都松开，停止间隙波动定时器
    if (currentState == 0) {
        m_state.humanizeTimer.stop();
        m_state.jitterTimer.stop();
        m_state.lastJitterTickMs = 0;
    }
}

// 基础偏移：把当前按键组合映射成归一化向量（不含任何拟人化修正）
PointF SteerWheelHandler::computeBaseOffset(const KeyMap::KeyMapNode& node) const
{
    PointF offset(0.0, 0.0);
    if (!m_keyMap) {
        return offset;
    }

    if (m_state.pressedUp) {
        offset.y -= node.data.steerWheel.up.extendOffset * m_keyMap->getSteerWheelCoefficient(0);
    }
    if (m_state.pressedDown) {
        offset.y += node.data.steerWheel.down.extendOffset * m_keyMap->getSteerWheelCoefficient(1);
    }
    if (m_state.pressedLeft) {
        offset.x -= node.data.steerWheel.left.extendOffset * m_keyMap->getSteerWheelCoefficient(2);
    }
    if (m_state.pressedRight) {
        offset.x += node.data.steerWheel.right.extendOffset * m_keyMap->getSteerWheelCoefficient(3);
    }

    return offset;
}

// 拟人化修正：长度系数 + 对角方向的角度抖动。
// 拟人强度为 0 时 currentAngleOffset/currentLengthFactor 恒为 0/1，等价于纯映射。
PointF SteerWheelHandler::applyHumanize(const PointF& baseOffset, int pressedNum) const
{
    PointF result = baseOffset;

    // 长度系数：任何方向都应用（模拟按压力度/落点的差异）
    result *= m_state.currentLengthFactor;

    // 角度抖动：只在对角方向应用。
    // 单方向时绕中心旋转不改变游戏读到的方向（摇杆只取主方向），
    // 只会让触摸点沿切向漂移，看起来像手指在打滑。
    if (pressedNum > 1 && (result.x != 0.0 || result.y != 0.0)) {
        double cosA = std::cos(m_state.currentAngleOffset);
        double sinA = std::sin(m_state.currentAngleOffset);
        result = PointF(result.x * cosA - result.y * sinA,
                        result.x * sinA + result.y * cosA);
    }

    return result;
}

// 直接模式（平滑=0 且曲线=0）下的拟人短过渡。
// 返回 false 表示未生成中间点（未开启拟人化），调用方按原逻辑直接下发。
bool SteerWheelHandler::buildHumanRamp(const PointF& start, const PointF& end,
                                       std::deque<PointF>& queuePos, uint32_t& stepTimerMs) const
{
    int humanize = qsc::ConfigCenter::instance().steerWheelHumanize();
    if (humanize <= 0) {
        return false;
    }

    double level = std::clamp(humanize / 100.0, 0.0, 1.0);
    double dx = end.x - start.x;
    double dy = end.y - start.y;
    double distance = std::sqrt(dx * dx + dy * dy);

    // 位移太小（游戏本身无法分辨）时不必过渡
    if (distance < 0.002) {
        return false;
    }

    int points = 2 + static_cast<int>(std::round(level * 2.0));  // 2~4 个中间点
    uint32_t totalMs = 10 + static_cast<uint32_t>(std::round(level * 10.0));  // 10~20ms

    stepTimerMs = std::max<uint32_t>(1, totalMs / static_cast<uint32_t>(points));

    for (int i = 1; i <= points; ++i) {
        double t = static_cast<double>(i) / (points + 1);
        // 轻微钟形：起手快、收尾稳，避免机械的匀速直线
        double e = t + 0.15 * std::sin(M_PI * t) * (1.0 - t);
        e = std::clamp(e, 0.0, 1.0);
        queuePos.push_back(PointF(start.x + dx * e, start.y + dy * e));
    }

    return true;
}

void SteerWheelHandler::sendFastTouch(uint8_t action, const PointF& pos)
{
    if (!m_controller) return;

    uint16_t nx = static_cast<uint16_t>(std::clamp(
        quantizeAxis(pos.x, m_state.quantW), 0.0, 1.0) * 65535);
    uint16_t ny = static_cast<uint16_t>(std::clamp(
        quantizeAxis(pos.y, m_state.quantH), 0.0, 1.0) * 65535);

    // 栈缓冲区序列化，避免堆分配
    char buf[10];
    FastTouchEvent evt(m_state.fastTouchSeqId, action, nx, ny);
    int len = FastMsg::serializeTouchInto(buf, evt);
    m_controller->postFastMsg(buf, len);
}

void SteerWheelHandler::getDelayQueue(const PointF& start, const PointF& end,
                                      double distanceStep, double posStep,
                                      uint32_t lowestTimer, uint32_t highestTimer,
                                      double speedScale,
                                      std::deque<PointF>& queuePos, uint32_t& stepTimerMs)
{
    (void)posStep;

    // 获取平滑度和曲线设置
    int smoothLevel = qsc::ConfigCenter::instance().steerWheelSmooth();
    int curveLevel = qsc::ConfigCenter::instance().steerWheelCurve();

    double x1 = start.x;
    double y1 = start.y;
    double x2 = end.x;
    double y2 = end.y;

    double dx = x2 - x1;
    double dy = y2 - y1;
    double distance = std::sqrt(dx * dx + dy * dy);

    if (distance < 0.0001) {
        queuePos.push_back(end);
        stepTimerMs = lowestTimer;
        return;
    }

    // smooth=0 且 curve=0 时，一步到位，不做任何插值动画
    if (smoothLevel == 0 && curveLevel == 0) {
        queuePos.push_back(end);
        stepTimerMs = 1;
        return;
    }

    double smoothRatio = smoothLevel / 100.0;
    double curveRatio = curveLevel / 100.0;
    int baseSteps = static_cast<int>(std::ceil(distance / std::max(distanceStep, 0.0001)));
    int maxSteps = 10 + static_cast<int>(std::round(curveRatio * 14.0));
    int steps = std::clamp(static_cast<int>(std::round(baseSteps * (1.2 + curveRatio * 1.8))),
                           1, std::max(1, maxSteps));

    // 垂直方向向量（用于曲线偏移）
    double perpX = -dy / distance;
    double perpY = dx / distance;

    // ===== 多频率曲线叠加，模拟真实人手的多曲度轨迹 =====

    // 主曲线（1个周期）：整体弧度方向，幅度最大
    double mainDirection = (randomBounded(2) == 0) ? 1.0 : -1.0;
    double mainAmplitude = (curveLevel / 100.0) * 0.2 * distance;

    // 次级波动（1.5-2.5个周期）：中间的起伏变化
    double secondFreq = 1.5 + randomDouble();  // 1.5 ~ 2.5
    double secondDirection = (randomBounded(2) == 0) ? 1.0 : -1.0;
    double secondAmplitude = (curveLevel / 100.0) * 0.08 * distance;

    // 微小波动（3-5个周期）：细微的不规则抖动
    double microFreq = 3.0 + randomDouble() * 2.0;  // 3 ~ 5
    double microDirection = (randomBounded(2) == 0) ? 1.0 : -1.0;
    double microAmplitude = (curveLevel / 100.0) * 0.03 * distance;

    // 随机相位偏移，让每次轨迹不同
    double mainPhase = randomDouble() * 0.2;      // 0 ~ 0.2
    double secondPhase = randomDouble() * M_PI;   // 0 ~ π
    double microPhase = randomDouble() * M_PI * 2; // 0 ~ 2π

    // ===== 时长预算：与步数解耦 =====
    // 改造前 totalDuration = 6 + distance*28 + smoothRatio*36，再除以 steps；
    // 而 steps 本身 ∝ distance，距离一长单步时长就被压到 1~2ms（被 lowestTimer 夹住），
    // 平滑参数形同失效。这里把单步时长直接由平滑度决定，总时长随距离自然增长并设上限。
    double humanizeLevel = std::clamp(qsc::ConfigCenter::instance().steerWheelHumanize() / 100.0, 0.0, 1.0);

    // 单步间隔：10ms 是真人触控采样的下限；开启拟人化时略微放慢，轨迹更"肉"
    double baseStepMs = 10.0 + smoothRatio * 60.0 * (0.6 + 0.4 * humanizeLevel);
    double speed = (speedScale > 0.05) ? speedScale : 1.0;
    double stepMs = baseStepMs / speed;

    // 总时长上限：长距离过渡也不能拖沓
    double maxTotalMs = 60.0 + distance * 400.0 + curveRatio * 40.0;
    if (stepMs * steps > maxTotalMs) {
        stepMs = maxTotalMs / steps;
    }

    stepTimerMs = static_cast<uint32_t>(std::clamp(std::round(stepMs),
                                                   static_cast<double>(lowestTimer),
                                                   static_cast<double>(highestTimer)));

    for (int i = 1; i <= steps; i++) {
        double t = static_cast<double>(i) / steps;

        // 末点精确落在目标上，避免剖面误差导致终点差一点点
        if (i == steps) {
            queuePos.push_back(end);
            break;
        }

        // 速度剖面：钟形加减速（起手加速、收尾减速），而不是匀速直线。
        // 拟人强度为 0 时退化为线性插值，与改造前完全一致。
        double e = t * t * (3.0 - 2.0 * t);
        e = t + (e - t) * humanizeLevel;

        // 插值基础位置
        double baseX = x1 + dx * e;
        double baseY = y1 + dy * e;

        // 多频率曲线叠加
        // 主曲线：sin(π*(t+phase))，在中间达到最大，首尾归零
        double mainOffset = std::sin(M_PI * (t + mainPhase)) * mainAmplitude * mainDirection;
        // 首尾衰减，确保起点终点在直线上
        mainOffset *= std::sin(M_PI * t);

        // 次级波动：添加中间的起伏变化
        double secondOffset = std::sin(secondFreq * M_PI * t + secondPhase) * secondAmplitude * secondDirection;
        // 首尾衰减
        secondOffset *= std::sin(M_PI * t);

        // 微小波动：细微的不规则抖动
        double microOffset = std::sin(microFreq * M_PI * t + microPhase) * microAmplitude * microDirection;
        // 首尾衰减
        microOffset *= std::sin(M_PI * t);

        // 合并所有曲线偏移
        double totalOffset = mainOffset + secondOffset + microOffset;

        // 应用曲线偏移
        double finalX = baseX + perpX * totalOffset;
        double finalY = baseY + perpY * totalOffset;

        queuePos.push_back(PointF(finalX, finalY));
    }
}
