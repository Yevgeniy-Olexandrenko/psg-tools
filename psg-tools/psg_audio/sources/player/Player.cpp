#include "Player.h"
#include "Output.h"

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
	if (stream.framesCount > 0 && m_output.Init(stream))
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
	// the output waits in Write for the time of the next frame
	bool playing = m_playing;
	bool firstFrame = true;
	while (playing && !m_paused)
	{
		// the first frame sets all registers
		const Frame& frame = m_stream->play.GetFrame(GetFrameId());
		if (!m_output.Write(firstFrame ? !Frame(frame) : frame, int(GetFrameId()))) playing = false;
		firstFrame = false;

		// go to next frame
		m_frameId = (m_frameId + m_step);
		if (m_frameId > m_stream->play.lastFrameId)
		{
			m_frameId = float(m_stream->play.lastFrameId);
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
}
