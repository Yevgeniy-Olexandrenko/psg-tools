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
            if (m_dchip.hasExpMode(chip) && pframe[chip].IsExpMode())
            {
                bool switchBanks = false;
                for (Register reg = BankB_Fst; reg < BankB_Lst; ++reg)
                {
                    if (pframe[chip].IsChanged(reg))
                    {
                        if (!switchBanks)
                        {
                            data.emplace_back(Mode_Bank, pframe[chip].GetData(Mode_Bank) | 0x10);
                            switchBanks = true;
                        }
                        data.emplace_back(reg & 0x0F, pframe[chip].GetData(reg));
                    }
                }
                if (switchBanks)
                    data.emplace_back(Mode_Bank, pframe[chip].GetData(Mode_Bank));
            }

            for (Register reg = BankA_Fst; reg <= BankA_Lst; ++reg)
            {
                if (pframe[chip].IsChanged(reg))
                    data.emplace_back(reg & 0x0F, pframe[chip].GetData(reg));
            }

            if (!(m_isOpened &= DeviceWrite(chip, data))) break;
        }
    }
    return m_isOpened;
}

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

std::string Output::toString() const
{
    return (GetDeviceName() + " -> " + m_dchip.toString());
}

float Output::GetLevelL() const
{
    return m_levels[0];
}

float Output::GetLevelR() const
{
    return m_levels[1];
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

    ComputeLevels();
    return m_frame;
}

void Output::ComputeLevels()
{
    float levelA = ComputeChannelLevel(0, Frame::Channel::A);
    float levelB = ComputeChannelLevel(0, Frame::Channel::B);
    float levelC = ComputeChannelLevel(0, Frame::Channel::C);

    if (m_dchip.count() == 2)
    {
        levelA = (0.5f * (levelA + ComputeChannelLevel(1, Frame::Channel::A)));
        levelB = (0.5f * (levelB + ComputeChannelLevel(1, Frame::Channel::B)));
        levelC = (0.5f * (levelC + ComputeChannelLevel(1, Frame::Channel::C)));
    }

    if (m_dchip.output() == Chip::Output::Stereo)
    {
        switch (m_dchip.stereo())
        {
        case Chip::Stereo::ABC:
            m_levels[0] = ((levelA + 0.5f * levelB) / 1.5f);
            m_levels[1] = ((levelC + 0.5f * levelB) / 1.5f);
            break;

        case Chip::Stereo::ACB:
            m_levels[0] = ((levelA + 0.5f * levelC) / 1.5f);
            m_levels[1] = ((levelB + 0.5f * levelC) / 1.5f);
            break;

        case Chip::Stereo::BAC:
            m_levels[0] = ((levelB + 0.5f * levelA) / 1.5f);
            m_levels[1] = ((levelC + 0.5f * levelA) / 1.5f);
            break;

        case Chip::Stereo::BCA:
            m_levels[0] = ((levelB + 0.5f * levelC) / 1.5f);
            m_levels[1] = ((levelA + 0.5f * levelC) / 1.5f);
            break;

        case Chip::Stereo::CAB:
            m_levels[0] = ((levelC + 0.5f * levelA) / 1.5f);
            m_levels[1] = ((levelB + 0.5f * levelA) / 1.5f);
            break;

        case Chip::Stereo::CBA:
            m_levels[0] = ((levelC + 0.5f * levelB) / 1.5f);
            m_levels[1] = ((levelA + 0.5f * levelB) / 1.5f);
            break;
        }
    }
    else
    {
        m_levels[0] = ((levelA + levelB + levelC) / 3.f);
        m_levels[1] = m_levels[0];
    }
}

float Output::ComputeChannelLevel(int chip, int chan)
{
    static const float c_volumeToLevel[] =
    {
        0.000f, 0.000f, 0.005f, 0.008f, 0.011f, 0.014f, 0.017f, 0.020f,
        0.024f, 0.030f, 0.035f, 0.040f, 0.049f, 0.058f, 0.068f, 0.078f,
        0.093f, 0.111f, 0.130f, 0.148f, 0.177f, 0.212f, 0.246f, 0.281f,
        0.334f, 0.400f, 0.467f, 0.534f, 0.635f, 0.758f, 0.880f, 1.000f
    };

    static const Register c_vRegs[]{ A_Volume, B_Volume, C_Volume };
    static const Register c_sRegs[]{ EA_Shape, EB_Shape, EC_Shape };

    uint8_t mixer = m_frame[chip].GetData(Mixer);
    uint8_t vol_e = m_frame[chip].GetData(c_vRegs[chan]);

    uint8_t volume = 0;
    if (vol_e & m_frame[chip].emask())
    {
        // envelope is enabled in the given channel
        uint8_t shape = (m_frame[chip].GetData(m_frame[chip].IsExpMode() ? c_sRegs[chan] : E_Shape) & 0x0F);
        if (shape == 0x08 || shape == 0x0A || shape == 0x0C || shape == 0x0E)
        {
            // if periodic envelope set max volume
            volume = m_frame[chip].vmask();
        }
    }
    else
    {
        // envelope is disabled in the given channel
        uint8_t tnMask = (m_frame[chip].tmask(chan) | m_frame[chip].nmask(chip));
        if ((mixer & tnMask) != tnMask)
        {
            // tone or noise is enabled
            volume = (vol_e & m_frame[chip].vmask());
        }
    }

    if (!m_frame[chip].IsExpMode()) { volume <<= 1; ++volume; }
#if 1
    return (volume / 31.f);
#else
    return c_volumeToLevel[volume];
#endif
}
