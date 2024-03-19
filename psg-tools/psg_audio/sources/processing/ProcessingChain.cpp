#include "ProcessingChain.h"
#include "debug/DebugOutput.h"

static DebugOutput<DBG_PROCESSING> dbg;

ProcessingChain::ProcessingChain(const std::string& tag)
    : m_tag(tag)
{
    dbg.open(m_tag);
}

ProcessingChain::~ProcessingChain()
{
	for (auto processor : m_chain) delete processor;
	m_chain.clear();
    dbg.close();
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
    dbg.open(m_tag);
}

const Frame& ProcessingChain::Execute(const Frame& frame)
{
    const Frame* pframe = &frame;
    dbg.print_payload('S', pframe);

    char p = '0';
    for (auto processor : m_chain)
    {
        pframe = &processor->Execute(*pframe);
        dbg.print_payload(p++, pframe);
    }

    FrameProcessor::Update(*pframe);
    dbg.print_payload('D', &m_frame);
    dbg.print_endline();
    return m_frame;
}

const Frame& ProcessingChain::GetFrame() const
{
    return m_frame;
}
