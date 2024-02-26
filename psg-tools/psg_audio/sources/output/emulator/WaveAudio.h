#pragma once

#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <condition_variable>

#define _WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef min
#undef max

class WaveAudio
{
	const uint32_t nBlocks = 8;
	const uint32_t nBlockSamples = 512;

	int _GetBlocks() { return nBlocks; }
	int _GetBlockSampleCount() { return nBlockSamples; }

protected:
	WaveAudio();
	virtual ~WaveAudio();

	bool Open(int sampleRate, int frameRate, int sampleChannels, int sampleBytes);
	bool Start();
	void Stop();
	void Close();

	virtual void FillBuffer(unsigned char* buffer, unsigned long size) = 0;

private:
	void DriverLoop();
	void FreeAudioBlock();
	static void CALLBACK waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2);

private:
	HWAVEOUT m_hwDevice = nullptr;
	std::thread m_thDriverLoop;
	std::atomic<bool> m_bDriverLoopActive{ false };
	std::unique_ptr<std::vector<short>[]> m_pvBlockMemory;
	std::unique_ptr<WAVEHDR[]> m_pWaveHeaders;
	std::atomic<unsigned int> m_nBlockFree = 0;
	std::condition_variable m_cvBlockNotZero;
	std::mutex m_muxBlockNotZero;
	uint32_t m_nBlockCurrent = 0;
};

//#pragma once
//
//#include <mutex>
//#include <windows.h>
//
//class WaveAudio
//{
//protected:
//	WaveAudio();
//	virtual ~WaveAudio();
//
//	bool Open(int sampleRate, int frameRate, int sampleChannels, int sampleBytes);
//	void Close();
//
//	virtual void FillBuffer(unsigned char* buffer, unsigned long size) = 0;
//
//private:
//	void OnBufferDone(WAVEHDR* hdr);
//	static void CALLBACK WaveOutProc(
//		HWAVEOUT hwo, UINT uMsg,
//		DWORD_PTR dwInstance,
//		DWORD_PTR dwParam1,
//		DWORD_PTR dwParam2
//	);
//
//protected:
//	HWAVEOUT m_waveout;
//	WAVEFORMATEX m_format;
//	WAVEHDR m_buffers[4];
//	std::mutex m_mutex;
//};
