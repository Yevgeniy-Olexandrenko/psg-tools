#include "Processing.h"

void Processing::Reset()
{
    m_frame.ResetData();
    m_frame.ResetChanges();
}

void Processing::Update(const Frame& frame)
{
    m_frame.ResetChanges();
    m_frame += frame;
}

const Frame& Processing::operator()(const Frame& frame)
{
    return frame;
}
