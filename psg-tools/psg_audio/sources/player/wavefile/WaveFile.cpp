#include "WaveFile.h"
#include "stream/Stream.h"

namespace
{
	constexpr uint16_t k_channels = 2;
	constexpr uint16_t k_bitsPerSample = 16;
	constexpr uint16_t k_blockAlign = (k_channels * k_bitsPerSample / 8);
	constexpr uint32_t k_headerSize = 44;

	// sizes in the header are updated about every second,
	// so the file stays playable even if the program is killed
	constexpr uint32_t k_headerUpdatePeriod = (k_PCMSampleRate * k_blockAlign);

	template<typename T> void WriteLE(std::ofstream& output, T value)
	{
		for (size_t i = 0; i < sizeof(T); ++i)
		{
			output.put(char(value & 0xFF));
			value >>= 8;
		}
	}
}

WaveFile::WaveFile(const std::filesystem::path& path)
	: m_path(path)
	, m_lastChip(-1)
	, m_samplesPerFrame(0)
	, m_samplesToRender(0)
	, m_dataSize(0)
	, m_headerUpdateSize(0)
{
}

WaveFile::~WaveFile()
{
	DeviceClose();
}

const std::string WaveFile::GetDeviceName() const
{
	return "Wave File";
}

bool WaveFile::DeviceOpen()
{
	m_output.open(m_path, std::fstream::binary);
	if (m_output)
	{
		m_dataSize = 0;
		m_headerUpdateSize = k_headerUpdatePeriod;
		WriteHeader();
		return bool(m_output);
	}
	return false;
}

bool WaveFile::DeviceInit(const Stream& stream, Chip& dchip)
{
	if (m_output)
	{
		PCMOutput::PCMInit(dchip);
		m_lastChip = dchip.count - 1;

		m_samplesPerFrame = double(k_PCMSampleRate) / stream.play.frameRate;
		m_samplesToRender = 0;
		return true;
	}
	return false;
}

bool WaveFile::DeviceWrite(int chip, const Data& data)
{
	constexpr float sampleMax = float(std::numeric_limits<int16_t>::max());
	constexpr float sampleMin = float(std::numeric_limits<int16_t>::min());

	PCMOutput::DeviceWrite(chip, data);
	if (chip == m_lastChip)
	{
		// render PCM for 1 frame time, the fractional part
		// of samples per frame is carried to the next frame
		m_samplesToRender += m_samplesPerFrame;
		size_t samples = size_t(m_samplesToRender);
		m_samplesToRender -= samples;

		m_floatBuffer.resize(samples * k_channels);
		m_sampleBuffer.resize(samples * k_channels);
		PCMOutput::PCMFill(m_floatBuffer);

		for (size_t i = 0; i < m_floatBuffer.size(); ++i)
		{
			float sample = (m_floatBuffer[i] * sampleMax);
			if (sample > sampleMax) sample = sampleMax;
			if (sample < sampleMin) sample = sampleMin;
			m_sampleBuffer[i] = int16_t(sample);
		}

		auto bytes = uint32_t(m_sampleBuffer.size() * sizeof(int16_t));
		m_output.write(reinterpret_cast<const char*>(m_sampleBuffer.data()), bytes);
		m_dataSize += bytes;

		if (m_dataSize >= m_headerUpdateSize)
		{
			m_headerUpdateSize = (m_dataSize + k_headerUpdatePeriod);
			UpdateHeader();
		}
	}
	return bool(m_output);
}

void WaveFile::DeviceClose()
{
	if (m_output.is_open())
	{
		UpdateHeader();
		m_output.close();
	}
}

void WaveFile::WriteHeader()
{
	m_output.write("RIFF", 4);
	WriteLE<uint32_t>(m_output, k_headerSize - 8 + m_dataSize);
	m_output.write("WAVE", 4);

	m_output.write("fmt ", 4);
	WriteLE<uint32_t>(m_output, 16);
	WriteLE<uint16_t>(m_output, 1); // PCM
	WriteLE<uint16_t>(m_output, k_channels);
	WriteLE<uint32_t>(m_output, k_PCMSampleRate);
	WriteLE<uint32_t>(m_output, k_PCMSampleRate * k_blockAlign);
	WriteLE<uint16_t>(m_output, k_blockAlign);
	WriteLE<uint16_t>(m_output, k_bitsPerSample);

	m_output.write("data", 4);
	WriteLE<uint32_t>(m_output, m_dataSize);
}

void WaveFile::UpdateHeader()
{
	auto position = m_output.tellp();
	m_output.seekp(4);
	WriteLE<uint32_t>(m_output, k_headerSize - 8 + m_dataSize);
	m_output.seekp(40);
	WriteLE<uint32_t>(m_output, m_dataSize);
	m_output.seekp(position);
	m_output.flush();
}
