#ifndef RECOILASSISTHANDLER_H
#define RECOILASSISTHANDLER_H

#include "IInputHandler.h"
#include "keymap.h"  // 需要 KeyMap::KeyMapNode 完整定义
#include "GameTypes.h"
#include "NativeTimer.h"

#include <cstdint>
#include <random>

class Controller;
class SessionContext;

/**
 * @brief 辅助压枪处理器 / Recoil Assist Handler（引擎原生实现，不依赖任何脚本）
 *
 * 按住键位映射中「辅助压枪」节点的热键时：
 *   1. 在节点位置（= 玩家拖放到的游戏开火按钮位置）按住一个触点，等价于按住开火键；
 *   2. 在可配置的拖拽起点落下第二个触点，并以 ~1/recoilStepIntervalMs 的周期
 *      持续向下拖动它，从而把被后坐力顶上去的准星压回来；
 *   3. 松开热键立即抬起两个触点。
 *
 * 与脚本实现的区别：
 *   - 全部逻辑在**主线程**由 NativeTimer 驱动，不经过 QuickJS 沙箱线程，
 *     因此不存在跨线程共享状态与看门狗超时问题；
 *   - 松键由引擎事件直接触发，不需要轮询按键状态。
 *
 * 所有调节参数都在 ConfigCenter（user/recoil* 系列），可在 UI 里可视化调节。
 */
class RecoilAssistHandler : public IInputHandler
{
public:
    RecoilAssistHandler();
    ~RecoilAssistHandler() override;

    void init(Controller* controller, SessionContext* context) override;

    // IInputHandler interface
    bool handleKeyEvent(const InputEvent& event, const Size& frameSize, const Size& showSize) override;
    bool handleMouseEvent(const InputEvent& event, const Size& frameSize, const Size& showSize) override;
    bool handleWheelEvent(const InputEvent& event, const Size& frameSize, const Size& showSize) override;
    void onFocusLost() override;
    void reset() override;
    int priority() const override { return 65; }  // 低于小眼睛(70)，高于视角控制
    std::string name() const override { return "RecoilAssistHandler"; }

    // 设置 KeyMap 引用
    void setKeyMap(KeyMap* keyMap) { m_keyMap = keyMap; }

    // 是否正在压枪（供其它模块查询）
    bool isActive() const { return m_state.active; }

    // 处理键盘事件（直接接收 node，由 InputDispatcher 调用）
    // 使用传入的 node，避免重新查找导致的修饰键丢失问题
    void processKeyEvent(const KeyMap::KeyMapNode& node, const InputEvent& event,
                         const Size& frameSize, const Size& showSize);

private:
    // 注入一个触摸事件（栈缓冲序列化，无堆分配）
    void sendTouch(uint32_t seqId, uint8_t action, const PointF& pos);

    void startRecoil(const KeyMap::KeyMapNode& node);
    void stopRecoil(const char* reason);
    void onTick();
    void onReleaseGraceExpired();

    double randUnit();  // [0, 1)

    struct
    {
        bool active = false;
        uint32_t fireSeqId = 0;   // 开火按钮触点（0 = 无）
        uint32_t dragSeqId = 0;   // 压枪拖拽触点（0 = 未注入/已停手）
        PointF firePos;           // 开火按钮位置（归一化）
        PointF dragStart;         // 拖拽起点（归一化）
        PointF currentPos;        // 拖拽当前点（归一化）
        double travelled = 0.0;   // 已累计路径长度（归一化）
        double elapsedMs = 0.0;   // 本次按压已持续时间
        double curvePhase = 0.0;  // 曲线相位
        uint64_t lastTickUs = 0;  // 上一次 tick 的 steady_clock 时间戳（微秒）
    } m_state;

    NativeTimer m_tickTimer;
    NativeTimer m_releaseGraceTimer;  // 松键宽限：吸收「按下期间瞬时释放」抖动
    bool m_pendingStop = false;       // 已收到释放、正在等宽限窗口到期
    KeyMap* m_keyMap = nullptr;
    std::mt19937 m_rng;
};

#endif // RECOILASSISTHANDLER_H
