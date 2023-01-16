#include "Output.h"

#include <cassert>
#include "stream/Stream.h"
#include "processing/ChannelsOutputEnable.h"
#include "processing/ChipClockRateConvert.h"
#include "processing/ChannelsLayoutChange.h"
#include "processing/AY8930EnvelopeFix.h"

////////////////////////////////////////////////////////////////////////////////

#if DBG_PROCESSING && defined(_DEBUG)
#include <fstream>
static std::ofstream dbg;
static void dbg_close() { if (dbg.is_open()) dbg.close(); }
static void dbg_open () { dbg_close(); dbg.open("dbg_processing.txt"); }
static void dbg_print_endl() { if (dbg.is_open()) dbg << std::endl; }
static void dbg_print_payload(char tag, const Frame& frame) 
{
    if (dbg.is_open())
    {
        dbg << tag << ": ";
        for (int chip = 0; chip < 2; ++chip)
        {
            bool isExpMode = frame[chip].IsExpMode();
            for (Register reg = 0; reg < (isExpMode ? 32 : 16); ++reg)
            {
                int d = frame[chip].GetData(reg);
                if (frame[chip].IsChanged(reg))
                    dbg << '[' << std::hex << std::setw(2) << std::setfill('0') << d << ']';
                else
                    dbg << ' ' << std::hex << std::setw(2) << std::setfill('0') << d << ' ';
            }
            dbg << ':';
        }
        dbg_print_endl();
    }
}
#else
static void dbg_open() {}
static void dbg_close() {}
static void dbg_print_endl() {}
static void dbg_print_payload(char tag, const Frame& frame) {}
#endif

////////////////////////////////////////////////////////////////////////////////

Output::Output()
    : m_isOpened(false)
{
}

Output::~Output()
{
    dbg_close();
}

bool Output::Open()
{
    m_isOpened = DeviceOpen();
    return m_isOpened;
}

bool Output::Init(const Stream& stream)
{
    if (m_isOpened)
    {
        m_dchip = stream.dchip;
        if (m_isOpened &= DeviceInit(stream, m_dchip))
        {
            // check if the output chip setup is correct
            assert(m_dchip.clockKnown());
            assert(m_dchip.outputKnown());
            if (m_dchip.output() == Chip::Output::Stereo)
            {
                assert(m_dchip.stereoKnown());
            }

            // init post-processing
            m_procChain.clear();
            m_procChain.emplace_back(new ChannelsOutputEnable(m_dchip));
            m_procChain.emplace_back(new ChipClockRateConvert(stream.schip, m_dchip));
            m_procChain.emplace_back(new ChannelsLayoutChange(m_dchip));
            m_procChain.emplace_back(new AY8930EnvelopeFix(m_dchip));
            Reset();
            dbg_open();

#if defined(DEBUG_TEST)
            m_psg.Init();
#endif
        }
    }
    return m_isOpened;
}

bool Output::Write(const Frame& frame)
{
    if (m_isOpened)
    {
        // processing before output
        const Frame& pframe = static_cast<Processing&>(*this)(frame);

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

#if defined(DEBUG_TEST)
            if (chip == 0)
            {
                for (const auto& pair : data)
                {
                    const uint8_t& reg = pair.first;
                    const uint8_t& val = pair.second;
                    debug_psg_write(reg);
                    debug_psg_write(val);
                }
                debug_psg_write(0xFF);
                if (m_output != data)
                {
                    std::cout << std::endl;
                }
                if (!(m_isOpened &= DeviceWrite(chip, m_output))) break;
            }
#else
            if (!(m_isOpened &= DeviceWrite(chip, data))) break;
#endif
        }
    }
    return m_isOpened;
}

#if defined(DEBUG_TEST)
void Output::debug_psg_write(uint8_t data)
{
    if (m_reg < 0x10)
    {
        // received data for register
        m_psg.SetRegister(m_reg, data);
        m_reg = 0xFF;
    }
    else if (data < 0x10)
    {
        // received register number
        m_reg = data;
    }
    else if (data == 0xFF)
    {
        // expected register number, but
        // received end-of-frame marker
        m_psg.Update(m_output);
    }
}
#endif

void Output::Close()
{
    DeviceClose();
    m_isOpened = false;
}

const Output::Enables& Output::GetEnables() const
{
    auto& processing = static_cast<ChannelsOutputEnable&>(*m_procChain.front());
    return processing.GetEnables();
}

Output::Enables& Output::GetEnables()
{
    auto& processing = static_cast<ChannelsOutputEnable&>(*m_procChain.front());
    return processing.GetEnables();
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

void Output::Reset()
{
    for (const auto& procStep : m_procChain)
    {
        procStep->Reset();
    }
    Processing::Reset();
}

const Frame& Output::operator()(const Frame& frame)
{
    const Frame* pframe = &frame;
    dbg_print_payload('S', *pframe);

    for (const auto& procStep : m_procChain)
    {
        pframe = &(*procStep)(*pframe);
        dbg_print_payload('P', *pframe);
    }

    Processing::Update(*pframe);
    dbg_print_payload('D', m_frame);
    dbg_print_endl();
    return m_frame;
}

float Output::ComputeChannelLevel(int chip, int chan) const
{
    uint8_t volume = 0;
    uint8_t maxVolume = m_frame[chip].vmask();
    if (m_frame[chip].IsEnvelopeEnabled(chan))
    {
        // envelope is enabled in the given channel
        if (m_frame[chip].IsPeriodicEnvelope(chan))
        {
            // if periodic envelope set max volume
            volume = maxVolume;
        }
    }
    else
    {
        // envelope is disabled in the given channel
        if (m_frame[chip].IsToneEnabled(chan) || m_frame[chip].IsNoiseEnabled(chan))
        {
            // tone or noise is enabled
            volume = m_frame[chip].GetVolume(chan);
        }
    }
    return (float(volume) / float(maxVolume));
}
