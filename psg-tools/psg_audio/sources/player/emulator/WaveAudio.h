#pragma once

#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <condition_variable>
#include <Windows.h>

class WaveAudio
{
	using Sample = short;

protected:
	WaveAudio();
	virtual ~WaveAudio();

	bool Open(int sampleRate, int channels, int blocks, int blockSamples);
	void Pause();
	void Resume();
	void Close();

	virtual void FillBuffer(std::vector<float>& buffer) = 0;

private:
	void DriverLoop();
	void FreeAudioBlock();
	static void CALLBACK waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2);

private:
	HWAVEOUT m_waveOut{ nullptr };
	
	std::atomic<bool> m_driverLoopActive{ false };
	std::thread m_driverLoopThread;
	uint32_t m_floatBufferSize{ 0 };

	std::unique_ptr<std::vector<Sample>[]> m_blockMemory;
	std::unique_ptr<WAVEHDR[]> m_blockHeader;

	uint32_t m_blocks{ 0 };
	std::atomic<uint32_t> m_freeBlocks{ 0 };
	uint32_t m_currentBlock{ 0 };

	std::condition_variable m_blockAvailableConVar;
	std::mutex m_blockAvailableMutex;
};
