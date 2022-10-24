#pragma once

#include "decoders/Decoder.h"

class DecodeRSF : public Decoder
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
	bool Open(Stream& stream) override;
	bool Decode(Frame& frame) override;
	void Close(Stream& stream) override;

private:
	std::ifstream m_input;
	int m_loop;
	int m_skip;
};
