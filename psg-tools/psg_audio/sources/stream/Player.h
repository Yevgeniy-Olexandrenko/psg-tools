#pragma once

#include <thread>
#include <atomic>
#include "Stream.h"

class Output;

class Player
{
public:
	Player(Output& output);
	~Player();

public:
	bool Init(const Stream& stream);
	void Step(float step);
	void Play();
	void Stop();

	bool IsPlaying() const;
	bool IsPaused() const;
	FrameId GetFrameId() const;
	
private:
	void PlaybackThread();

private:
	Output& m_output;
	const Stream* m_stream;
	std::thread m_playback;

	std::atomic<bool> m_isPlaying;
	std::atomic<bool> m_isPaused;

	std::atomic<float> m_frameStep;
	std::atomic<float> m_frameId;
};