#include "EncodeVTX.h"
#include "lha/lh5.h"

namespace
{
	const uint8_t c_masks[14] = { 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0x3F, 0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0x0F };

	void PutU16(std::ofstream& output, uint32_t x) { output << uint8_t(x) << uint8_t(x >> 8); }
	void PutU32(std::ofstream& output, uint32_t x) { PutU16(output, x); PutU16(output, x >> 16); }
}

bool EncodeVTX::Open(const Stream& stream)
{
	if (CheckFileExt(stream, "vtx"))
	{
		m_output.open(stream.file, std::fstream::binary);
		if (m_output)
		{
			m_frames.clear();
			return true;
		}
	}
	return false;
}

void EncodeVTX::Encode(const Frame& frame)
{
	// only the first chip in the standard mode can be stored,
	// value 0xFF of the envelope shape means no write to R13
	for (Register reg = Register::BankA_Fst; reg <= Register::BankA_Lst; ++reg)
	{
		if (reg == Register::E_Shape && !frame[0].IsChanged(reg))
			m_frames.push_back(c_unchangedShape);
		else
			m_frames.push_back(frame[0].Read(reg) & c_masks[reg]);
	}
}

void EncodeVTX::Close(const Stream& stream)
{
	// registers are stored one after another, each for all frames
	size_t count = m_frames.size() / 14;
	std::vector<uint8_t> data(m_frames.size());
	for (size_t frame = 0; frame < count; ++frame)
	{
		for (size_t reg = 0; reg < 14; ++reg)
			data[reg * count + frame] = m_frames[frame * 14 + reg];
	}

	std::vector<uint8_t> packed;
	if (lh5::Compress(data.data(), data.size(), packed) == lh5::Result::Ok)
	{
		uint8_t stereo = 0x00;
		if (stream.dchip.output != Chip::Output::Mono)
		{
			stereo = uint8_t(Chip::Stereo(stream.dchip.stereo));
			if (!stereo) stereo = uint8_t(Chip::Stereo::ABC);
		}
		uint32_t loop = uint32_t(stream.loopFrameId);

		m_output << (stream.dchip.first.model == Chip::Model::YM2149 ? "ym" : "ay");
		m_output << stereo;
		PutU16(m_output, loop <= 0xFFFF ? loop : 0);
		PutU32(m_output, uint32_t(stream.dchip.clockValue));
		m_output << uint8_t(stream.play.frameRate);
		PutU16(m_output, 0); // year
		PutU32(m_output, uint32_t(data.size()));

		m_output << (std::string)stream.info.title << char(0);
		m_output << (std::string)stream.info.artist << char(0);
		m_output << char(0); // program
		m_output << char(0); // tracker
		m_output << (std::string)stream.info.comment << char(0);
		m_output.write((const char*)packed.data(), packed.size());
	}
	m_output.close();
}
