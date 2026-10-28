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
	, m_tailSize(0)
	, m_headerUpdateSize(0)
	, m_loopStart(0)
	, m_hasLoop(false)
{
}

WaveFile::~WaveFile()
{
	DeviceClose();
}

void WaveFile::SetInfo(const Stream& stream)
{
	m_info.clear();
	if (stream.info.titleKnown) m_info.emplace_back("INAM", stream.info.title);
	if (stream.info.artistKnown) m_info.emplace_back("IART", stream.info.artist);
	if (stream.info.commentKnown) m_info.emplace_back("ICMT", stream.info.comment);

	// source form: the format the sound comes from
	const std::string& type = stream.info.type;
	if (!type.empty()) m_info.emplace_back("ISRF", type);
}

void WaveFile::SetLoopStart()
{
	m_loopStart = (m_dataSize / k_blockAlign);
	m_hasLoop = true;
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
		m_tailSize = 0;
		m_headerUpdateSize = k_headerUpdatePeriod;
		m_loopStart = 0;
		m_hasLoop = false;
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
		WriteInfo();
		WriteLoop();
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

void WaveFile::WriteInfo()
{
	// the LIST chunk goes after the data chunk, Windows
	// Explorer does not show it in front of the data
	std::vector<InfoItem> info(m_info);
	info.emplace_back("ISFT", "PSG Tools");

	uint32_t listSize = 4;
	for (const auto& [id, text] : info)
	{
		listSize += 8 + uint32_t((text.size() + 2) & ~1);
	}

	m_output.write("LIST", 4);
	WriteLE<uint32_t>(m_output, listSize);
	m_output.write("INFO", 4);
	for (const auto& [id, text] : info)
	{
		// each text is null-terminated and padded to an even size
		auto size = uint32_t(text.size() + 1);
		m_output.write(id.data(), 4);
		WriteLE<uint32_t>(m_output, size);
		m_output.write(text.data(), text.size());
		m_output.write("\0\0", (size & 1) ? 2 : 1);
	}

	m_tailSize += (8 + listSize);
}

void WaveFile::WriteLoop()
{
	// the loop goes from its start to the last sample
	uint32_t samples = (m_dataSize / k_blockAlign);
	if (m_hasLoop && m_loopStart < samples)
	{
		m_output.write("smpl", 4);
		WriteLE<uint32_t>(m_output, 36 + 24);
		WriteLE<uint32_t>(m_output, 0); // manufacturer
		WriteLE<uint32_t>(m_output, 0); // product
		WriteLE<uint32_t>(m_output, 1000000000 / k_PCMSampleRate); // sample period, ns
		WriteLE<uint32_t>(m_output, 60); // MIDI unity note
		WriteLE<uint32_t>(m_output, 0); // MIDI pitch fraction
		WriteLE<uint32_t>(m_output, 0); // SMPTE format
		WriteLE<uint32_t>(m_output, 0); // SMPTE offset
		WriteLE<uint32_t>(m_output, 1); // number of loops
		WriteLE<uint32_t>(m_output, 0); // sampler data size

		WriteLE<uint32_t>(m_output, 0); // cue point id
		WriteLE<uint32_t>(m_output, 0); // type: forward
		WriteLE<uint32_t>(m_output, m_loopStart);
		WriteLE<uint32_t>(m_output, samples - 1); // last sample, inclusive
		WriteLE<uint32_t>(m_output, 0); // fraction
		WriteLE<uint32_t>(m_output, 0); // play count: infinite
		m_tailSize += (8 + 36 + 24);
	}
}

void WaveFile::UpdateHeader()
{
	auto position = m_output.tellp();
	m_output.seekp(4);
	WriteLE<uint32_t>(m_output, k_headerSize - 8 + m_dataSize + m_tailSize);
	m_output.seekp(40);
	WriteLE<uint32_t>(m_output, m_dataSize);
	m_output.seekp(position);
	m_output.flush();
}
