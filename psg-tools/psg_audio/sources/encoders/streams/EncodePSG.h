#pragma once

#include "encoders/Encoder.h"
#include "stream/Chip.h"

class EncodePSG : public Encoder
{
public:
	bool Open(const Stream& stream) override;
	void Encode(const Frame& frame ) override;
	void Close(const Stream& stream) override;

private:
	void WriteFrameBeginOrSkip();

private:
	std::ofstream m_output;
	ChipEncoder m_encoder;
	uint16_t m_skip;
};