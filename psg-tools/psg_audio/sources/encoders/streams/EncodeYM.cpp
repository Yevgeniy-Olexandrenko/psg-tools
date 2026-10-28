#include "EncodeYM.h"
#include "lha/lzh.h"

namespace
{
	const uint8_t c_masks[14] = { 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0x3F, 0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0x0F };

	void PutU16(std::vector<uint8_t>& data, uint32_t x) { data.push_back(uint8_t(x >> 8)); data.push_back(uint8_t(x)); }
	void PutU32(std::vector<uint8_t>& data, uint32_t x) { PutU16(data, x >> 16); PutU16(data, x); }
	void PutStr(std::vector<uint8_t>& data, const std::string& s) { data.insert(data.end(), s.begin(), s.end()); data.push_back(0); }
}

bool EncodeYM::Open(const Stream& stream)
{
	if (CheckFileExt(stream, "ym"))
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

void EncodeYM::Encode(const Frame& frame)
{
	// only the first chip in the standard mode can be stored,
	// value 0xFF of the envelope shape means no write to R13,
	// R14 and R15 hold special effects, which are not used
	for (Register reg = Register::BankA_Fst; reg <= Register::BankA_Lst; ++reg)
	{
		if (reg == Register::E_Shape && !frame[0].IsChanged(reg))
			m_frames.push_back(c_unchangedShape);
		else
			m_frames.push_back(frame[0].Read(reg) & c_masks[reg]);
	}
	m_frames.push_back(0x00);
	m_frames.push_back(0x00);
}

void EncodeYM::Close(const Stream& stream)
{
	size_t count = m_frames.size() / 16;

	// YM6 header, all values are big-endian
	std::vector<uint8_t> data;
	data.reserve(64 + m_frames.size());
	PutStr(data, "YM6!LeOnArD!"); data.pop_back();
	PutU32(data, uint32_t(count));
	PutU32(data, 0x00000001); // song attributes: interleaved
	PutU16(data, 0);          // number of digidrums
	PutU32(data, uint32_t(stream.dchip.clockValue));
	PutU16(data, uint16_t(stream.play.frameRate));
	PutU32(data, uint32_t(m_loopFrameId));
	PutU16(data, 0);          // size of additional data
	PutStr(data, stream.info.title);
	PutStr(data, stream.info.artist);
	PutStr(data, stream.info.comment);

	// interleaved: registers are stored one after another, each for all frames
	size_t offset = data.size();
	data.resize(offset + m_frames.size());
	for (size_t frame = 0; frame < count; ++frame)
	{
		for (size_t reg = 0; reg < 16; ++reg)
			data[offset + reg * count + frame] = m_frames[frame * 16 + reg];
	}
	PutStr(data, "End!"); data.pop_back();

	std::vector<uint8_t> archive;
	std::string name = stream.file.filename().string();
	if (lzh::Pack(data.data(), data.size(), name, archive) == lzh::Result::Ok)
	{
		m_output.write((const char*)archive.data(), archive.size());
	}
	m_output.close();
}
