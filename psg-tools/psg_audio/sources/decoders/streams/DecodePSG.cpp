#include <algorithm>
#include "DecodePSG.h"

// PSG: a header of 16 bytes (or of 4 bytes: then 0xFF follows the
// signature), then a stream of bytes:
//   0xFF     opens a frame
//   0xFE N   opens N * 4 frames (N = 0: no frame, the frame continues)
//   0xFD     ends the music
//   R V      writes V to register R: 0x00-0x0F of the first chip,
//            0x80-0x8F of the second chip (TurboSound). Other R are
//            writes to other devices (MSX); the decoder skips them.
// The writes go to the last frame opened. Writes before the first marker
// go to frame 0. A file that starts with silence starts with 0xFE.

namespace
{
	const uint32_t PSGSignature = 0x1A475350;
}

bool DecodePSG::Open(Stream& stream)
{
	m_input.open(stream.file, std::fstream::binary);

	Header header;
	if (m_input.read((char*)(&header), sizeof(header)) && header.m_sig == PSGSignature)
	{
		stream.info.type = "PSG stream";
		if (header.m_ver == 0xFF)
		{
			// a header of 4 bytes: the data starts at the version byte
			m_input.seekg(offsetof(Header, m_ver));
			stream.play.frameRate = 50;
		}
		else
		{
			// the frame rate byte is in version 10 and later
			stream.play.frameRate = (header.m_ver >= 10 && header.m_fps ? header.m_fps : 50);
		}

		m_opened = 0;
		m_decoder.Init();
		return true;
	}
	return false;
}

bool DecodePSG::Decode(Frame& frame)
{
	// Read the markers before the writes. Use only peek() to look ahead:
	// unget() can fail at the end of the file buffer.
	while (!m_opened)
	{
		int marker = m_input.peek();
		if (marker == EOF || marker == 0xFD) return false;
		if (marker < 0xFD) m_opened = 1; // writes before the first marker
		else if (m_input.get() == 0xFF) m_opened = 1;
		else m_opened = std::max(m_input.get(), 0) * 4;
	}

	// of the frames opened together, only the last frame has writes
	if (--m_opened) return true;

	// read the writes up to the next marker; 0xFE N is also a pair
	for (int reg; (reg = m_input.peek()) != EOF && reg != 0xFF && reg != 0xFD; )
	{
		m_input.ignore(1);
		int data = m_input.get();
		if (data == EOF) break;

		if (reg == 0xFE)
		{
			if ((m_opened = data * 4)) break;
			continue; // 0xFE 0: no frame, the frame continues
		}

		// registers 0x00-0x0F and 0x80-0x8F only
		if ((reg & 0x70) == 0)
		{
			int chip = (reg >> 7);
			ChipRecord record{ uint8_t(reg & 0x0F), uint8_t(data) };
			m_decoder.Decode(chip, record, frame[chip]);
		}
	}
	return true;
}

void DecodePSG::Close(Stream& stream)
{
	m_input.close();

	// the file has no header, the frames tell the chips
	if (stream.IsExpandedModeUsed(0))
		stream.schip.first.model = Chip::Model::AY8930;

	if (stream.IsExpandedModeUsed(1))
		stream.schip.second.model = Chip::Model::AY8930;

	if (stream.IsSecondChipUsed() && !stream.schip.second.modelKnown)
		stream.schip.second.model = stream.schip.first.model;

	stream.Finalize(0, true);
}
