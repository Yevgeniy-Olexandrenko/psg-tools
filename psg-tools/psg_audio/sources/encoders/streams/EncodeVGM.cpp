#include "EncodeVGM.h"
#include <algorithm>
#include <cmath>
#include <zlib/zlib.h>

namespace
{
	const uint32_t c_headerSize = 0x100;
	const uint32_t c_version = 0x171;
	const uint32_t c_sampleRate = 44100;

	// AY8910 chip types of VGM
	const uint8_t c_typeAY8910 = 0x00;
	const uint8_t c_typeAY8930 = 0x03;
	const uint8_t c_typeYM2149 = 0x10;

	// AY8910 flags: legacy output (the default) and ZX Spectrum like ABC panning
	const uint8_t c_flagLegacyOutput = 0x01;
	const uint8_t c_flagStereoABC = 0x80;

	bool IsShape(Register reg)
	{
		return (reg == Register::EA_Shape || reg == Register::EB_Shape || reg == Register::EC_Shape);
	}

	void PutU16(std::vector<uint8_t>& data, uint32_t x)
	{
		data.push_back(uint8_t(x));
		data.push_back(uint8_t(x >> 8));
	}

	void PutU32(std::vector<uint8_t>& data, uint32_t x)
	{
		PutU16(data, x);
		PutU16(data, x >> 16);
	}

	// The bytes of the string go as Latin-1 characters. A 0 character ends the string.
	void PutString(std::vector<uint8_t>& data, const std::string& str)
	{
		for (char ch : str) PutU16(data, uint8_t(ch));
		PutU16(data, 0);
	}
}

bool EncodeVGM::Open(const Stream& stream)
{
	if (CheckFileExt(stream, "vgm") || CheckFileExt(stream, "vgz"))
	{
		m_data.clear();
		m_frameRate = stream.play.frameRate;
		m_chips = stream.dchip.count;
		m_frameId = 0;
		m_wait = 0;
		m_loopOffset = 0;
		m_loopSamples = 0;
		m_expMode[0] = m_expMode[1] = false;
		return true;
	}
	return false;
}

void EncodeVGM::Encode(const Frame& frame)
{
	// At the loop, the chips keep the registers of the last frame.
	// So the loop frame writes all the registers again.
	bool loop = (m_loopFrameId && m_frameId == m_loopFrameId);
	if (loop)
	{
		WriteWait();
		m_loopOffset = uint32_t(m_data.size());
		m_loopSamples = SampleOf(m_frameId);
	}

	for (int chip = 0; chip < m_chips; ++chip)
	{
		WriteChip(frame, chip, loop);
	}

	++m_frameId;
	m_wait += (SampleOf(m_frameId) - SampleOf(m_frameId - 1));
}

void EncodeVGM::Close(const Stream& stream)
{
	WriteWait();
	m_data.push_back(0x66); // end of sound data

	uint32_t totalSamples = SampleOf(m_frameId);
	uint32_t gd3Offset = (c_headerSize + uint32_t(m_data.size()));

	// GD3 tag: 11 UTF-16 strings. Track, game, system and author have an
	// English and a Japanese string, then date, converter and notes follow.
	std::vector<uint8_t> gd3;
	PutString(gd3, stream.info.title);
	PutString(gd3, "");
	PutString(gd3, ""); // game
	PutString(gd3, "");
	PutString(gd3, ""); // system
	PutString(gd3, "");
	PutString(gd3, stream.info.artist);
	PutString(gd3, "");
	PutString(gd3, ""); // date
	PutString(gd3, "PSG Tools");
	PutString(gd3, stream.info.comment);

	// One chip type for both chips: AY8930 if one of them is,
	// else its expanded mode registers do not play.
	uint8_t type = (stream.dchip.first.model == Chip::Model::YM2149 ? c_typeYM2149 : c_typeAY8910);
	for (int chip = 0; chip < m_chips; ++chip)
	{
		if (stream.dchip.model(chip) == Chip::Model::AY8930) type = c_typeAY8930;
	}

	// The processing gives the frames the ABC layout.
	uint8_t flags = c_flagLegacyOutput;
	if (stream.dchip.output == Chip::Output::Stereo) flags |= c_flagStereoABC;

	uint32_t clock = uint32_t(stream.dchip.clockValue);
	if (m_chips == 2) clock |= 0x40000000;

	std::vector<uint8_t> file(c_headerSize, 0);
	const auto SetU32 = [&](uint32_t offset, uint32_t x)
	{
		for (int i = 0; i < 4; ++i) file[offset + i] = uint8_t(x >> (8 * i));
	};

	file.insert(file.end(), m_data.begin(), m_data.end());
	file.insert(file.end(), { 'G', 'd', '3', ' ' });
	PutU32(file, 0x00000100);
	PutU32(file, uint32_t(gd3.size()));
	file.insert(file.end(), gd3.begin(), gd3.end());

	// An offset is relative to the position of its field.
	SetU32(0x00, 0x206D6756); // "Vgm "
	SetU32(0x04, uint32_t(file.size()) - 0x04);
	SetU32(0x08, c_version);
	SetU32(0x14, gd3Offset - 0x14);
	SetU32(0x18, totalSamples);
	if (m_loopFrameId)
	{
		SetU32(0x1C, c_headerSize + m_loopOffset - 0x1C);
		SetU32(0x20, totalSamples - m_loopSamples);
	}
	SetU32(0x24, uint32_t(RoundFrameRate(m_frameRate)));
	SetU32(0x34, c_headerSize - 0x34);
	SetU32(0x74, clock);
	file[0x78] = type;
	file[0x79] = flags;

	if (CheckFileExt(stream, "vgz"))
	{
		if (gzFile gz = gzopen(stream.file.string().c_str(), "wb9"))
		{
			gzwrite(gz, file.data(), unsigned(file.size()));
			gzclose(gz);
		}
	}
	else
	{
		std::ofstream output(stream.file, std::fstream::binary);
		output.write((const char*)file.data(), file.size());
	}
}

void EncodeVGM::WriteChip(const Frame& frame, int chip, bool loop)
{
	const auto& regs = frame[chip];
	const auto Write = [&](Register reg)
	{
		// The loop frame does not write an unchanged shape: a write restarts the envelope.
		if (regs.IsChanged(reg) || (loop && !IsShape(reg)))
		{
			WriteRegister(chip, reg, regs.Read(reg));
		}
	};

	// A change of the AY8930 mode resets the registers, so R13 goes first.
	// Else R13 goes last: the shape restarts the envelope after its period is set.
	bool modeChange = (regs.IsExpMode() != m_expMode[chip]);
	m_expMode[chip] = regs.IsExpMode();
	if (modeChange) Write(Register::Mode_Bank);

	for (Register reg = Register::BankA_Fst; reg < Register::Mode_Bank; ++reg) Write(reg);

	// The expanded mode registers go as 0x10-0x1A, as in the PSG format.
	if (regs.IsExpMode())
	{
		for (Register reg = Register::EB_Fine; reg <= Register::N_OrMask; ++reg) Write(reg);
	}

	if (!modeChange) Write(Register::Mode_Bank);
}

void EncodeVGM::WriteRegister(int chip, Register reg, uint8_t data)
{
	WriteWait();
	m_data.push_back(0xA0);
	m_data.push_back(uint8_t(chip << 7 | reg));
	m_data.push_back(data);
}

void EncodeVGM::WriteWait()
{
	while (m_wait)
	{
		uint32_t samples = std::min(m_wait, uint32_t(0xFFFF));
		if (samples == 735) m_data.push_back(0x62);
		else if (samples == 882) m_data.push_back(0x63);
		else if (samples <= 16) m_data.push_back(uint8_t(0x70 | (samples - 1)));
		else
		{
			m_data.push_back(0x61);
			PutU16(m_data, samples);
		}
		m_wait -= samples;
	}
}

uint32_t EncodeVGM::SampleOf(FrameId frameId) const
{
	// The frame rate can be fractional. Round the time of each frame: the error
	// stays under one sample. Rounded frame lengths would add up their errors.
	return uint32_t(std::llround(frameId * double(c_sampleRate) / m_frameRate));
}
