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
	// Returns the processed frame. Its register data is valid, but its
	// changes are not: a processor rewrites registers several times per
	// frame (copy the source, then overwrite with the processed value)
	// and each Update keeps only the diff of the last write. So never
	// read IsChanged() from the result directly, merge it with += into
	// a frame that holds the previous result: += recomputes the changes
	// from the data. The envelope shape register is the exception, its
	// write (retrigger) flag passes through processors unchanged.
	virtual const Frame& Execute(const Frame& frame);

protected:
	Frame m_frame;
};
