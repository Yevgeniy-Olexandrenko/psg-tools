#pragma once

#include "Output.h"
#include "emulation/AY89XXPsg.h"

constexpr int k_PCMSampleRate = 44100;

class PCMOutput : public Output
{
public:
	PCMOutput();
	virtual ~PCMOutput();

protected:
	void PCMInit(Chip& dchip);
	void PCMWrite(const FrameData& data);
	void PCMFill(float* buffer, size_t samples);

private:
	std::unique_ptr<AY89XXPsg> m_psg[2];
	float m_panL[3], m_panR[3];
};
