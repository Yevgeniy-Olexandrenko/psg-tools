#pragma once

#include "encoders/Encoder.h"

class EncodeRSF : public Encoder
{
#pragma pack(push, 1)
	struct Header
	{
		uint32_t m_sigAndVer;
		uint16_t m_frameRate;
		uint16_t m_dataOffset;
		uint32_t m_frameCount;
		uint32_t m_loopFrame;
		uint32_t m_chipFreq;
	};
#pragma pack(pop)

public:
	bool Open(const Stream& stream) override;
	void Encode(const Frame& frame) override;
	void Close(const Stream& stream) override;

private:
	void WriteSkip();

private:
	std::ofstream m_output;
	uint16_t m_skip;
};