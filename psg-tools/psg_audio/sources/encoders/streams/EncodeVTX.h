#pragma once

#include "encoders/Encoder.h"

class EncodeVTX : public Encoder
{
public:
	bool Open  (const Stream& stream) override;
	void Encode(const Frame&  frame ) override;
	void Close (const Stream& stream) override;

private:
	std::ofstream m_output;
	std::vector<uint8_t> m_frames; // 14 registers per frame
};
