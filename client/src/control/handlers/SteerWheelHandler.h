#ifndef STEERWHEELHANDLER_H
#define STEERWHEELHANDLER_H

#include "IInputHandler.h"
#include "NativeTimer.h"
#include "keymap.h"
#include <cstdint>
#include <deque>

/**
 * @brief 方向盘（WASD轮盘）处理器 / Steer Wheel (WASD) Handler
 *
 * 职责 / Responsibilities：
 * - 处理 WASD 按键输入 / Handle WASD key input
 * - 计算轮盘触摸位置 / Calculate wheel touch position
 * - 发送 FastMsg 触摸事件 / Send FastMsg touch events
 * - 拟人化 / Humanization：
 *   · 每次按下采样一个圆盘分布的稳定随机零位（randomOffset）
 *   · 按住期间以 ~100Hz 输出持续微颤（OU 噪声，生理性震颤量级）
 *   · 状态切换用二阶欠阻尼速度剖面（钟形加减速 + 轻微过冲回正）
 *   · 直接模式下仍有 2~4 点 / 10~20ms 的短过渡，避免单帧瞬移
 *   · 拟人强度为 0 时全部关闭，行为与未启用时一致
 */
class SteerWheelHandler : public IInputHandler
{
public:
    explicit SteerWheelHandler();
    ~SteerWheelHandler() override;

    void init(Controller* controller, SessionContext* context) override;
    bool handleKeyEvent(const InputEvent& event,
                        const Size& frameSize,
                        const Size& showSize) override;
    void onFocusLost() override;
    void reset() override;
    std::string name() const override { return "SteerWheelHandler"; }
    int priority() const override { return 20; }  // 高优先级

    // ========== 配置方法 ==========

    /**
     * @brief 设置 KeyMap 引用（用于获取轮盘配置）
     */
    void setKeyMap(KeyMap* keyMap) { m_keyMap = keyMap; }

    /**
     * @brief 设置轮盘系数（脚本 API 调用）
     */
    void setCoefficient(double up, double down, double left, double right);

    /**
     * @brief 重置轮盘系数为默认值
     */
    void resetCoefficient();

    /**
     * @brief 重置轮盘状态（场景切换时调用）
     */
    void resetWheel();

private:
    void onSteerWheelTimer();
    void onHumanizeTimer();
    void onResetRepressTimer();
    void onJitterTimer();

private:
    void processSteerWheel(const KeyMap::KeyMapNode& node, const InputEvent& event);
    void executeMove(const KeyMap::KeyMapNode& node);

    /// 当前按键组合对应的基础偏移（不含任何拟人化修正）
    PointF computeBaseOffset(const KeyMap::KeyMapNode& node) const;
    /// 应用拟人化修正：长度系数 + 对角方向的角度抖动
    PointF applyHumanize(const PointF& baseOffset, int pressedNum) const;
    /// 直接模式下的拟人短过渡：2~4 个中间点，总时长 10~20ms。
    /// 返回 false 表示未开启拟人化（未生成中间点），调用方按原逻辑直接下发。
    bool buildHumanRamp(const PointF& start, const PointF& end,
                        std::deque<PointF>& queuePos, uint32_t& stepTimerMs) const;

    void sendFastTouch(uint8_t action, const PointF& pos);
    void getDelayQueue(const PointF& start, const PointF& end,
                       double distanceStep, double posStep,
                       uint32_t lowestTimer, uint32_t highestTimer,
                       double speedScale,
                       std::deque<PointF>& queuePos, uint32_t& stepTimerMs);

    KeyMap* m_keyMap = nullptr;
    Size m_frameSize;
    Size m_showSize;

    // 轮盘状态
    struct {
        int touchKey = GameKey::Key_unknown;
        bool pressedUp = false;
        bool pressedDown = false;
        bool pressedLeft = false;
        bool pressedRight = false;
        uint32_t fastTouchSeqId = 0;

        // 拟人化参数（宏观层：2~8 秒一次的慢漂移）
        double currentAngleOffset = 0.0;
        double currentLengthFactor = 1.0;
        double targetAngleOffset = 0.0;
        double targetLengthFactor = 1.0;
        NativeTimer humanizeTimer;
        int lastPressedState = 0;

        // 拟人化参数（高频层：~100Hz 持续微颤，一阶 OU 噪声）
        NativeTimer jitterTimer;
        int jitterIntervalMs = 10;          // ~100Hz，落在真人触控采样 60~120Hz 区间
        uint64_t lastJitterTickMs = 0;      // 墙钟自节流，主线程拥塞时丢弃过期 tick
        double noiseAngleFast = 0.0;        // τ=50ms 快分量（覆盖 8~12Hz 震颤）
        double noiseAngleSlow = 0.0;        // τ=700ms 慢分量（按住时方向缓慢微调）
        double noiseRadiusFast = 0.0;
        double noiseRadiusSlow = 0.0;
        double noiseAngleApplied = 0.0;     // 已下发的噪声累计值（增量叠加用）
        double noiseRadiusApplied = 0.0;

        // 每次按下的稳定随机零位 + 整像素量化网格
        PointF pressBasePos;
        bool pressBaseValid = false;
        double quantW = 0.0;                // 0 = 不量化
        double quantH = 0.0;

        // 延迟数据
        struct {
            PointF currentPos;
            NativeTimer timer;
            std::deque<PointF> queuePos;
            uint32_t stepTimerMs = 0;
            int pressedNum = 0;
        } delayData;

        // resetWheel 延迟 re-trigger
        NativeTimer resetRepressTimer;
        bool waitingForResetRepress = false;
    } m_state;
};

#endif // STEERWHEELHANDLER_H
