#include "ScriptWatchdog.h"
#include "ThreadDispatcher.h"
#define LOG_TAG "ScriptWatchdog"
#include "Logger.h"

ScriptWatchdog::ScriptWatchdog(int timeoutMs)
    : m_timeoutMs(timeoutMs)
{
    m_timer.setSingleShot(true);
    m_timer.setCallback([this]() { onTimeout(); });

    m_hardTimer.setSingleShot(true);
    m_hardTimer.setCallback([this]() { onHardTimeout(); });
}

ScriptWatchdog::~ScriptWatchdog()
{
    // 先作废存活标志：feed() 已投递但尚未执行的主线程回调会被丢弃，
    // 避免它们在本对象析构后仍读 m_running / m_timedOut / 调 m_timer.start()
    m_alive->store(false, std::memory_order_release);
    stop();
}

void ScriptWatchdog::start()
{
    if (m_running.exchange(true)) {
        return;  // 已经在运行
    }

    m_timedOut.store(false);
    m_timer.start(m_timeoutMs);
}

void ScriptWatchdog::stop()
{
    m_running.store(false);
    m_timer.stop();
    m_hardTimer.stop();
}

void ScriptWatchdog::feed()
{
    // 喂狗：重置超时计时器
    // 由于可能从工作线程调用，使用 dispatch::postToMain 确保在主线程执行
    if (m_running.load() && !m_timedOut.load()) {
        // 内层必须重新判活：lambda 被执行时本对象可能已经析构
        dispatch::postToMain([this, alive = m_alive]() {
            if (!alive->load(std::memory_order_acquire)) return;
            if (m_running.load() && !m_timedOut.load()) {
                m_timer.start(m_timeoutMs);
            }
        });
    }
}

void ScriptWatchdog::onTimeout()
{
    if (!m_running.load()) {
        return;
    }

    m_timedOut.store(true);
    LOGW() << "[ScriptWatchdog] Soft timeout triggered, attempting graceful interrupt...";

    // 发送软超时信号
    softTimeout.fire();

    // 启动硬超时定时器
    m_hardTimer.start(m_hardTimeoutMs);
}

void ScriptWatchdog::onHardTimeout()
{
    if (!m_running.load()) {
        return;
    }

    LOGE() << "[ScriptWatchdog] Hard timeout triggered, forcing termination!";

    // 发送强制终止信号
    hardTimeout.fire();
}
