#pragma once

#include <fstream>
#include <filesystem>
#include "player/PCMOutput.h"

class WaveFile : public PCMOutput
{
public:
	WaveFile(const std::filesystem::path& path);
	virtual ~WaveFile();

	// adds the stream's title, artist, comment and type to the
	// file as a LIST/INFO chunk, must be called before Open()
	void SetInfo(const Stream& stream);

	// marks the current position as the loop start, the loop
	// goes to the end and is saved as a smpl chunk on close
	void SetLoopStart();

protected:
	const std::string GetDeviceName() const override;

	bool DeviceOpen() override;
	bool DeviceInit(const Stream& stream, Chip& dchip) override;
	bool DeviceWrite(int chip, const Data& data) override;
	void DeviceClose() override;

private:
	void WriteHeader();
	void WriteInfo();
	void WriteLoop();
	void UpdateHeader();

private:
	using InfoItem = std::pair<std::string, std::string>;

	std::filesystem::path m_path;
	std::vector<InfoItem> m_info;
	std::ofstream m_output;
	int m_lastChip;

	double m_samplesPerFrame;
	double m_samplesToRender;
	uint32_t m_dataSize;
	uint32_t m_tailSize;
	uint32_t m_headerUpdateSize;
	uint32_t m_loopStart;
	bool m_hasLoop;

	std::vector<float> m_floatBuffer;
	std::vector<int16_t> m_sampleBuffer;
};
