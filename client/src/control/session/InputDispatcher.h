#ifndef INPUT_DISPATCHER_H
#define INPUT_DISPATCHER_H

#include "GameTypes.h"
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <mutex>

#include "GameSignal.h"
#include "NativeTimer.h"
#include "InputEvent.h"
#include "keymap.h"
#include "input.h"
#include "keycodes.h"

class Controller;
class HandlerChain;
class SteerWheelHandler;
class ViewportHandler;
class FreeLookHandler;
class RecoilAssistHandler;
class CursorHandler;
class KeyboardHandler;
class ScriptBridge;

/**
 * @brief 输入事件分发器 / Input Event Dispatcher
 *
 * 负责 / Responsible for：
 * - 处理鼠标/键盘/滚轮事件 / Handling mouse/keyboard/wheel events
 * - 管理光标状态（捕获/释放）/ Managing cursor state (capture/release)
 * - 事件分发到 HandlerChain / Dispatching events to HandlerChain
 * - 处理特定类型的键位映射 / Processing specific key mapping types
 *
 * 从 SessionContext 拆分出来，专注于输入处理。
 * Split from SessionContext, focused on input processing.
 */
class InputDispatcher
{
public:
    explicit InputDispatcher(Controller* controller, KeyMap* keyMap);
    ~InputDispatcher();

    // ========== 尺寸设置 ==========

    void setFrameSize(const Size& size) { m_frameSize = size; }
    void setShowSize(const Size& size) { m_showSize = size; }
    void setMobileSize(const Size& size) { m_mobileSize = size; }
    Size frameSize() const { return m_frameSize; }
    Size showSize() const { return m_showSize; }
    Size mobileSize() const { return m_mobileSize; }

    void setDevicePixelRatio(double dpr) { m_devicePixelRatio = dpr; }
    double devicePixelRatio() const { return m_devicePixelRatio; }

    // ========== Handler 设置 ==========

    void setHandlerChain(HandlerChain* chain) { m_handlerChain = chain; }
    void setSteerWheelHandler(SteerWheelHandler* handler) { m_steerWheelHandler = handler; }
    void setViewportHandler(ViewportHandler* handler) { m_viewportHandler = handler; }
    void setFreeLookHandler(FreeLookHandler* handler) { m_freeLookHandler = handler; }
    void setRecoilAssistHandler(RecoilAssistHandler* handler) { m_recoilAssistHandler = handler; }
    void setCursorHandler(CursorHandler* handler) { m_cursorHandler = handler; }
    void setKeyboardHandler(KeyboardHandler* handler) { m_keyboardHandler = handler; }

    // ========== ScriptBridge 设置 ==========

    void setScriptBridge(ScriptBridge* bridge) { m_scriptBridge = bridge; }

    // ========== 光标状态 ==========

    bool isCursorCaptured() const { return m_cursorCaptured.load(); }
    bool toggleCursorCaptured();
    void setCursorCaptured(bool captured);

    // ========== 事件处理 ==========

    void mouseEvent(const InputEvent& from, const Size& frameSize, const Size& showSize);
    void wheelEvent(const InputEvent& from, const Size& frameSize, const Size& showSize);
    void keyEvent(const InputEvent& from, const Size& frameSize, const Size& showSize);

    // ========== 窗口焦点 ==========

    void onWindowFocusLost();

    // ========== 按键状态访问 ==========

    // 返回按键状态的【快照副本】（内部加锁）。
    // 注意：脚本工作线程会通过 mapi.getKeyState() 高频轮询按键状态，而主线程
    // 在鼠标/键盘/焦点事件中会写入或清空 m_keyStates。若此处直接交出内部 map
    // 的引用，两个线程就会并发读写同一个 unordered_map —— 插入触发 rehash 时
    // 桶数组会被搬移/释放，读线程的 find() 会访问已失效内存，属于数据竞争，
    // 表现为随机崩溃（且崩前常常先出现"按键状态读取错误"）。
    // 因此这里只返回加锁拷贝的快照，调用方拿到的是独立副本。
    std::unordered_map<int, bool> keyStates() const
    {
        std::lock_guard<std::mutex> lock(m_keyStatesMutex);
        return m_keyStates;
    }

    // ========== 按键转换 ==========

    static AndroidKeycode convertKeyCode(int key, uint32_t modifiers);

    // ========== 坐标转换 ==========

    PointF calcFrameAbsolutePos(PointF relativePos) const;
    PointF calcScreenAbsolutePos(PointF relativePos) const;

    Signal<bool> grabCursor;

private:
    void updateSize(const Size& frameSize, const Size& showSize);

    // 鼠标处理
    bool processMouseClick(const InputEvent& from);
    bool processMouseMove(const InputEvent& from);
    void processCursorMouse(const InputEvent& from);
    void moveCursorTo(const InputEvent& from, int localX, int localY);
    // 作废鼠标位移参考点：下一个移动事件只用于重建参考点，不产生位移
    void invalidateMouseOrigin();
    void mouseMoveStartTouch(const InputEvent& from);
    void mouseMoveStopTouch();
    void startMouseMoveTimer();
    void stopMouseMoveTimer();

    // 按键处理
    void processScript(const KeyMap::KeyMapNode& node, bool isPress);
    void processFreeLook(const KeyMap::KeyMapNode& node, const InputEvent& from);
    void processRecoilAssist(const KeyMap::KeyMapNode& node, const InputEvent& from);
    void processAndroidKey(AndroidKeycode androidKey, const InputEvent& from);

    // 按键发送
    void sendKeyEvent(AndroidKeyeventAction action, AndroidKeycode keyCode);

    // 触摸 ID 管理
    int attachTouchID(int key);
    void detachTouchID(int key);
    int getTouchID(int key);

private:
    Controller* m_controller = nullptr;
    KeyMap* m_keyMap = nullptr;

    Size m_frameSize;
    Size m_showSize;
    Size m_mobileSize;
    double m_devicePixelRatio = 1.0;  // DPR for logical→physical coordinate conversion

    // Handler 引用
    HandlerChain* m_handlerChain = nullptr;
    SteerWheelHandler* m_steerWheelHandler = nullptr;
    ViewportHandler* m_viewportHandler = nullptr;
    FreeLookHandler* m_freeLookHandler = nullptr;
    RecoilAssistHandler* m_recoilAssistHandler = nullptr;
    CursorHandler* m_cursorHandler = nullptr;
    KeyboardHandler* m_keyboardHandler = nullptr;

    // ScriptBridge 引用
    ScriptBridge* m_scriptBridge = nullptr;

    // 光标状态
    std::atomic<bool> m_cursorCaptured{false};
    bool m_cursorHidden = false;
    bool m_needBackMouseMove = false;
    bool m_processMouseMove = true;

    // 多点触控 ID
    static const int MULTI_TOUCH_MAX_NUM = 10;
    int m_multiTouchID[MULTI_TOUCH_MAX_NUM] = { 0 };

    // 鼠标位移参考点（"光标此刻在哪"）
    // 正常情况下恒为控件中心：每次处理完一次位移都会把光标钉回中心并把参考点同步过去。
    // 进入捕获 / 控件尺寸变化 / 失焦后参考点被置为无效，下一个移动事件只用来重建参考点，
    // 避免把"光标到控件中心"的整段距离当成一次位移（表现为进入视角模式瞬间视角猛跳）。
    struct {
        NativeTimer timer;
        PointF originPos;
        bool originValid = false;
    } m_ctrlMouseMove;
    // [MouseDiag] 大位移日志节流（steady_clock 毫秒）
    long long m_lastJumpLogMs = 0;

    // 按键状态（可能被脚本工作线程读取，所有访问必须持 m_keyStatesMutex）
    std::unordered_map<int, bool> m_keyStates;
    mutable std::mutex m_keyStatesMutex;
    bool m_modifierComboDetected = false;
    int m_lastModifierKey = 0;

    // 鼠标脚本按钮跟踪（用于跨模式释放）
    std::unordered_set<uint32_t> m_pressedScriptMouseButtons;
};

#endif // INPUT_DISPATCHER_H
