#pragma once

#include "player/PCMOutput.h"
#include "WaveAudio.h"

class Emulator : public PCMOutput, public WaveAudio
{
public:
	Emulator();
	virtual ~Emulator();

protected:
	const std::string GetDeviceName() const override;

	bool DeviceOpen() override;
	bool DeviceInit(const Stream& stream, Chip& dchip) override;
	void FillBuffer(std::vector<float>& buffer) override;
	void DeviceClose() override;
};
