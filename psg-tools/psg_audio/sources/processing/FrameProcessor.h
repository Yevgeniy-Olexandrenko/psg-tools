#pragma once

#include "stream/Chip.h"
#include "stream/Frame.h"

#define Enable_ChannelsOutputEnable
#define Enable_ChipClockRateConvert
#define Enable_ChannelsLayoutChange
#define Enable_AY8930EnvelopeFix

class FrameProcessor
{
protected:
	virtual void Reset();
	virtual void Update(const Frame& frame);

public:
	virtual const Frame& Execute(const Frame& frame);

protected:
	Frame m_frame;
};
