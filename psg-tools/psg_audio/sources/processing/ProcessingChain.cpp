#include "ProcessingChain.h"

#if DBG_PROCESSING && defined(_DEBUG)
#include <cassert>
#include <fstream>
#include <iomanip>
static std::ofstream file;
struct dbg
{
    static void open(const std::string& tag)
    {
        close();
        assert(!tag.empty());
        file.open("dbg_processing_" + tag + ".txt");
    }
    static void print_payload(char tag, const Frame& frame)
    {
        if (file.is_open())
        {
            file << tag << ": ";
            for (int chip = 0; chip < 2; ++chip)
            {
                bool isExpMode = frame[chip].IsExpMode();
                for (Register reg = 0; reg < (isExpMode ? 32 : 16); ++reg)
                {
                    int d = frame[chip].GetData(reg);
                    if (frame[chip].IsChanged(reg))
                        file << '[' << std::hex << std::setw(2) << std::setfill('0') << d << ']';
                    else
                        file << ' ' << std::hex << std::setw(2) << std::setfill('0') << d << ' ';
                }
                file << ':';
            }
            print_endl();
        }
    }
    static void print_endl()
    {
        if (file.is_open()) file << std::endl;
    }
    static void close()
    {
        if (file.is_open()) file.close();
    }
};
#else
struct dbg
{
    static void open(const std::string& tag) {}
    static void print_payload(char tag, const Frame& frame) {}
    static void print_endl() {}
    static void close() {}
};
#endif

ProcessingChain::ProcessingChain(const std::string& tag)
    : m_tag(tag)
{
    dbg::open(m_tag);
}

ProcessingChain::~ProcessingChain()
{
	for (auto processor : m_chain) delete processor;
	m_chain.clear();
    dbg::close();
}

FrameProcessor* ProcessingChain::GetProcessor(size_t index) const
{
    return (index < m_chain.size() ? m_chain[index] : nullptr);
}

void ProcessingChain::Clear()
{
    for (auto processor : m_chain) delete processor;
    m_chain.clear();

    FrameProcessor::Reset();
    dbg::open(m_tag);
}

const Frame& ProcessingChain::Execute(const Frame& frame)
{
    const Frame* pframe = &frame;
    dbg::print_payload('S', *pframe);

    char p = '0';
    for (auto processor : m_chain)
    {
        pframe = &processor->Execute(*pframe);
        dbg::print_payload(p++, *pframe);
    }

    FrameProcessor::Update(*pframe);
    dbg::print_payload('D', m_frame);
    dbg::print_endl();
    return m_frame;
}

const Frame& ProcessingChain::GetFrame() const
{
    return m_frame;
}
