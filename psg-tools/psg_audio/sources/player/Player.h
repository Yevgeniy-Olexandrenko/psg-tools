#pragma once

#include <thread>
#include <atomic>
#include "stream/Stream.h"

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
	void Playback();

private:
	Output& m_output;
	const Stream* m_stream;
	std::thread m_thread;

	std::atomic<bool> m_playing;
	std::atomic<bool> m_paused;

	std::atomic<float> m_step;
	std::atomic<float> m_frameId;
};