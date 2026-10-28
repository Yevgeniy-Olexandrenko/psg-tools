#pragma once

#include <fstream>
#include <filesystem>
#include "player/PCMOutput.h"

class WaveFile : public PCMOutput
{
public:
	WaveFile(const std::filesystem::path& path);
	virtual ~WaveFile();

protected:
	const std::string GetDeviceName() const override;

	bool DeviceOpen() override;
	bool DeviceInit(const Stream& stream, Chip& dchip) override;
	bool DeviceWrite(int chip, const Data& data) override;
	void DeviceClose() override;

private:
	void WriteHeader();
	void UpdateHeader();

private:
	std::filesystem::path m_path;
	std::ofstream m_output;
	int m_lastChip;

	double m_samplesPerFrame;
	double m_samplesToRender;
	uint32_t m_dataSize;
	uint32_t m_headerUpdateSize;

	std::vector<float> m_floatBuffer;
	std::vector<int16_t> m_sampleBuffer;
};
