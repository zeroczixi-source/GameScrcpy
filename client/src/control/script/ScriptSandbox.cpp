#include "ScriptSandbox.h"
#include "ScriptWatchdog.h"
#include "ScriptEngine.h"
#include "controller.h"
#include "SessionContext.h"
#include "StringUtils.h"
#include "fastmsg.h"
#include "keycodes.h"
#include "ConfigCenter.h"
#include "ScriptTipWidget.h"
#include "selectionregionmanager.h"
#include "scriptbuttonmanager.h"
#include "scriptswipemanager.h"
#include "JsEngine.h"
#include "JsBindings.h"
#include "ThreadDispatcher.h"
#include <filesystem>
#include <opencv2/core.hpp>

#ifdef ENABLE_IMAGE_MATCHING
#include "imagematcher.h"
#endif

#define LOG_TAG "ScriptSandbox"
#include "Logger.h"
#include <random>
#include <chrono>
#include <thread>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// C++ 标准随机数辅�?(替代 QRandomGenerator)
static thread_local std::mt19937 t_rng{std::random_device{}()};

static double randomDouble() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(t_rng);
}

static int randomBounded(int max) {
    std::uniform_int_distribution<int> dist(0, max - 1);
    return dist(t_rng);
}

// C++ 标准时间辅助 (替代 QDateTime::currentMSecsSinceEpoch)
static int64_t currentMSecsSinceEpoch() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static constexpr auto kClickReleaseDelay = std::chrono::milliseconds(20);

// ============================================================
// 线程局部帧缓存 - 同一线程短时间内多次 findImage 复用同一�?
// 避免每次找图都重新抓帧（�?+ 拷贝开销�?
// ============================================================
struct ThreadLocalFrameCache {
    cv::Mat frame;
    int64_t timestamp = 0;       // 抓帧时间戳(ms)
    static constexpr int64_t CACHE_TTL_MS = 50; // 缓存有效期50ms

    cv::Mat getFrame() {
        int64_t now = currentMSecsSinceEpoch();
        if (!frame.empty() && (now - timestamp) < CACHE_TTL_MS) {
            return frame;
        }
        // 缓存过期，重新抓帧
        frame = ScriptEngine::grabCurrentFrame();
        timestamp = now;
        return frame;
    }

    void invalidate() {
        frame = cv::Mat();
        timestamp = 0;
    }
};

static thread_local ThreadLocalFrameCache t_frameCache;

// ============================================================
// 线程局部灰度帧缓存 - 零拷贝图像匹配使�?
// 直接获取 Y 分量，避�?YUV→RGB→Gray 的无意义转换
// ============================================================
struct ThreadLocalGrayFrameCache {
    GrayFrame frame;
    int64_t timestamp = 0;
    static constexpr int64_t CACHE_TTL_MS = 50;

    const GrayFrame& getFrame() {
        int64_t now = currentMSecsSinceEpoch();
        if (frame.isValid() && (now - timestamp) < CACHE_TTL_MS) {
            return frame;
        }
        frame = ScriptEngine::grabCurrentGrayFrame();
        timestamp = now;
        return frame;
    }

    void invalidate() {
        frame = GrayFrame();
        timestamp = 0;
    }
};

static thread_local ThreadLocalGrayFrameCache t_grayFrameCache;

// ============================================================
// 线程局部模板图片缓�?- 避免每次从磁盘加�?PNG
// ============================================================
struct ThreadLocalTemplateCache {
    std::unordered_map<std::string, cv::Mat> cache;
    static constexpr int MAX_CACHE_SIZE = 64; // 最多缓存64张模板

    cv::Mat getTemplate(const std::string& name) {
        auto it = cache.find(name);
        if (it != cache.end()) {
            return it->second;
        }
        // 缓存未命中，从磁盘加载
        cv::Mat img = ImageMatcher::loadTemplateImage(name);
        if (!img.empty()) {
            // loadTemplateImage 已以灰度模式加载，无需额外转换
            // 缓存满时清理一半
            if (static_cast<int>(cache.size()) >= MAX_CACHE_SIZE) {
                int count = 0;
                for (auto cit = cache.begin(); cit != cache.end() && count < static_cast<int>(cache.size()) / 2; ) {
                    cit = cache.erase(cit);
                    ++count;
                }
            }
            cache[name] = img;
        }
        return img;
    }
};

static thread_local ThreadLocalTemplateCache t_templateCache;

// ============================================================
// 线程局部找图结果缓�?- 同一帧内相同参数的找图直接返回缓�?
// 避免 UI 刷新循环中对同一图片重复执行 OpenCV 模板匹配
// ============================================================
struct FindResultEntry {
    FindImageResult result;
    int64_t timestamp;
};

struct ThreadLocalFindResultCache {
    std::unordered_map<std::string, FindResultEntry> cache;
    static constexpr int64_t CACHE_TTL_MS = 50;
    static constexpr int MAX_ENTRIES = 64;

    static std::string makeKey(const std::string& name, double x1, double y1, double x2, double y2, double threshold) {
        return strutil::format("%s|%d,%d,%d,%d|%d",
            name.c_str(),
            static_cast<int>(std::round(x1 * 10000)),
            static_cast<int>(std::round(y1 * 10000)),
            static_cast<int>(std::round(x2 * 10000)),
            static_cast<int>(std::round(y2 * 10000)),
            static_cast<int>(std::round(threshold * 10000)));
    }

    bool tryGet(const std::string& key, FindImageResult& out) {
        auto it = cache.find(key);
        if (it != cache.end()) {
            int64_t now = currentMSecsSinceEpoch();
            if ((now - it->second.timestamp) < CACHE_TTL_MS) {
                out = it->second.result;
                return true;
            }
            cache.erase(it);
        }
        return false;
    }

    void put(const std::string& key, const FindImageResult& result) {
        if (static_cast<int>(cache.size()) >= MAX_ENTRIES) {
            int64_t now = currentMSecsSinceEpoch();
            for (auto it = cache.begin(); it != cache.end(); ) {
                if ((now - it->second.timestamp) >= CACHE_TTL_MS) {
                    it = cache.erase(it);
                } else {
                    ++it;
                }
            }
        }
        FindResultEntry entry;
        entry.result = result;
        entry.timestamp = currentMSecsSinceEpoch();
        cache[key] = entry;
    }
};

static thread_local ThreadLocalFindResultCache t_findResultCache;

// =============================================================
// ScriptSandbox 实现
// =============================================================

// 静态成员初始化
int ScriptSandbox::s_maxTouchPoints = 10;

void ScriptSandbox::setMaxTouchPoints(int max)
{
    s_maxTouchPoints = std::clamp(max, 1, 50);
}

int ScriptSandbox::maxTouchPoints()
{
    return s_maxTouchPoints;
}

ScriptSandbox::ScriptSandbox(int sandboxId, Controller* controller, SessionContext* ctx)
    : m_sandboxId(sandboxId)
    , m_controller(controller)
    , m_sessionContext(ctx)
{
    // 创建看门狗（保持在主线程）
    m_watchdog = new ScriptWatchdog(30000);
    m_watchdog->softTimeout.connect([this]() { onSoftTimeout(); });
    m_watchdog->hardTimeout.connect([this]() { onHardTimeout(); });
}

ScriptSandbox::~ScriptSandbox()
{
    // 先作废生命周期令牌：脚本线程已投递到主线程、尚未执行的回调会被丢弃，
    // 避免它们在沙箱析构后仍访问 m_running / fire 信号 / m_watchdog
    if (m_aliveToken) {
        m_aliveToken->store(false, std::memory_order_release);
    }

    stop();

    if (m_thread.joinable()) {
        if (m_running.load()) {
            // 契约被破坏：只有在调用方错误地销毁了一个仍在运行的沙箱时才会出现
            LOGE() << "[ScriptSandbox" << m_sandboxId
                   << "] destroyed while script thread still running";
        }
        m_thread.join();
    }

    delete m_watchdog;
    m_watchdog = nullptr;
}

void ScriptSandbox::setScript(const std::string& script)
{
    m_script = script;
    m_isInlineScript = true;
}

void ScriptSandbox::setScriptPath(const std::string& path)
{
    m_scriptPath = path;
    m_isInlineScript = false;
}

void ScriptSandbox::setTimeoutMs(int ms)
{
    m_watchdog->setTimeoutMs(ms);
}

// v14: SEH 包裹函数（MSVC 不允许 __try 与 C++ 对象展开在同一函数中）
// 防御脚本引擎中的 SEH 异常（如 access violation）
// 说明：__try/__except 是 MSVC 扩展，MinGW/GCC 不支持，故仅 MSVC 编译此函数；
//      其他编译器走 start() 里的 #else 分支（直接调 runScript()）。
#if defined(_WIN32) && defined(_MSC_VER)
static bool runScriptWithSEH(ScriptSandbox* sandbox, void (ScriptSandbox::*fn)())
{
    __try {
        (sandbox->*fn)();
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
#endif

void ScriptSandbox::start()
{
    if (m_running.exchange(true)) return;

    m_stopRequested.store(false);

    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_watchdog->start();

    m_thread = std::thread([this]() {
        bool ok = true;
#if defined(_WIN32) && defined(_MSC_VER)
        ok = runScriptWithSEH(this, &ScriptSandbox::runScript);
#else
        runScript();
#endif
        if (!ok) {
            LOGE() << "[ScriptSandbox" << m_sandboxId << "] SEH exception caught in script thread";
            dispatch::postToMain([this, alive = m_aliveToken]() {
                if (!alive || !alive->load(std::memory_order_acquire)) return;
                scriptError.fire("[ScriptSandbox] SEH exception in script thread");
            });
        }
        // 收尾顺序（关键）：先把线程要用的值拷到栈上，再把 m_running 归零，
        // 最后才投递 finished。这样主线程收到 finished 去 delete 本对象时，
        // 本线程已经不再读取 this 的任何成员，~ScriptSandbox 的 join() 必定立刻返回。
        auto alive = m_aliveToken;
        const int finishedId = m_sandboxId;
        m_running.store(false, std::memory_order_release);

        // 线程即将结束 — 在主线程触发 finished 信号
        dispatch::postToMain([this, alive, finishedId]() {
            if (!alive || !alive->load(std::memory_order_acquire)) return;
            if (m_watchdog) {
                m_watchdog->stop();
            }
            finished.fire(finishedId);
        });
    });
}

void ScriptSandbox::stop()
{
    m_stopRequested.store(true);

    JsEngine* engine = m_jsEngine.load();
    if (engine) {
        engine->setInterrupted(true);
    }

    // 注意：这里不再写 m_running。m_running 的唯一含义是"脚本线程仍在运行"，
    // 只允许由脚本线程在真正退出时置 false，否则 stopAll()/forceTerminate() 的
    // 等待循环与 isRunning() 会全部失效。

    // NativeTimer 可以从任何线程停止
    if (m_watchdog) {
        m_watchdog->stop();
    }
}

bool ScriptSandbox::forceTerminate()
{
    LOGW() << "[ScriptSandbox" << m_sandboxId << "] forceTerminate() called";
    stop();
    m_sessionContext.store(nullptr);

    // 断开数据信号（保留 finished 信号用于清理）
    touchRequested.disconnectAll();
    keyRequested.disconnectAll();
    tipRequested.disconnectAll();
    shotmodeRequested.disconnectAll();
    radialParamRequested.disconnectAll();
    resetviewRequested.disconnectAll();
    resetWheelRequested.disconnectAll();
    simulateKeyRequested.disconnectAll();
    keyUIPosRequested.disconnectAll();
    scriptError.disconnectAll();

    // 等待线程结束（带超时）
    //
    // 绝不 detach：脚本线程整个生命周期都持有裸 this，detach 后析构本对象
    // 会让线程写已释放内存。因此超时只报告失败，由调用方决定是否放弃销毁
    // （ScriptEngine 会故意泄漏该沙箱，让 this 保持有效）。
    if (m_thread.joinable()) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (m_running.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (m_running.load()) {
            LOGE() << "[ScriptSandbox" << m_sandboxId
                   << "] Thread still running after 2s, giving up (object must be leaked)";
            return false;
        }
    }

    return true;
}

void ScriptSandbox::clearSessionContext()
{
    m_sessionContext.store(nullptr);
    // 标记中断，但不直接调�?stop()（stop 中的 watchdog->stop 可能跨线程）
    m_stopRequested.store(true);
    JsEngine* engine = m_jsEngine.load();
    if (engine) {
        engine->setInterrupted(true);
    }
    // 同上：不写 m_running，停止请求不等于线程已退出
    // 看门狗在主线程停�?
    if (m_watchdog) {
        m_watchdog->stop();
    }
}

std::string ScriptSandbox::resolveModulePath(const std::string& modulePath)
{
    namespace fs = std::filesystem;
    fs::path mp(strutil::toWide(modulePath));
    if (mp.is_absolute()) {
        return modulePath;
    }

    std::string basePath = m_scriptBasePath;
    if (basePath.empty()) {
        basePath = strutil::fromWide(fs::current_path().wstring()) + "/keymap/scripts";
    }

    fs::path fullPath = fs::path(strutil::toWide(basePath)) / mp;
    std::string result = strutil::fromWide(fullPath.wstring());

    if (!strutil::endsWith(result, ".js") && !strutil::endsWith(result, ".mjs")) {
        std::string withJs = result + ".js";
        if (fs::exists(fs::path(strutil::toWide(withJs)))) {
            return withJs;
        }
    }

    return result;
}

void ScriptSandbox::onSoftTimeout()
{
    LOGW() << "[ScriptSandbox" << m_sandboxId << "] Soft timeout, attempting graceful interrupt...";
    JsEngine* engine = m_jsEngine.load();
    if (engine) {
        engine->setInterrupted(true);
    }
}

void ScriptSandbox::onHardTimeout()
{
    LOGW() << "[ScriptSandbox" << m_sandboxId << "] Hard timeout, requesting stop (thread is never detached)";

    // 再次尝试中断（可能上次没生效�?
    JsEngine* engine = m_jsEngine.load();
    if (engine) {
        engine->setInterrupted(true);
    }

    // 清除外部引用，切断脚本与外界的联系
    m_sessionContext.store(nullptr);

    // 断开数据信号（保留 finished）
    touchRequested.disconnectAll();
    keyRequested.disconnectAll();
    tipRequested.disconnectAll();
    shotmodeRequested.disconnectAll();
    radialParamRequested.disconnectAll();
    resetviewRequested.disconnectAll();
    resetWheelRequested.disconnectAll();
    simulateKeyRequested.disconnectAll();
    keyUIPosRequested.disconnectAll();
    scriptError.disconnectAll();

    // 不写 m_running：本函数只表达"请求停止"，线程退出由脚本线程自己报告。
    // 也绝不强制终止线程！让 setInterrupted(true) 使 JS 代码自然停止
}

void ScriptSandbox::runScript()
{
    // 在工作线程中创建 JS 引擎
    JsEngine engine;
    m_jsEngine.store(&engine);
    engine.installConsoleExtension();

    // 创建沙箱专用 API（在工作线程中创建）
    SandboxScriptApi* api = new SandboxScriptApi(this);
    api->setScriptBasePath(m_scriptBasePath);
    api->setKeyId(m_keyId);
    api->setAnchorPos(m_anchorPos);
    api->setIsPress(m_isPress);
    api->setSessionContext(m_sessionContext.load());

    // 注册 mapi 对象 + logerror 全局函数（替代 QJSEngine::newQObject + Q_INVOKABLE）
    registerApiBindings(engine.context(), api);

    std::string error;
    bool scriptFailed = false;

    if (m_isInlineScript) {
        if (!engine.evaluate(m_script, error)) {
            scriptFailed = true;
        }
    } else {
        std::string fullPath = resolveModulePath(m_scriptPath);
        if (!std::filesystem::exists(std::filesystem::path(strutil::toWide(fullPath)))) {
            std::string errorMsg = strutil::format("Sandbox %d script file not found: %s",
                m_sandboxId, fullPath.c_str());
            LOGW() << errorMsg;
            dispatch::postToMain([this, alive = m_aliveToken, errorMsg]() {
                if (!alive || !alive->load(std::memory_order_acquire)) return;
                scriptError.fire(errorMsg);
            });
            m_jsEngine.store(nullptr);
            clearModuleCache();
            delete api;
            return;
        }
        JSValue result = engine.importModule(fullPath, error);
        if (!error.empty()) {
            scriptFailed = true;
        }
        JS_FreeValue(engine.context(), result);
    }

    if (scriptFailed) {
        std::string errorMsg = strutil::format("Sandbox %d script error: %s",
            m_sandboxId, error.c_str());
        if (!engine.isInterrupted()) {
            LOGW() << errorMsg;
            dispatch::postToMain([this, alive = m_aliveToken, errorMsg]() {
                if (!alive || !alive->load(std::memory_order_acquire)) return;
                scriptError.fire(errorMsg);
            });
        }
    }

    // 清理
    m_jsEngine.store(nullptr);

    SessionContext* ctx = m_sessionContext.load();

    // 释放残留触摸点（防止幽灵触摸�?
    // - 释放脚本�?m_isPress）结束后：释放所有残留触摸（安全网）
    // - 按下脚本（m_isPress）被中断/停止时：释放（因为对应的释放脚本可能不会执行�?
    // - 按下脚本正常完成时：不释放！触摸需要保留给对应的释放脚�?
    bool shouldRelease = !m_isPress || m_stopRequested.load();
    if (shouldRelease && ctx && ctx->hasTouchSeqs(api->keyId())) {
        std::vector<uint32_t> seqIds = ctx->takeTouchSeqs(api->keyId());
        LOGW() << "[ScriptSandbox" << m_sandboxId << "] Releasing" << seqIds.size()
                   << "ghost touch(es) for keyId=" << api->keyId();
        for (uint32_t seqId : seqIds) {
            dispatch::postToMain([this, alive = m_aliveToken, seqId]() {
                if (!alive || !alive->load(std::memory_order_acquire)) return;
                touchRequested.fire(seqId, FTA_UP, 32767, 32767);
            });
        }
    }

    // 清理轮盘参数
    if (!m_isPress && ctx) {
        std::string keyIdStr = std::to_string(api->keyId());
        if (ctx->radialParamKeyId() == keyIdStr) {
            ctx->setRadialParamKeyId(std::string());
            dispatch::postToMain([this, alive = m_aliveToken]() {
                if (!alive || !alive->load(std::memory_order_acquire)) return;
                radialParamRequested.fire(1.0, 1.0, 1.0, 1.0);
            });
        }
    }

    clearModuleCache();
    delete api;
}

// =============================================================
// SandboxScriptApi 实现
// =============================================================

SandboxScriptApi::SandboxScriptApi(ScriptSandbox* sandbox)
    : m_sandbox(sandbox),
      m_aliveToken(sandbox ? sandbox->aliveToken() : nullptr)
{
}

void SandboxScriptApi::normalizePos(double x, double y, uint16_t& outX, uint16_t& outY)
{
    double tx = std::clamp(x, 0.0, 1.0);
    double ty = std::clamp(y, 0.0, 1.0);
    outX = static_cast<uint16_t>(tx * 65535.0);
    outY = static_cast<uint16_t>(ty * 65535.0);
}

PointF SandboxScriptApi::applyRandomOffset(double x, double y)
{
    int offsetLevel = qsc::ConfigCenter::instance().randomOffset();
    if (offsetLevel <= 0) {
        return PointF(x, y);
    }

    double maxOffset = offsetLevel * 0.0003;

    double offsetX = (randomDouble() - 0.5) * 2.0 * maxOffset;
    double offsetY = (randomDouble() - 0.5) * 2.0 * maxOffset;

    double resultX = std::clamp(x + offsetX, 0.001, 0.999);
    double resultY = std::clamp(y + offsetY, 0.001, 0.999);

    return PointF(resultX, resultY);
}

std::vector<PointF> SandboxScriptApi::generateSmoothPath(double sx, double sy, double ex, double ey, int steps)
{
    std::vector<PointF> path;
    if (steps < 1) steps = 1;

    double dx = ex - sx;
    double dy = ey - sy;
    double distance = std::sqrt(dx * dx + dy * dy);

    if (distance < 0.0001) {
        path.push_back(PointF(ex, ey));
        return path;
    }

    int curveLevel = qsc::ConfigCenter::instance().slideCurve();

    double perpX = -dy / distance;
    double perpY = dx / distance;

    double mainDirection = (randomBounded(2) == 0) ? 1.0 : -1.0;
    double mainAmplitude = (curveLevel / 100.0) * 0.15 * distance;

    double secondFreq = 1.5 + randomDouble();
    double secondDirection = (randomBounded(2) == 0) ? 1.0 : -1.0;
    double secondAmplitude = (curveLevel / 100.0) * 0.06 * distance;

    double microFreq = 3.0 + randomDouble() * 2.0;
    double microDirection = (randomBounded(2) == 0) ? 1.0 : -1.0;
    double microAmplitude = (curveLevel / 100.0) * 0.02 * distance;

    double mainPhase = randomDouble() * 0.2;
    double secondPhase = randomDouble() * M_PI;
    double microPhase = randomDouble() * M_PI * 2;

    for (int i = 1; i <= steps; i++) {
        double t = static_cast<double>(i) / steps;

        double baseX = sx + dx * t;
        double baseY = sy + dy * t;

        double mainOffset = std::sin(M_PI * (t + mainPhase)) * mainAmplitude * mainDirection;
        mainOffset *= std::sin(M_PI * t);

        double secondOffset = std::sin(secondFreq * M_PI * t + secondPhase) * secondAmplitude * secondDirection;
        secondOffset *= std::sin(M_PI * t);

        double microOffset = std::sin(microFreq * M_PI * t + microPhase) * microAmplitude * microDirection;
        microOffset *= std::sin(M_PI * t);

        double totalOffset = mainOffset + secondOffset + microOffset;

        double finalX = baseX + perpX * totalOffset;
        double finalY = baseY + perpY * totalOffset;

        finalX = std::clamp(finalX, 0.001, 0.999);
        finalY = std::clamp(finalY, 0.001, 0.999);

        path.push_back(PointF(finalX, finalY));
    }

    return path;
}

void SandboxScriptApi::click(double x, double y)
{
    if (!m_isPress) return;
    if (isInterrupted()) return;

    uint32_t seqId = FastTouchSeq::next();
    uint16_t nx, ny;
    double px = (x < 0) ? m_anchorPos.x : x;
    double py = (y < 0) ? m_anchorPos.y : y;

    PointF randomPos = applyRandomOffset(px, py);
    normalizePos(randomPos.x, randomPos.y, nx, ny);

    ScriptSandbox* sandbox = m_sandbox;
    dispatch::postToMain([sandbox, alive = m_aliveToken, seqId, nx, ny]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->touchRequested.fire(seqId, FTA_DOWN, nx, ny);
    });

    std::this_thread::sleep_for(kClickReleaseDelay);

    dispatch::postToMain([sandbox, alive = m_aliveToken, seqId, nx, ny]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->touchRequested.fire(seqId, FTA_UP, nx, ny);
    });
}

void SandboxScriptApi::holdpress(double x, double y)
{
    if (isInterrupted()) return;

    // 安全获取 SessionContext 指针（防止竞争条件）
    SessionContext* ctx = m_sessionContext.load();
    if (!ctx) {
        // SessionContext 已销毁，停止脚本
        LOGW() << "[SandboxAPI] holdpress keyId=" << m_keyId << "ctx=null, stopping sandbox";
        if (m_sandbox) m_sandbox->stop();
        return;
    }

    double px = (x < 0) ? m_anchorPos.x : x;
    double py = (y < 0) ? m_anchorPos.y : y;

    PointF randomPos = applyRandomOffset(px, py);
    uint16_t nx, ny;
    normalizePos(randomPos.x, randomPos.y, nx, ny);

    if (m_isPress) {
        uint32_t seqId = FastTouchSeq::next();
        ctx->addTouchSeq(m_keyId, seqId);
        ScriptSandbox* sandbox = m_sandbox;
        dispatch::postToMain([sandbox, alive = m_aliveToken, seqId, nx, ny]() {
            if (!alive || !alive->load(std::memory_order_acquire)) return;
            sandbox->touchRequested.fire(seqId, FTA_DOWN, nx, ny);
        });
    } else {
        if (ctx->hasTouchSeqs(m_keyId)) {
            std::vector<uint32_t> seqIds = ctx->takeTouchSeqs(m_keyId);
            ScriptSandbox* sandbox = m_sandbox;
            for (uint32_t seqId : seqIds) {
                dispatch::postToMain([sandbox, alive = m_aliveToken, seqId, nx, ny]() {
                    if (!alive || !alive->load(std::memory_order_acquire)) return;
                    sandbox->touchRequested.fire(seqId, FTA_UP, nx, ny);
                });
            }
        }
    }
}

void SandboxScriptApi::releaseAll()
{
    // 本键位登记的所有触点都会被抬起，包括 touchDown 建立的持久触点
    m_heldTouchSeqId = 0;

    // 安全获取 SessionContext 指针
    SessionContext* ctx = m_sessionContext.load();
    if (!ctx) return;

    if (ctx->hasTouchSeqs(m_keyId)) {
        std::vector<uint32_t> seqIds = ctx->takeTouchSeqs(m_keyId);
        ScriptSandbox* sandbox = m_sandbox;
        for (uint32_t seqId : seqIds) {
            dispatch::postToMain([sandbox, alive = m_aliveToken, seqId]() {
                if (!alive || !alive->load(std::memory_order_acquire)) return;
                sandbox->touchRequested.fire(seqId, FTA_UP, 32767, 32767);
            });
        }
    }
}

void SandboxScriptApi::release()
{
    uint32_t seqId = FastTouchSeq::next();
    ScriptSandbox* sandbox = m_sandbox;
    dispatch::postToMain([sandbox, alive = m_aliveToken, seqId]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->touchRequested.fire(seqId, FTA_UP, 32767, 32767);
    });
}

// ============================================================
// 低层触摸注入：touchDown / touchMove / touchUp
// ------------------------------------------------------------
// 语义（与真实手指一致）：
//   touchDown(x, y)   按下并把触点交给脚本持有（不抬指）
//   touchMove(x, y)   把「同一触点」移动到新位置（复用同一个 seqId = 连续拖动）
//   touchUp()         抬起该触点
// 只在按住侧脚本（m_isPress == true）生效；touchUp 额外允许在松开侧脚本中调用，
// 用于清理（此时退化为 releaseAll）。
// 注意：touchMove 不做随机偏移，抖动/曲线等拟人化由脚本侧决定，保持底层可控。
// ============================================================

void SandboxScriptApi::touchDown(double x, double y)
{
    if (!m_isPress) return;
    if (isInterrupted()) return;

    SessionContext* ctx = m_sessionContext.load();
    if (!ctx) {
        LOGW() << "[SandboxAPI] touchDown keyId=" << m_keyId << " ctx=null, stopping sandbox";
        if (m_sandbox) m_sandbox->stop();
        return;
    }

    double px = (x < 0) ? m_anchorPos.x : x;
    double py = (y < 0) ? m_anchorPos.y : y;

    // 已经持有触点时，本次调用退化为移动到目标位置
    if (m_heldTouchSeqId != 0) {
        touchMove(px, py);
        return;
    }

    PointF randomPos = applyRandomOffset(px, py);

    uint16_t nx = 0, ny = 0;
    normalizePos(randomPos.x, randomPos.y, nx, ny);

    uint32_t seqId = FastTouchSeq::next();
    m_heldTouchSeqId = seqId;
    m_heldTouchX = nx;
    m_heldTouchY = ny;

    // 登记到本键位，保证脚本异常结束（停止/看门狗）时由收尾安全网兜底抬指
    ctx->addTouchSeq(m_keyId, seqId);

    ScriptSandbox* sandbox = m_sandbox;
    dispatch::postToMain([sandbox, alive = m_aliveToken, seqId, nx, ny]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->touchRequested.fire(seqId, FTA_DOWN, nx, ny);
    });
}

void SandboxScriptApi::touchMove(double x, double y)
{
    if (!m_isPress) return;
    if (isInterrupted()) return;

    // 尚未按下时自动补一次按下，方便脚本直接用绝对坐标开始拖动
    if (m_heldTouchSeqId == 0) {
        touchDown(x, y);
        return;
    }

    // 与 click/holdpress/touchDown 一致：负数坐标表示使用键位节点坐标
    double px = (x < 0) ? m_anchorPos.x : x;
    double py = (y < 0) ? m_anchorPos.y : y;

    uint16_t nx = 0, ny = 0;
    normalizePos(px, py, nx, ny);

    m_heldTouchX = nx;
    m_heldTouchY = ny;

    uint32_t seqId = m_heldTouchSeqId;
    ScriptSandbox* sandbox = m_sandbox;
    dispatch::postToMain([sandbox, alive = m_aliveToken, seqId, nx, ny]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->touchRequested.fire(seqId, FTA_MOVE, nx, ny);
    });
}

void SandboxScriptApi::touchUp()
{
    SessionContext* ctx = m_sessionContext.load();
    if (!ctx) return;

    uint32_t seqId = m_heldTouchSeqId;

    if (seqId == 0) {
        // 松开侧脚本（或无持有触点时）的调用退化为 releaseAll
        releaseAll();
        return;
    }

    // 只撤回自己这一个触点的登记：其余触点（例如 holdpress 按住的开火按钮）
    // 必须保留登记，否则松开侧的 releaseAll 与收尾安全网都找不到它，会漏成幽灵触点。
    std::vector<uint32_t> seqIds = ctx->takeTouchSeqs(m_keyId);
    bool stillRegistered = false;
    for (uint32_t id : seqIds) {
        if (id == seqId) {
            stillRegistered = true;
        } else {
            ctx->addTouchSeq(m_keyId, id);
        }
    }

    m_heldTouchSeqId = 0;

    // 已被松开侧脚本的 releaseAll 抬起过：不再重复发 UP（避免无主触点的幽灵抬指）
    if (!stillRegistered) return;

    uint16_t ux = m_heldTouchX;
    uint16_t uy = m_heldTouchY;
    ScriptSandbox* sandbox = m_sandbox;
    dispatch::postToMain([sandbox, alive = m_aliveToken, seqId, ux, uy]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->touchRequested.fire(seqId, FTA_UP, ux, uy);
    });
}

void SandboxScriptApi::slide(double sx, double sy, double ex, double ey, int delayMs, int num)
{
    if (!m_isPress) return;
    if (isInterrupted()) return;
    if (num <= 0) num = 10;

    uint32_t seqId = FastTouchSeq::next();
    uint16_t nx, ny;

    PointF startPos = applyRandomOffset(sx, sy);
    PointF endPos = applyRandomOffset(ex, ey);

    std::vector<PointF> path = generateSmoothPath(startPos.x, startPos.y, endPos.x, endPos.y, num);

    normalizePos(startPos.x, startPos.y, nx, ny);
    ScriptSandbox* sandbox = m_sandbox;
    dispatch::postToMain([sandbox, alive = m_aliveToken, seqId, nx, ny]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->touchRequested.fire(seqId, FTA_DOWN, nx, ny);
    });

    int stepTime = (path.size() > 0) ? (delayMs / static_cast<int>(path.size())) : delayMs;
    if (stepTime < 2) stepTime = 2;

    for (int i = 0; i < static_cast<int>(path.size()) && !isInterrupted(); ++i) {
        sleep(stepTime);
        normalizePos(path[i].x, path[i].y, nx, ny);
        dispatch::postToMain([sandbox, alive = m_aliveToken, seqId, nx, ny]() {
            if (!alive || !alive->load(std::memory_order_acquire)) return;
            sandbox->touchRequested.fire(seqId, FTA_MOVE, nx, ny);
        });
    }

    normalizePos(endPos.x, endPos.y, nx, ny);
    dispatch::postToMain([sandbox, alive = m_aliveToken, seqId, nx, ny]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->touchRequested.fire(seqId, FTA_UP, nx, ny);
    });
}

void SandboxScriptApi::pinch(double centerX, double centerY, double scale, int durationMs, int steps)
{
    if (!m_isPress) return;
    if (isInterrupted()) return;
    if (steps <= 0) steps = 10;
    if (scale <= 0) scale = 1.0;

    PointF center = applyRandomOffset(centerX, centerY);

    double baseDistance = 0.1;
    double startDistance, endDistance;

    if (scale > 1.0) {
        startDistance = baseDistance;
        endDistance = baseDistance * scale;
    } else {
        startDistance = baseDistance / scale;
        endDistance = baseDistance;
    }

    uint32_t seqId1 = FastTouchSeq::next();
    uint32_t seqId2 = FastTouchSeq::next();

    uint16_t nx1, ny1, nx2, ny2;

    double startX1 = center.x - startDistance / 2;
    double startX2 = center.x + startDistance / 2;
    double startY1 = center.y;
    double startY2 = center.y;

    PointF pos1 = applyRandomOffset(startX1, startY1);
    PointF pos2 = applyRandomOffset(startX2, startY2);

    normalizePos(pos1.x, pos1.y, nx1, ny1);
    normalizePos(pos2.x, pos2.y, nx2, ny2);
    ScriptSandbox* sandbox = m_sandbox;
    dispatch::postToMain([sandbox, alive = m_aliveToken, seqId1, nx1, ny1, seqId2, nx2, ny2]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->touchRequested.fire(seqId1, FTA_DOWN, nx1, ny1);
        sandbox->touchRequested.fire(seqId2, FTA_DOWN, nx2, ny2);
    });

    int stepTime = durationMs / steps;
    if (stepTime < 2) stepTime = 2;
    double distanceStep = (endDistance - startDistance) / steps;

    for (int i = 1; i <= steps && !isInterrupted(); ++i) {
        sleep(stepTime);

        double currentDistance = startDistance + distanceStep * i;
        double x1 = center.x - currentDistance / 2;
        double x2 = center.x + currentDistance / 2;

        normalizePos(std::clamp(x1, 0.001, 0.999), startY1, nx1, ny1);
        normalizePos(std::clamp(x2, 0.001, 0.999), startY2, nx2, ny2);

        dispatch::postToMain([sandbox, alive = m_aliveToken, seqId1, nx1, ny1, seqId2, nx2, ny2]() {
            if (!alive || !alive->load(std::memory_order_acquire)) return;
            if (!sandbox) return;
            sandbox->touchRequested.fire(seqId1, FTA_MOVE, nx1, ny1);
            sandbox->touchRequested.fire(seqId2, FTA_MOVE, nx2, ny2);
        });
    }

    double endX1 = center.x - endDistance / 2;
    double endX2 = center.x + endDistance / 2;
    PointF endPos1 = applyRandomOffset(endX1, startY1);
    PointF endPos2 = applyRandomOffset(endX2, startY2);
    normalizePos(endPos1.x, endPos1.y, nx1, ny1);
    normalizePos(endPos2.x, endPos2.y, nx2, ny2);
    dispatch::postToMain([sandbox, alive = m_aliveToken, seqId1, nx1, ny1, seqId2, nx2, ny2]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->touchRequested.fire(seqId1, FTA_UP, nx1, ny1);
        sandbox->touchRequested.fire(seqId2, FTA_UP, nx2, ny2);
    });
}

void SandboxScriptApi::key(const std::string& keyName, int durationMs)
{
    if (!m_isPress) return;
    if (isInterrupted()) return;

    ScriptSandbox* sandbox = m_sandbox;
    dispatch::postToMain([sandbox, alive = m_aliveToken, keyName]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->simulateKeyRequested.fire(keyName, true);
    });

    if (durationMs > 0) {
        sleep(durationMs);
    }

    dispatch::postToMain([sandbox, alive = m_aliveToken, keyName]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->simulateKeyRequested.fire(keyName, false);
    });
}

void SandboxScriptApi::sleep(int ms)
{
    if (!m_isPress) return;
    if (ms <= 0) return;

    const int checkInterval = 50;  // 增大检查间隔，减少循环次数
    int remaining = ms;

    while (remaining > 0) {
        if (isInterrupted()) break;

        int sleepTime = std::min(remaining, checkInterval);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
        remaining -= sleepTime;

        // 喂狗，避免超�?
        if (m_sandbox && m_sandbox->m_watchdog) {
            m_sandbox->m_watchdog->feed();
        }
    }
}

bool SandboxScriptApi::isInterrupted()
{
    return !m_sandbox || m_sandbox->m_stopRequested.load();
}

void SandboxScriptApi::stop()
{
    if (m_sandbox) {
        m_sandbox->stop();
    }
}

void SandboxScriptApi::toast(const std::string& msg, int durationMs)
{
    if (!m_isPress) return;
    if (isInterrupted() || !m_sandbox->isRunning()) return;

    int duration = std::max(1, durationMs);
    int keyId = m_keyId;

    QString qMsg = strutil::toQ(msg);
    dispatch::postToMain([qMsg, duration, keyId]() {
        ScriptTipWidget::instance()->addMessage(qMsg, duration, keyId);
    });
}

void SandboxScriptApi::setGlobal(const std::string& key, const ScriptValue& value)
{
    if (!m_isPress || isInterrupted()) return;

    SessionContext* ctx = m_sessionContext.load();
    if (!ctx) return;

    ctx->setVar(key, value);
}

ScriptValue SandboxScriptApi::getGlobal(const std::string& key)
{
    if (isInterrupted()) return ScriptValue();

    SessionContext* ctx = m_sessionContext.load();
    if (!ctx) return ScriptValue();

    return ctx->getVar(key);
}

std::string SandboxScriptApi::resolveModulePath(const std::string& modulePath)
{
    namespace fs = std::filesystem;
    fs::path mp(strutil::toWide(modulePath));
    if (mp.is_absolute()) {
        return modulePath;
    }

    std::string basePath = m_scriptBasePath;
    if (basePath.empty()) {
        basePath = strutil::fromWide(fs::current_path().wstring()) + "/keymap/scripts";
    }

    fs::path fullPath = fs::path(strutil::toWide(basePath)) / mp;
    std::string result = strutil::fromWide(fullPath.wstring());

    if (!strutil::endsWith(result, ".js") && !strutil::endsWith(result, ".mjs")) {
        std::string withJs = result + ".js";
        if (fs::exists(fs::path(strutil::toWide(withJs)))) {
            return withJs;
        }
    }

    return result;
}

void SandboxScriptApi::log(const std::string& msg)
{
    LOGI() << "[Sandbox Script]" << msg;
}

void SandboxScriptApi::shotmode(bool gameMode)
{
    if (!m_isPress) return;
    ScriptSandbox* sandbox = m_sandbox;
    dispatch::postToMain([sandbox, alive = m_aliveToken, gameMode]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->shotmodeRequested.fire(gameMode);
    });
}

void SandboxScriptApi::setRadialParam(double up, double down, double left, double right)
{
    if (!m_isPress) return;

    // 安全获取 SessionContext 指针
    SessionContext* ctx = m_sessionContext.load();
    if (!ctx) return;

    ctx->setRadialParamKeyId(std::to_string(m_keyId));
    m_radialParamModified = true;
    ScriptSandbox* sandbox = m_sandbox;
    dispatch::postToMain([sandbox, alive = m_aliveToken, up, down, left, right]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->radialParamRequested.fire(up, down, left, right);
    });
}

void SandboxScriptApi::resetview()
{
    if (!m_isPress) return;
    ScriptSandbox* sandbox = m_sandbox;
    dispatch::postToMain([sandbox, alive = m_aliveToken]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->resetviewRequested.fire();
    });
}

void SandboxScriptApi::resetwheel()
{
    if (!m_isPress) return;
    ScriptSandbox* sandbox = m_sandbox;
    dispatch::postToMain([sandbox, alive = m_aliveToken]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->resetWheelRequested.fire();
    });
}

PosResult SandboxScriptApi::getmousepos()
{
    PosResult result;

    if (isInterrupted()) return result;

    // 安全获取 SessionContext 指针
    SessionContext* ctx = m_sessionContext.load();
    if (ctx) {
        PointF pos = ctx->script_getMousePos();
        result.x = std::round(pos.x * 10000.0) / 10000.0;
        result.y = std::round(pos.y * 10000.0) / 10000.0;
    }

    return result;
}

KeyPosResult SandboxScriptApi::getkeypos(const std::string& keyName)
{
    KeyPosResult result;

    if (isInterrupted()) return result;

    // 安全获取 SessionContext 指针
    SessionContext* ctx = m_sessionContext.load();
    if (ctx) {
        KeyPosResult kpr = ctx->script_getKeyPosByName(keyName);
        result.x = kpr.x;
        result.y = kpr.y;
        result.valid = kpr.valid;
    }

    return result;
}

int SandboxScriptApi::getKeyState(const std::string& keyName)
{
    if (isInterrupted()) return 0;

    // 安全获取 SessionContext 指针
    SessionContext* ctx = m_sessionContext.load();
    if (ctx) {
        return ctx->script_getKeyStateByName(keyName);
    }
    return 0;
}

void SandboxScriptApi::setKeyUIPos(const std::string& keyName, double x, double y, double xoffset, double yoffset)
{
    if (!m_isPress) return;
    double finalX = x + xoffset;
    double finalY = y + yoffset;
    ScriptSandbox* sandbox = m_sandbox;
    dispatch::postToMain([sandbox, alive = m_aliveToken, keyName, finalX, finalY]() {
        if (!alive || !alive->load(std::memory_order_acquire)) return;
        sandbox->keyUIPosRequested.fire(keyName, finalX, finalY);
    });
}

int SandboxScriptApi::getQtKey(const std::string& keyName)
{
    std::string k;
    k.reserve(keyName.size());
    for (char c : keyName) k += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (k == "SPACE" || k == " ") return GameKey::Key_Space;
    if (k == "ENTER" || k == "RETURN") return GameKey::Key_Return;
    if (k == "ESC" || k == "ESCAPE") return GameKey::Key_Escape;
    if (k == "TAB") return GameKey::Key_Tab;
    if (k == "BACKSPACE") return GameKey::Key_Backspace;
    if (k == "SHIFT") return GameKey::Key_Shift;
    if (k == "CTRL" || k == "CONTROL") return GameKey::Key_Control;
    if (k == "ALT") return GameKey::Key_Alt;
    if (k == "UP") return GameKey::Key_Up;
    if (k == "DOWN") return GameKey::Key_Down;
    if (k == "LEFT") return GameKey::Key_Left;
    if (k == "RIGHT") return GameKey::Key_Right;

    if (k.size() >= 2 && k.size() <= 3 && k[0] == 'F') {
        int num = 0;
        bool ok = true;
        for (size_t i = 1; i < k.size(); ++i) {
            if (k[i] >= '0' && k[i] <= '9') num = num * 10 + (k[i] - '0');
            else { ok = false; break; }
        }
        if (ok && num >= 1 && num <= 12) {
            return GameKey::Key_F1 + num - 1;
        }
    }

    if (k.size() == 1) {
        char c = k[0];
        if (c >= 'A' && c <= 'Z') return GameKey::Key_A + (c - 'A');
        if (c >= '0' && c <= '9') return GameKey::Key_0 + (c - '0');

        switch (c) {
            case '`': return GameKey::Key_QuoteLeft;
            case '~': return GameKey::Key_AsciiTilde;
            case '-': return GameKey::Key_Minus;
            case '=': return GameKey::Key_Equal;
            case '[': return GameKey::Key_BracketLeft;
            case ']': return GameKey::Key_BracketRight;
            case '\\': return GameKey::Key_Backslash;
            case ';': return GameKey::Key_Semicolon;
            case '\'': return GameKey::Key_Apostrophe;
            case ',': return GameKey::Key_Comma;
            case '.': return GameKey::Key_Period;
            case '/': return GameKey::Key_Slash;
        }
    }

    return 0;
}

int SandboxScriptApi::getAndroidKeyCode(const std::string& keyName)
{
    std::string k;
    k.reserve(keyName.size());
    for (char c : keyName) k += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (k == "W") return AKEYCODE_W;
    if (k == "A") return AKEYCODE_A;
    if (k == "S") return AKEYCODE_S;
    if (k == "D") return AKEYCODE_D;
    if (k == "SPACE") return AKEYCODE_SPACE;
    if (k == "ENTER") return AKEYCODE_ENTER;
    if (k == "ESC") return AKEYCODE_ESCAPE;
    if (k == "BACK") return AKEYCODE_BACK;
    if (k == "HOME") return AKEYCODE_HOME;
    if (k == "MENU") return AKEYCODE_MENU;

    if (k.size() == 1) {
        char c = k[0];
        if (c >= '0' && c <= '9') return AKEYCODE_0 + (c - '0');
        if (c >= 'A' && c <= 'Z') return AKEYCODE_A + (c - 'A');
    }
    return AKEYCODE_UNKNOWN;
}

FindImageResult SandboxScriptApi::findImage(const std::string& imageName,
                                         double x1, double y1,
                                         double x2, double y2,
                                         double threshold)
{
    FindImageResult result;

    if (!m_isPress || isInterrupted()) return result;

#ifdef ENABLE_IMAGE_MATCHING
    std::string cacheKey = ThreadLocalFindResultCache::makeKey(imageName, x1, y1, x2, y2, threshold);
    if (t_findResultCache.tryGet(cacheKey, result)) {
        return result;
    }

    const GrayFrame& grayFrame = t_grayFrameCache.getFrame();
    if (!grayFrame.isValid()) {
        cv::Mat currentFrame = t_frameCache.getFrame();
        if (currentFrame.empty()) {
            LOGW() << "[Sandbox findImage] Frame is empty, stopping sandbox";
            if (m_sandbox) m_sandbox->stop();
            return result;
        }

        cv::Mat templateImage = t_templateCache.getTemplate(imageName);
        if (templateImage.empty()) {
            LOGW() << "[Sandbox findImage] Failed to load template:" << imageName;
            return result;
        }

        LOG_I_ONCE("[Sandbox findImage] fullframe path: frame=%dx%d ch=%d, tpl=%dx%d ch=%d, name=%s, threshold=%.2f",
                 currentFrame.cols, currentFrame.rows, currentFrame.channels(),
                 templateImage.cols, templateImage.rows, templateImage.channels(),
                 imageName.c_str(), threshold);

        RectF searchRegion(x1, y1, x2 - x1, y2 - y1);
        ImageMatcher matcher;
        ImageMatchResult matchResult = matcher.findTemplate(currentFrame, templateImage, threshold, searchRegion);

        result.found = matchResult.found;
        result.x = std::round(matchResult.x * 10000.0) / 10000.0;
        result.y = std::round(matchResult.y * 10000.0) / 10000.0;
        result.confidence = std::round(matchResult.confidence * 10000.0) / 10000.0;

        t_findResultCache.put(cacheKey, result);
        return result;
    }

    cv::Mat templateImage = t_templateCache.getTemplate(imageName);
    if (templateImage.empty()) {
        LOGW() << "[Sandbox findImage] Failed to load template:" << imageName;
        return result;
    }

    LOG_I_ONCE("[Sandbox findImage] gray path: grayFrame=%dx%d, tpl=%dx%d ch=%d, name=%s, threshold=%.2f",
             grayFrame.width, grayFrame.height,
             templateImage.cols, templateImage.rows, templateImage.channels(),
             imageName.c_str(), threshold);

    RectF searchRegion(x1, y1, x2 - x1, y2 - y1);

    ImageMatcher matcher;
    ImageMatchResult matchResult = matcher.findTemplateFromGray(grayFrame, templateImage, threshold, searchRegion);

    result.found = matchResult.found;
    result.x = std::round(matchResult.x * 10000.0) / 10000.0;
    result.y = std::round(matchResult.y * 10000.0) / 10000.0;
    result.confidence = std::round(matchResult.confidence * 10000.0) / 10000.0;

    t_findResultCache.put(cacheKey, result);
#else
    (void)imageName;
    (void)x1; (void)y1;
    (void)x2; (void)y2;
    (void)threshold;
    LOGW() << "[Sandbox findImage] Image matching is disabled (OpenCV not available)";
#endif

    return result;
}

FindImageResult SandboxScriptApi::findImageByRegion(const std::string& imageName,
                                                 int regionId,
                                                 double threshold)
{
    SelectionRegion region;
    if (!SelectionRegionManager::instance().findById(regionId, region)) {
        LOGW() << "[Sandbox findImageByRegion] Region not found, id:" << regionId;
        return FindImageResult();
    }
    return findImage(imageName, region.x0, region.y0, region.x1, region.y1, threshold);
}

ButtonPosResult SandboxScriptApi::getbuttonpos(int buttonId)
{
    ButtonPosResult result;

    if (isInterrupted()) return result;

    ScriptButton btn;
    if (ScriptButtonManager::instance().findById(buttonId, btn)) {
        result.x = btn.x;
        result.y = btn.y;
        result.valid = true;
        result.name = btn.name.toStdString();
    } else {
        LOGW() << "[Sandbox getbuttonpos] Button not found, id:" << buttonId;
    }

    return result;
}

void SandboxScriptApi::swipeById(int swipeId, int durationMs, int steps)
{
    if (!m_isPress) return;
    if (isInterrupted()) return;

    ScriptSwipe swp;
    if (!ScriptSwipeManager::instance().findById(swipeId, swp)) {
        LOGW() << "[Sandbox swipeById] Swipe not found, id:" << swipeId;
        return;
    }

    // 委托给现有的 slide 方法
    slide(swp.x0, swp.y0, swp.x1, swp.y1, durationMs, steps);
}
