#include "Emulator.h"
#include <chrono>
#include "stream/Stream.h"

namespace
{
    // with the audio blocks, gives the delay of the sound after Write
    constexpr size_t k_queueSize = 2;
    constexpr int k_blocks = 4;
    constexpr int k_blockSamples = 366;

    // the sound device took no frame for this time: it is lost or stalled
    constexpr auto k_stallTime = std::chrono::seconds(2);
}

Emulator::Emulator()
    : m_active(false)
    , m_samplesPerFrame(0)
    , m_samplesToFrame(0)
{
}

Emulator::~Emulator()
{
    DeviceClose();
}

const std::string Emulator::GetDeviceName() const
{
    return "Emulator";
}

bool Emulator::DeviceOpen()
{
    return WaveAudio::Open(k_PCMSampleRate, 2, k_blocks, k_blockSamples);
}

double Emulator::GetDeviceLatency() const
{
    // After Write the frame waits for the frames in the queue; the block it
    // goes to waits for the other blocks queued in the device.
    const double samples = (k_queueSize * m_samplesPerFrame + (k_blocks - 0.5) * k_blockSamples);
    return (samples / k_PCMSampleRate);
}

bool Emulator::DeviceInit(const Stream& stream, Chip& dchip)
{
    WaveAudio::Pause();
    PCMOutput::PCMInit(dchip);
    m_queue.clear();
    m_active = true;
    m_samplesPerFrame = (double(k_PCMSampleRate) / stream.play.frameRate);
    m_samplesToFrame = 0;
    WaveAudio::Resume();
    return true;
}

bool Emulator::DeviceWrite(const FrameData& data)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    bool ready = m_queueNotFull.wait_for(lock, k_stallTime, [this] { return (m_queue.size() < k_queueSize || !m_active); });
    if (ready && m_active) m_queue.push_back(data);
    return (ready && m_active);
}

void Emulator::FillBuffer(std::vector<float>& buffer)
{
    const size_t samples = (buffer.size() / 2);
    for (size_t done = 0; done < samples;)
    {
        // at the time of the next frame write it to the chips; if there is
        // none yet (the playback starts, waits or stops), keep the sound
        if (m_samplesToFrame < 1)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_queue.empty())
            {
                lock.unlock();
                PCMOutput::PCMFill(&buffer[2 * done], samples - done);
                return;
            }
            PCMOutput::PCMWrite(m_queue.front());
            m_queue.pop_front();
            lock.unlock();

            m_queueNotFull.notify_one();
            m_samplesToFrame += m_samplesPerFrame;
        }

        // the fractional part of the samples goes to the next frame
        size_t count = size_t(m_samplesToFrame);
        if (count > samples - done) count = (samples - done);

        PCMOutput::PCMFill(&buffer[2 * done], count);
        m_samplesToFrame -= count;
        done += count;
    }
}

void Emulator::DeviceClose()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_active = false;
    }
    m_queueNotFull.notify_all();
    WaveAudio::Close();
}
