#include "FrameProcessor.h"

void FrameProcessor::Reset()
{
    m_frame.ResetChanges();
    m_frame.ResetData();
}

void FrameProcessor::Update(const Frame& frame)
{
    m_frame.ResetChanges();
    m_frame += frame;
}

const Frame& FrameProcessor::Execute(const Frame& frame)
{
    return frame;
}
