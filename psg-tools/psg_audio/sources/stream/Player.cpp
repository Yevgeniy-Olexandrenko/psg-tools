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
	, m_playing(false)
	, m_paused(false)
	, m_step(1.f)
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
	Step(1.f);

	m_playing = false;
	if (stream.framesCount() > 0 && m_output.Init(stream))
	{
		m_stream  = &stream;
		m_playing = true;
		m_frameId = 0;
	}
	return m_playing;
}

void Player::Step(float step)
{
	m_step = step;
}

void Player::Play()
{
	if (m_paused)
	{
		m_paused = false;
		m_thread = std::thread([this] { Playback(); });
	}
}

void Player::Stop()
{
	if (!m_paused)
	{
		m_paused = true;
		if (m_thread.joinable()) m_thread.join();
	}
}

bool Player::IsPlaying() const
{
	return m_playing;
}

bool Player::IsPaused() const
{
	return m_paused;
}

FrameId Player::GetFrameId() const
{
	return FrameId(m_frameId);
}

////////////////////////////////////////////////////////////////////////////////

void Player::Playback()
{
	timeBeginPeriod(1U);
	auto hndl = reinterpret_cast<HANDLE>(m_thread.native_handle());
	SetThreadPriority(hndl, THREAD_PRIORITY_TIME_CRITICAL);

	auto firstFrame  = true;
	auto frameNextTS = GetTime();
	auto framePeriod = 1.0 / m_stream->play.frameRate();

	bool playing = m_playing;
	while (playing && !m_paused)
	{
		// play current frame
		if (firstFrame)
		{
			Frame frame(m_stream->play.GetFrame(GetFrameId()));
			if (!m_output.Write(!frame)) playing = false;
			firstFrame = false;
		}
		else
		{
			const Frame& frame = m_stream->play.GetFrame(GetFrameId());
			if (!m_output.Write(frame)) playing = false;
		}

		// next frame timestamp waiting
		frameNextTS += framePeriod;
		WaitFor(frameNextTS - GetTime());

		// go to next frame
		m_frameId = (m_frameId + m_step);
		if (m_frameId > m_stream->play.lastFrameId())
		{
			m_frameId = float(m_stream->play.lastFrameId());
			playing = false;
		}
		else if (m_frameId < 0)
		{
			m_frameId = 0;
			playing = false;
		}
	}

	// silence output when job is done
	m_output.Write(!Frame());
	m_playing = playing;
	timeEndPeriod(1U);
}
