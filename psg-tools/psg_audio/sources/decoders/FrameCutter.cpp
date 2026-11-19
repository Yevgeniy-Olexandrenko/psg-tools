#include <algorithm>
#include "FrameCutter.h"

namespace
{
	const int k_sampleRate = 44100;

	// A faster rate is a digital sound, which frames cannot keep.
	const int k_maxFrameRate = 200;
}

FrameCutter::FrameCutter(int frameTime)
	: m_frameTime(frameTime)
	, m_longPause(frameTime / 4)
	, m_noUpdate(frameTime + frameTime / 2)
	, m_time(0)
	, m_written(false)
	, m_sinceWrite(0)
	, m_sinceFrame(frameTime)
	, m_burstTime(0)
	, m_updateTime(-1)
	, m_updates(0)
	, m_updateTimes(2 * frameTime, 0)
{
}

void FrameCutter::OnWrite()
{
	if (!m_written) m_burstTime = m_time;
	m_written = true;
	m_sinceWrite = 0;
}

bool FrameCutter::IsFrameEnd(int wait)
{
	bool update = (m_written && m_sinceWrite + wait >= m_longPause);
	if (update && m_sinceFrame < m_longPause) update = m_written = false;
	if (!update && m_sinceFrame < m_noUpdate) return false;

	// count the times between the updates in a row, from the start of
	// a burst: the player starts it at the frame, its length varies
	if (update && m_updateTime >= 0 && m_burstTime - m_updateTime <= m_noUpdate)
	{
		m_updateTimes[m_burstTime - m_updateTime]++;
		m_updates++;
	}
	m_updateTime = (update ? m_burstTime : -1);
	m_sinceFrame = (update ? 0 : m_sinceFrame - m_frameTime);
	m_written = false;
	return true;
}

void FrameCutter::EndFrame(int sinceFrame)
{
	m_updateTime = -1;
	m_sinceFrame = sinceFrame;
	m_written = false;
}

int FrameCutter::Advance(int wait)
{
	int samples = std::min(wait, m_noUpdate - m_sinceFrame);
	m_sinceWrite += samples;
	m_sinceFrame += samples;
	m_time += samples;
	return samples;
}

// The most common time between the updates in a row is the frame time, if most of
// the updates come at about this time. The mean of these times, which vary by a few
// samples, gives the real frame rate instead of the nominal one.
bool FrameCutter::GetFrameRate(FrameRate& frameRate) const
{
	int mode = int(std::max_element(m_updateTimes.begin(), m_updateTimes.end()) - m_updateTimes.begin());
	int count = 0;
	double time = 0;
	for (int t = std::max(mode - 2, 0); t <= mode + 2 && t < int(m_updateTimes.size()); ++t)
	{
		count += m_updateTimes[t];
		time += double(t) * m_updateTimes[t];
	}
	if (count >= 50 && 2 * count >= m_updates && time * k_maxFrameRate >= double(k_sampleRate) * count)
	{
		frameRate = (k_sampleRate * count / time);
		return true;
	}
	return false;
}
