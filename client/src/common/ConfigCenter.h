#ifndef CONFIGCENTER_H
#define CONFIGCENTER_H

#include <mutex>
#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <functional>
#include "GameTypes.h"

class IniConfig;

namespace qsc {

// 配置变更回调函数类型 / Configuration change callback function type
using ConfigChangeListener = std::function<void(const std::string& key, const ConfigValue& oldValue, const ConfigValue& newValue)>;

/**
 * @brief 配置中心 - 单例模式 / Configuration Center - Singleton Pattern
 *
 * 统一管理全局配置和用户配置，支持：
 * Centralized management of global and user config, supporting:
 * - 分层配置（默认值 -> 全局配置 -> 用户配置 -> 运行时覆盖）/ Layered config (default -> global -> user -> runtime override)
 * - 配置变更监听 / Config change listeners
 * - 依赖注入（用于测试）/ Dependency injection (for testing)
 */
class ConfigCenter
{
public:
    static ConfigCenter& instance();

    // 依赖注入支持
    static void injectInstance(ConfigCenter* instance);
    static void resetInstance();

    virtual ~ConfigCenter();

    // 初始化
    bool initialize(const std::string& configPath = std::string(), const std::string& userDataPath = std::string());
    bool isInitialized() const;

    // 通用配置访问
    ConfigValue get(const std::string& key, const ConfigValue& defaultValue = ConfigValue()) const;
    void set(const std::string& key, const ConfigValue& value, bool persistent = true);
    void setOverride(const std::string& key, const ConfigValue& value);
    void removeOverride(const std::string& key);
    bool contains(const std::string& key) const;
    void remove(const std::string& key);

    // 模板方法 — uses configval helpers / std::get_if
    template<typename T>
    T get(const std::string& key, const T& defaultValue = T()) const {
        ConfigValue v = get(key, ConfigValue(defaultValue));
        if (auto* p = std::get_if<T>(&v)) return *p;
        return defaultValue;
    }

    // Specializations for type-converting access are defined after the class
    // (out-of-line, as ISO C++ requires: in-class explicit specialization is
    //  rejected by GCC/Clang and only tolerated by MSVC)

    // String accessor helper (std::string specialization)
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;

    // --- 全局配置快捷方法 ---
    std::string language() const;
    std::string title() const;
    int maxFps() const;
    int desktopOpenGL() const;
    bool useSkin() const;
    bool renderExpiredFrames() const;
    std::string serverPath() const;
    std::string adbPath() const;
    std::string logLevel() const;
    std::string codecOptions() const;
    std::string codecName() const;

    // --- 用户配置快捷方法 ---
    std::string recordPath() const;
    void setRecordPath(const std::string& path);

    uint32_t bitRate() const;
    void setBitRate(uint32_t rate);

    int maxSizeIndex() const;
    void setMaxSizeIndex(int index);

    bool reverseConnect() const;
    void setReverseConnect(bool enable);

    bool showFPS() const;
    void setShowFPS(bool show);

    bool windowOnTop() const;
    void setWindowOnTop(bool onTop);

    bool autoOffScreen() const;
    void setAutoOffScreen(bool enable);

    bool framelessWindow() const;
    void setFramelessWindow(bool enable);

    bool keepAlive() const;
    void setKeepAlive(bool enable);

    bool simpleMode() const;
    void setSimpleMode(bool enable);

    bool showToolbar() const;
    void setShowToolbar(bool show);

    // 随机偏移范围 (0~100，对应 0~50像素)
    int randomOffset() const;
    void setRandomOffset(int value);

    // 轮盘平滑度 (0~100，0=无平滑，100=高平滑)
    int steerWheelSmooth() const;
    void setSteerWheelSmooth(int value);

    // 轮盘拟人曲线幅度 (0~100，0=无曲线，100=最大幅度)
    int steerWheelCurve() const;
    void setSteerWheelCurve(int value);

    // 轮盘拟人强度 (0~100，0=纯映射，100=最强：持续微颤 + 钟形加减速 + 预测性过冲)
    // 覆盖 randomOffset / steerWheelSmooth / steerWheelCurve 三层拟人化，为总开关
    int steerWheelHumanize() const;
    void setSteerWheelHumanize(int value);

    // 滑动轨迹曲线幅度 (0~100，0=直线，100=最大弧度)
    int slideCurve() const;
    void setSlideCurve(int value);

    // ============ 辅助压枪（原生实现，无脚本） ============
    // 总开关：关闭后压枪节点只按住开火按钮、不注入补偿拖拽（可用于对照排查）
    bool recoilEnabled() const;
    void setRecoilEnabled(bool enabled);

    // 强度倍率 (0~300 %)，同时缩放垂直与水平补偿
    int recoilStrength() const;
    void setRecoilStrength(int value);

    // 垂直压枪速率 (-300~300，单位 0.01 屏高/秒；正=向下拖压枪，负=方向上拖，供视角反向的游戏使用)
    int recoilVerticalRate() const;
    void setRecoilVerticalRate(int value);

    // 水平漂移速率 (-100~100，单位 0.01 屏高/秒；负=向左)
    int recoilHorizontalRate() const;
    void setRecoilHorizontalRate(int value);

    // 拖拽起点 (0~100 %)，即补偿拖拽的落指位置
    int recoilDragX() const;
    void setRecoilDragX(int value);
    int recoilDragY() const;
    void setRecoilDragY(int value);

    // 注入步进间隔 (2~50 ms)，越小越平滑、注入频率越高
    int recoilStepIntervalMs() const;
    void setRecoilStepIntervalMs(int value);

    // 步进间隔随机化幅度 (0~100 %)，打散固定注入频率
    int recoilRandomizeStep() const;
    void setRecoilRandomizeStep(int value);

    // 拟人抖动强度 (0~100)
    int recoilJitter() const;
    void setRecoilJitter(int value);

    // 轨迹曲线强度 (0~100，0=直线)
    int recoilCurve() const;
    void setRecoilCurve(int value);

    // 单次按压内的最大累计位移 (1~100 %)
    int recoilMaxDistance() const;
    void setRecoilMaxDistance(int value);

    // 起手渐入：时长 (0~3000 ms) 与起始力度百分比 (0~100)
    int recoilRampUpMs() const;
    void setRecoilRampUpMs(int value);
    int recoilRampUpStartRatio() const;
    void setRecoilRampUpStartRatio(int value);

    // 后段渐出：时长 (0~3000 ms，0=不渐出) 与末尾残留百分比 (0~100)
    int recoilDecayMs() const;
    void setRecoilDecayMs(int value);
    int recoilDecayEndRatio() const;
    void setRecoilDecayEndRatio(int value);

    // 松键宽限 (0~500 ms)：按住期间若收到瞬时「释放→按下」抖动，在此窗口内不抬指，
    // 避免游戏开火按钮被反复抬起导致射击断续。0 = 关闭（旧行为）。
    int recoilReleaseGraceMs() const;
    void setRecoilReleaseGraceMs(int value);

    // 键位提示层透明度 (0~100，0=全透明，100=不透明)
    int keyMapOverlayOpacity() const;
    void setKeyMapOverlayOpacity(int value);

    // 键位提示层是否显示
    bool keyMapOverlayVisible() const;
    void setKeyMapOverlayVisible(bool visible);

    // 脚本弹窗透明度 (0~100，0=全透明，100=不透明)
    int scriptTipOpacity() const;
    void setScriptTipOpacity(int value);

    // 视频传输状态 (true=正在传输, false=暂停)
    bool videoStreaming() const;
    void setVideoStreaming(bool streaming);

    // 息屏状态
    bool screenOff() const;
    void setScreenOff(bool off);

    // 锐化强度 (0~100, 0=关闭, 100=最大)
    int sharpenStrength() const;
    void setSharpenStrength(int value);

    // --- 设备专属配置 ---
    std::string nickName(const std::string& serial) const;
    void setNickName(const std::string& serial, const std::string& name);

    Rect windowRect(const std::string& serial) const;
    void setWindowRect(const std::string& serial, const Rect& rect);

    std::string keyMap(const std::string& serial) const;
    void setKeyMap(const std::string& serial, const std::string& keyMapFile);

    // --- 配置变更监听 ---
    int addChangeListener(const std::string& key, ConfigChangeListener listener);
    void removeChangeListener(int listenerId);

    // --- 配置导入导出 ---
    std::map<std::string, ConfigValue> exportUserConfig() const;
    void importUserConfig(const std::map<std::string, ConfigValue>& config);
    void resetToDefaults();

protected:
    ConfigCenter();

private:
    void registerDefaults();
    std::string deviceKey(const std::string& serial, const std::string& key) const;
    void notifyChange(const std::string& key, const ConfigValue& oldValue, const ConfigValue& newValue);

private:
    static ConfigCenter* s_instance;
    static ConfigCenter* s_injectedInstance;

    IniConfig* m_globalConfig = nullptr;
    IniConfig* m_userConfig = nullptr;
    mutable std::recursive_mutex m_mutex;
    bool m_initialized = false;

    std::map<std::string, ConfigValue> m_defaults;
    std::map<std::string, ConfigValue> m_overrides;

    struct ListenerEntry {
        int id;
        std::string pattern;
        ConfigChangeListener listener;
    };
    std::vector<ListenerEntry> m_listeners;
    int m_nextListenerId = 1;
};

// --- type-converting explicit specializations of the get<T> template ---
// Defined out-of-line for ISO C++ conformance: an in-class explicit
// specialization is rejected by GCC/Clang (and only tolerated by MSVC).
// Must remain visible to every translation unit that uses these types.
template<>
inline bool ConfigCenter::get<bool>(const std::string& key, const bool& defaultValue) const {
    return configval::toBool(get(key, ConfigValue(defaultValue)), defaultValue);
}

template<>
inline int ConfigCenter::get<int>(const std::string& key, const int& defaultValue) const {
    return configval::toInt(get(key, ConfigValue(defaultValue)), defaultValue);
}

template<>
inline uint32_t ConfigCenter::get<uint32_t>(const std::string& key, const uint32_t& defaultValue) const {
    return configval::toUInt(get(key, ConfigValue(defaultValue)), defaultValue);
}

template<>
inline double ConfigCenter::get<double>(const std::string& key, const double& defaultValue) const {
    return configval::toDouble(get(key, ConfigValue(defaultValue)), defaultValue);
}

} // namespace qsc

#endif // CONFIGCENTER_H
