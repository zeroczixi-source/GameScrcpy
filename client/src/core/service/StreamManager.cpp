#include "StreamManager.h"
#include "ThreadDispatcher.h"
#include "demuxer.h"
#include "decoder.h"
#include "interfaces/IVideoChannel.h"
#include "impl/TcpVideoChannel.h"
#include "impl/KcpVideoChannel.h"

namespace qsc {
namespace core {

StreamManager::StreamManager()
{
    m_fpsTimer.setInterval(1000);
    m_fpsTimer.setCallback([this]() { pollDecoderFps(); });
}

StreamManager::~StreamManager()
{
    // 先作废生命周期令牌：已投递到主线程、尚未执行的回调会被丢弃，避免悬垂 this
    if (m_aliveToken) {
        m_aliveToken->store(false, std::memory_order_release);
    }
    stop();
}

void StreamManager::setVideoChannel(IVideoChannel* channel)
{
    m_videoChannel = channel;
}

void StreamManager::setDecoder(Decoder* decoder)
{
    m_decoder = decoder;
}

void StreamManager::setFrameCallback(FrameCallback callback)
{
    m_frameCallback = std::move(callback);
}

bool StreamManager::start(const Size& frameSize)
{
    if (m_running) {
        return true;
    }

    if (!m_videoChannel || !m_decoder) {
        return false;
    }

    m_frameSize = frameSize;

    // 创建 Demuxer
    m_demuxer = std::make_unique<Demuxer>();
    m_demuxer->setFrameSize(frameSize);

    // 根据通道类型安装对应的 socket
    if (auto* tcpChannel = dynamic_cast<TcpVideoChannel*>(m_videoChannel)) {
        m_demuxer->installVideoSocket(tcpChannel->socket());
    } else if (auto* kcpChannel = dynamic_cast<KcpVideoChannel*>(m_videoChannel)) {
        m_demuxer->installKcpVideoSocket(kcpChannel->socket());
    } else {
        return false;
    }

    // 连接信号
    // 注意：投递出去的回调可能在本对象析构之后才被主线程执行（退出流程/设备断开），
    // 因此捕获生命周期令牌，执行前先确认对象仍然存活。
    auto alive = m_aliveToken;
    m_demuxer->onStreamStop.connect([this, alive]() {
        dispatch::postToMain([this, alive]() {
            if (!alive || !alive->load(std::memory_order_acquire)) {
                return;  // 对象已析构，丢弃该回调
            }
            onDemuxerStopped();
        });
    });
    m_demuxer->getFrame.connect([this](AVPacket* packet) {
        onGetFrame(packet);
    });

    // 启动解码
    if (!m_demuxer->startDecode()) {
        m_demuxer.reset();
        return false;
    }

    m_running = true;

    // 启动 FPS 轮询定时器
    m_fpsTimer.start();

    // 发送解码器信息
    decoderInfo.fire(m_decoder->isHardwareAccelerated(), m_decoder->hwDecoderName());

    return true;
}

void StreamManager::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    m_fpsTimer.stop();

    if (m_demuxer) {
        m_demuxer->stopDecode();
        m_demuxer.reset();
    }
}

bool StreamManager::isRunning() const
{
    return m_running;
}

void StreamManager::screenshot(ScreenshotCallback callback)
{
    if (m_decoder && callback) {
        m_decoder->peekFrame(callback);
    }
}

void StreamManager::onDemuxerStopped()
{
    m_running = false;
    streamStopped.fire();
}

void StreamManager::onGetFrame(AVPacket* packet)
{
    if (m_decoder && packet) {
        m_decoder->push(packet);
    }
}

void StreamManager::pollDecoderFps()
{
    if (!m_decoder) return;
    uint32_t fps = m_decoder->pollFps();
    m_currentFps = fps;
    fpsUpdated.fire(fps);
}

} // namespace core
} // namespace qsc
