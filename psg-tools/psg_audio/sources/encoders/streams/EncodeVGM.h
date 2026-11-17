#pragma once

#include <vector>
#include "encoders/Encoder.h"
#include "stream/Chip.h"

// Writes the frames as VGM 1.71 AY8910 commands for one or two chips.
// The file is .vgm, or .vgz compressed with gzip.
class EncodeVGM : public Encoder
{
public:
	bool Open(const Stream& stream) override;
	void Encode(const Frame& frame) override;
	void Close(const Stream& stream) override;

private:
	void WriteFrame(const Frame& frame);
	void WriteWait();
	uint32_t SampleOf(FrameId frameId) const;

private:
	std::vector<uint8_t> m_data; // commands after the header
	double   m_frameRate;
	int      m_chips;
	FrameId  m_frameId;      // of the next frame
	uint32_t m_wait;         // samples not written yet
	uint32_t m_loopOffset;   // of the loop frame in the data
	uint32_t m_loopSamples;  // before the loop frame
	ChipEncoder m_encoder;
	Frame    m_lastFrame;    // the registers after the last frame
	Frame    m_preLoopFrame; // the registers before the loop frame
};
