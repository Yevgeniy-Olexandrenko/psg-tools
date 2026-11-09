#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <Windows.h>

// Plays a ring of blocks on the default sound device (the wave mapper),
// FillBuffer gives the samples of each block. When the device stops taking
// the blocks (it was lost or switched), the audio thread opens it again.
class WaveAudio
{
protected:
	WaveAudio();
	virtual ~WaveAudio();

	bool Open(int sampleRate, int channels, int blocks, int blockSamples);
	void Pause();
	void Resume();
	void Close();

	virtual void FillBuffer(std::vector<float>& buffer) = 0;

private:
	bool OpenDevice();
	void CloseDevice();
	void DriverLoop();

	WAVEFORMATEX m_format{};
	HWAVEOUT m_waveOut{ nullptr };
	HANDLE m_blockDone{ nullptr }; // the device signals it for each played block
	std::vector<std::vector<short>> m_blocks;
	std::vector<WAVEHDR> m_headers;

	std::atomic<bool> m_active{ false };
	std::thread m_thread;
};
