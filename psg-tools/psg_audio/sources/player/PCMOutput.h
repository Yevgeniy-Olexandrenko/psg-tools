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
	bool DeviceWrite(int chip, const Data& data) override;

	void PCMInit(Chip& dchip);
	void PCMFill(std::vector<float>& buffer);

private:
	std::unique_ptr<AY89XXPsg> m_psg[2];
	float m_panL[3], m_panR[3];
};
