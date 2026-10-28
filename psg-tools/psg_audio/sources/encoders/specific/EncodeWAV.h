#pragma once

#include "encoders/Encoder.h"
#include "player/WaveFile/WaveFile.h"

class EncodeWAV : public Encoder
{
public:
	bool Open  (const Stream& stream) override;
	void Encode(const Frame&  frame ) override;
	void Close (const Stream& stream) override;

	Duration GetDefaultDuration() const override { return Duration::Playback; }
	bool HasOwnProcessing() const override { return true; }

private:
	std::unique_ptr<WaveFile> m_output;
	FrameId m_frameId;
};
