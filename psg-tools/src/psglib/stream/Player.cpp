#include <Windows.h>
#include "Player.h"
#include "output/Output.h"

////////////////////////////////////////////////////////////////////////////////

double GetTime()
{
	LARGE_INTEGER counter, frequency;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&frequency);
	return counter.QuadPart / double(frequency.QuadPart);
}

void WaitFor(const double period)
{
	static double accumulator = 0;
	double start, finish, elapsed;

	accumulator += period;
	finish = (GetTime() + accumulator);

	while (true)
	{
		start = GetTime(); Sleep(1U);
		elapsed = (GetTime() - start);

		if ((accumulator -= elapsed) < 0) return;

		if (accumulator < elapsed)
		{
			start += elapsed;
			while (GetTime() < finish) SwitchToThread();
			accumulator -= (GetTime() - start);
			return;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

Player::Player(Output& output)
	: m_output(output)
	, m_stream(nullptr)
	, m_isPlaying(false)
	, m_isPaused(false)
	, m_frameStep(1.0f)
	, m_frameId(0)
{
	m_output.Open();
}

Player::~Player()
{
	Stop();
	m_output.Close();
}

////////////////////////////////////////////////////////////////////////////////

bool Player::Init(const Stream& stream)
{
	Stop();
	m_isPlaying = false;

	if (stream.framesCount() > 0 && m_output.Init(stream))
	{
		m_stream = &stream;
		m_isPlaying = true;
		m_frameId = 0;
	}

	return m_isPlaying;
}

void Player::Step(float step)
{
	m_frameStep = step;
}

void Player::Play()
{
	if (m_isPaused)
	{
		m_isPaused = false;
		m_playback = std::thread([this] { PlaybackThread(); });
	}
}

void Player::Stop()
{
	if (!m_isPaused)
	{
		m_isPaused = true;
		if (m_playback.joinable()) m_playback.join();
	}
}

bool Player::IsPlaying() const
{
	return m_isPlaying;
}

bool Player::IsPaused() const
{
	return m_isPaused;
}

FrameId Player::GetFrameId() const
{
	return FrameId(m_frameId);
}

////////////////////////////////////////////////////////////////////////////////

void Player::PlaybackThread()
{
	timeBeginPeriod(1U);
	auto hndl = reinterpret_cast<HANDLE>(m_playback.native_handle());
	SetThreadPriority(hndl, THREAD_PRIORITY_TIME_CRITICAL);

	auto frameNextTS = GetTime();
	auto framePeriod = 1.0 / m_stream->play.frameRate();
	bool isPlaying   = m_isPlaying;
	bool firstFrame  = true;

	while (isPlaying && !m_isPaused)
	{
		// play current frame
		if (firstFrame)
		{
			Frame frame(m_stream->play.GetFrame(GetFrameId()));
			if (!m_output.Write(!frame)) isPlaying = false;
			firstFrame = false;
		}
		else
		{
			const Frame& frame = m_stream->play.GetFrame(GetFrameId());
			if (!m_output.Write(frame)) isPlaying = false;
		}

		// next frame timestamp waiting
		frameNextTS += framePeriod;
		WaitFor(frameNextTS - GetTime());

		// go to next frame
		m_frameId = (m_frameId + m_frameStep);
		if (m_frameId > m_stream->play.lastFrameId())
		{
			m_frameId = m_stream->play.lastFrameId();
			isPlaying = false;
		}
		else if (m_frameId < 0)
		{
			m_frameId = 0;
			isPlaying = false;
		}
	}

	// silence output when job is done
	m_output.Write(!Frame());
	m_isPlaying = isPlaying;
	timeEndPeriod(1U);
}
