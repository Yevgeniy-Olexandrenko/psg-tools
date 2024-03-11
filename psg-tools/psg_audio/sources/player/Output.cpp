#include "Output.h"

#include <cassert>
#include "stream/Stream.h"
#include "processing/ChannelsOutputEnable.h"
#include "processing/ChipClockRateConvert.h"
#include "processing/ChannelsLayoutChange.h"
#include "processing/AY8930EnvelopeFix.h"

Output::Output()
    : m_alive(false)
    , m_processing("output")
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
            m_alive &= m_dchip.clockKnown();
            m_alive &= m_dchip.outputKnown();
            if (m_dchip.count() == 2) 
                m_alive &= m_dchip.second.modelKnown();
            if (m_dchip.output() == Chip::Output::Stereo)
                m_alive &= m_dchip.stereoKnown();
            assert(m_alive);

            if (m_alive)
            {
                // configure frame processing chain
                m_processing
                    .Append<ChannelsOutputEnable>(m_dchip)
                    .Append<ChipClockRateConvert>(stream.schip, m_dchip)
                    .Append<ChannelsLayoutChange>(m_dchip)
                    .Append<AY8930EnvelopeFix>(m_dchip);
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
        for (int chip = 0; chip < m_dchip.count(); ++chip)
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

void Output::GetLevels(float& L, float& C, float& R) const
{
    float a = ComputeChannelLevel(0, Frame::Channel::A);
    float b = ComputeChannelLevel(0, Frame::Channel::B);
    float c = ComputeChannelLevel(0, Frame::Channel::C);

    if (m_dchip.count() == 2)
    {
        a = (0.5f * (a + ComputeChannelLevel(1, Frame::Channel::A)));
        b = (0.5f * (b + ComputeChannelLevel(1, Frame::Channel::B)));
        c = (0.5f * (c + ComputeChannelLevel(1, Frame::Channel::C)));
    }

    if (m_dchip.output() == Chip::Output::Stereo)
    {
        switch (m_dchip.stereo())
        {
        case Chip::Stereo::ABC: L = a; C = b; R = c; break;
        case Chip::Stereo::ACB: L = a; C = c; R = b; break;
        case Chip::Stereo::BAC: L = b; C = a; R = c; break;
        case Chip::Stereo::BCA: L = b; C = c; R = a; break;
        case Chip::Stereo::CAB: L = c; C = a; R = b; break;
        case Chip::Stereo::CBA: L = c; C = b; R = a; break;
        }
    }
    else
    {
        L = C = R = ((a + b + c) / 3.f);
    }
}

void Output::GetLevels(float& L, float& R) const
{
    float l, c, r;
    GetLevels(l, c, r);

    if (m_dchip.output() == Chip::Output::Stereo)
    {
        L = ((l + 0.5f * c) / 1.5f);
        R = ((r + 0.5f * c) / 1.5f);
    }
    else
    {
        L = R = ((l + c + r) / 3.f);
    }
}

std::string Output::toString() const
{
    return (GetDeviceName() + " -> " + m_dchip.toString());
}

float Output::ComputeChannelLevel(int chip, int chan) const
{
    uint8_t volume = 0;
    auto& regs = m_processing.GetFrame()[chip];

    if (regs.IsEnvelopeEnabled(chan))
    {
        // envelope is enabled in the given channel
        if (regs.IsPeriodicEnvelope(chan))
        {
            // if periodic envelope set max volume
            volume = regs.vmask();
        }
    }
    else
    {
        // envelope is disabled in the given channel
        if (regs.IsToneEnabled(chan) || regs.IsNoiseEnabled(chan))
        {
            // tone or noise is enabled
            volume = regs.GetVolume(chan);
        }
    }
    return (float(volume) / float(regs.vmask()));
}
