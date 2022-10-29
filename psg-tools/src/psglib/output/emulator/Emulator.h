#pragma once

#include "SoundChip.h"
#include "output/Output.h"
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
	void FillBuffer(unsigned char* buffer, unsigned long size) override;
	void DeviceClose() override;

private:
	std::unique_ptr<SoundChip> m_psg[2];
};
