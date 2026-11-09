#include "Output.h"

#include <algorithm>
#include <cassert>
#include "stream/Stream.h"
#include "processing/ChannelsOutputEnable.h"
#include "processing/ChipClockRateConvert.h"
#include "processing/ChannelsLayoutChange.h"
#include "processing/AY8930EnvelopeFix.h"

namespace
{
    // The player thread writes while the GUI copies, so do not take the
    // oldest quarter of the history: the player writes there next.
    template<typename T, size_t N>
    void TakeHistory(const std::array<T, N>& history, uint32_t written, uint32_t& count, std::vector<T>& items)
    {
        const uint32_t maxCount = uint32_t(N * 3 / 4);
        if (written - count > maxCount) count = (written - maxCount);

        items.clear();
        for (; count != written; ++count)
        {
            items.push_back(history[count % N]);
        }
    }
}

Output::Output()
    : m_alive(false)
    , m_processing("output_processing")
    , m_heard{}
    , m_levels{}
    , m_bands{}
    , m_measuredCount(0)
    , m_framePeriod{}
    , m_latency{}
    , m_position(0)
{
}

Output::~Output()
{
}

bool Output::Open()
{
    m_alive = DeviceOpen();
    return m_alive;
}

bool Output::Init(const Stream& stream)
{
    if (m_alive)
    {
        // device may override the destination chip setup
        m_dchip = stream.dchip;
        m_alive &= DeviceInit(stream, m_dchip);
        m_processing.Clear();

        if (m_alive)
        {
            // check if the destination chip setup is correct
            m_alive &= m_dchip.clockKnown;
            m_alive &= m_dchip.outputKnown;
            if (m_dchip.count == 2) 
                m_alive &= m_dchip.second.modelKnown;
            if (m_dchip.output == Chip::Output::Stereo)
                m_alive &= m_dchip.stereoKnown;
            assert(m_alive);

            if (m_alive)
            {
                // configure frame processing chain
                m_processing
                    .Append<ChannelsOutputEnable>(m_dchip)
                    .Append<ChipClockRateConvert>(stream.schip, m_dchip)
                    .Append<ChannelsLayoutChange>(m_dchip)
                    .Append<AY8930EnvelopeFix>(m_dchip);

                m_meter.Init(m_dchip, stream.play.frameRate);

                using Seconds = std::chrono::duration<double>;
                m_framePeriod = std::chrono::duration_cast<Clock::duration>(Seconds(1 / stream.play.frameRate));
                m_latency = std::chrono::duration_cast<Clock::duration>(Seconds(GetDeviceLatency()));
                m_heardTime = {};
                m_position = 0;
                m_heard.fill({}); // the last stream is heard, at position 0
            }
        }
    }
    return m_alive;
}

bool Output::Write(const Frame& frame, int position)
{
    if (m_alive)
    {
        // processing before output
        const Frame& pframe = m_processing.Execute(frame);

        // output to chip(s)
        m_data[0].clear();
        m_data[1].clear();
        for (int chip = 0; chip < m_dchip.count; ++chip)
        {
            bool switchBanks = false;
            ChipData& data = m_data[chip];
            if (m_dchip.hasExpMode(chip) && pframe[chip].IsExpMode())
            {
                for (Register reg = Register::BankB_Fst; reg < Register::BankB_Lst; ++reg)
                {
                    if (pframe[chip].IsChanged(reg))
                    {
                        if (!switchBanks)
                        {
                            data.emplace_back(Register::Mode_Bank, pframe[chip].GetData(Register::Mode_Bank) | 0x10);
                            switchBanks = true;
                        }
                        data.emplace_back(reg & 0x0F, pframe[chip].GetData(reg));
                    }
                }
                if (switchBanks)
                {
                    data.emplace_back(Register::Mode_Bank, pframe[chip].GetData(Register::Mode_Bank));
                }
            }

            for (Register reg = Register::BankA_Fst; reg <= Register::BankA_Lst; ++reg)
            {
                if (pframe[chip].IsChanged(reg))
                {
                    if (switchBanks && reg == Register::Mode_Bank) continue;
                    data.emplace_back(reg & 0x0F, pframe[chip].GetData(reg));
                }
            }
        }
        m_alive &= DeviceWrite(m_data);

        // The frame sounds after the latency of the device. The writes have
        // the jitter of the device, so the frame times step by the frame
        // period and follow the writes slowly. After a pause they jump to
        // the write. They never go back, the GUI takes them in order.
        const Clock::time_point time = (Clock::now() + m_latency);
        const Clock::time_point next = (m_heardTime + m_framePeriod);
        const Clock::duration error = (time - next);
        m_heardTime = (error > 2 * m_framePeriod ? time : next + std::max(error, -m_framePeriod) / 16);
        if (position >= 0) m_position = position;

        // measure the frame and publish the result for the GUI
        uint32_t count = m_measuredCount.load(std::memory_order_relaxed);
        Levels& levels = m_levels[count % m_levels.size()];
        Bands& bands = m_bands[count % m_bands.size()];
        m_heard[count % m_heard.size()] = { m_heardTime, m_position };
        m_meter.Measure(pframe, levels.L, levels.R);
        m_meter.GetBands(bands.levels, bands.sounds);
        m_measuredCount.store(count + 1, std::memory_order_release);
    }
    return m_alive;
}

void Output::Close()
{
    DeviceClose();
    m_alive = false;
}

const Output::Enables& Output::GetEnables() const
{
    return static_cast<ChannelsOutputEnable*>(m_processing.GetProcessor(0))->GetEnables();
}

Output::Enables& Output::GetEnables()
{
    return static_cast<ChannelsOutputEnable*>(m_processing.GetProcessor(0))->GetEnables();
}

uint32_t Output::GetHeardCount() const
{
    const Clock::time_point now = Clock::now();
    uint32_t count = m_measuredCount.load(std::memory_order_acquire);
    const uint32_t oldest = (count - uint32_t(m_heard.size() * 3 / 4));
    while (count != oldest && m_heard[(count - 1) % m_heard.size()].time > now) --count;
    return count;
}

int Output::GetPosition() const
{
    return m_heard[(GetHeardCount() - 1) % m_heard.size()].position;
}

void Output::GetLevels(uint32_t& count, std::vector<Levels>& levels) const
{
    TakeHistory(m_levels, GetHeardCount(), count, levels);
}

void Output::GetBands(uint32_t& count, std::vector<Bands>& bands) const
{
    TakeHistory(m_bands, GetHeardCount(), count, bands);
}

std::string Output::toString() const
{
    return (m_dchip.toString() + " -> " + GetDeviceName());
}
