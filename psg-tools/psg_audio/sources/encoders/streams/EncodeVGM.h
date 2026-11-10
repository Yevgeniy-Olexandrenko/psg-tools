#pragma once

#include <vector>
#include "encoders/Encoder.h"

// Writes the frames as VGM 1.71 AY8910 commands for one or two chips.
// The file is .vgm, or .vgz compressed with gzip.
class EncodeVGM : public Encoder
{
public:
	bool Open  (const Stream& stream) override;
	void Encode(const Frame&  frame ) override;
	void Close (const Stream& stream) override;

private:
	void WriteChip(const Frame& frame, int chip, bool loop);
	void WriteRegister(int chip, Register reg, uint8_t data);
	void WriteWait();
	uint32_t SampleOf(FrameId frameId) const;

private:
	std::vector<uint8_t> m_data; // commands after the header
	double   m_frameRate;
	int      m_chips;
	FrameId  m_frameId;     // of the next frame
	uint32_t m_wait;        // samples not written yet
	uint32_t m_loopOffset;  // of the loop frame in the data
	uint32_t m_loopSamples; // before the loop frame
	bool     m_expMode[2];  // in the last frame
};
