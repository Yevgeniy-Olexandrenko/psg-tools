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

void Output::SetEnables(const Enables& enables)
{
    auto& processing = static_cast<ChannelsOutputEnable&>(*m_procChain.front());
    processing.SetEnables(enables);
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
