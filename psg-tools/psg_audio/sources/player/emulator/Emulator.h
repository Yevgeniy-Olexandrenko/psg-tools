#pragma once

#include "emulation/AY89XXPsg.h"
#include "player/Output.h"
#include "WaveAudio.h"

constexpr int k_emulatorSampleRate = 44100;

class Emulator : public Output, public WaveAudio
{
public:
	Emulator();
	virtual ~Emulator();

protected:
	const std::string GetDeviceName() const override;

	bool DeviceOpen() override;
	bool DeviceInit(const Stream& stream, Chip& dchip) override;
	bool DeviceWrite(int chip, const Data& data) override;
	void FillBuffer(std::vector<float>& buffer) override;
	void DeviceClose() override;

private:
	void SetPan(int chan, float pan, bool isEqp);

private:
	std::unique_ptr<AY89XXPsg> m_psg[2];
	float m_panL[3], m_panR[3];
};
