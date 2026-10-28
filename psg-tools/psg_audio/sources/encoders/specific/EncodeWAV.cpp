#include "EncodeWAV.h"

bool EncodeWAV::Open(const Stream& stream)
{
	if (CheckFileExt(stream, "wav"))
	{
		// the output does the same frame processing as the player
		m_output.reset(new WaveFile(stream.file));
		m_output->SetInfo(stream);

		if (m_output->Open() && m_output->Init(stream))
		{
			m_frameId = 0;
			return true;
		}
		m_output.reset();
	}
	return false;
}

void EncodeWAV::Encode(const Frame& frame)
{
	if (m_loopFrameId > 0 && m_frameId == m_loopFrameId)
	{
		m_output->SetLoopStart();
	}
	m_output->Write(frame);
	m_frameId++;
}

void EncodeWAV::Close(const Stream& stream)
{
	m_output->Close();
	m_output.reset();
}
