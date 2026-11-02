#include "Output.h"

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
    , m_levels{}
    , m_bands{}
    , m_measuredCount(0)
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
            }
        }
    }
    return m_alive;
}

bool Output::Write(const Frame& frame)
{
    if (m_alive)
    {
        // processing before output
        const Frame& pframe = m_processing.Execute(frame);

        // output to chip(s)
        Data data(32);
        for (int chip = 0; chip < m_dchip.count; ++chip)
        {
            data.clear();
            bool switchBanks = false;

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

            if (!(m_alive &= DeviceWrite(chip, data))) break;
        }

        // measure the frame and publish the result for the GUI
        uint32_t count = m_measuredCount.load(std::memory_order_relaxed);
        Levels& levels = m_levels[count % m_levels.size()];
        Bands& bands = m_bands[count % m_bands.size()];
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

void Output::GetLevels(uint32_t& count, std::vector<Levels>& levels) const
{
    TakeHistory(m_levels, m_measuredCount.load(std::memory_order_acquire), count, levels);
}

void Output::GetBands(uint32_t& count, std::vector<Bands>& bands) const
{
    TakeHistory(m_bands, m_measuredCount.load(std::memory_order_acquire), count, bands);
}

std::string Output::toString() const
{
    return (m_dchip.toString() + " -> " + GetDeviceName());
}
