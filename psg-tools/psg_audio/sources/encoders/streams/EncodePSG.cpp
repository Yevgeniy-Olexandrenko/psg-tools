#include "EncodePSG.h"

bool EncodePSG::Open(const Stream& stream)
{
	if (CheckFileExt(stream, "psg"))
	{
		m_output.open(stream.file, std::fstream::binary);
		if (m_output)
		{
			m_output << "PSG";
			m_output << uint8_t(0x1A);
			m_output << uint8_t(0x0A);
			m_output << uint8_t(RoundFrameRate(stream.play.frameRate));
			m_output << std::string(10, 0x00);

			m_skip = 0;
			m_encoder.Init(stream.dchip);
			return true;
		}
	}
	return false;
}

void EncodePSG::Encode(const Frame& frame)
{
	m_skip++;
	if (frame.HasChanges())
	{
		WriteFrameBeginOrSkip();

		ChipPacket packet;
		for (int chip = 0; chip < 2; ++chip)
		{
			m_encoder.Encode(chip, frame[chip], packet);
			for (const ChipRecord& record : packet)
			{
				m_output << uint8_t(chip << 7 | record.addr) << record.data;
			}
		}
	}
}

void EncodePSG::Close(const Stream& stream)
{
	WriteFrameBeginOrSkip();
	m_output.close();
}

void EncodePSG::WriteFrameBeginOrSkip()
{
	if (m_skip)
	{
		// one 0xFE command skips up to 255 blocks of 4 frames
		for (int blocks = m_skip / 4; blocks > 0; blocks -= 255)
		{
			m_output << uint8_t(0xFE) << uint8_t(std::min(blocks, 255));
		}
		if (int frames = m_skip % 4) m_output << std::string(frames, uint8_t(0xFF));
		m_skip = 0;
	}
}
