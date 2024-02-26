#include "WaveAudio.h"
#include <algorithm>

#pragma comment(lib, "winmm.lib")


//#include "WaveAudio.h"
//#include <mmsystem.h>
//
//#pragma comment(lib, "winmm.lib")
//
//WaveAudio::WaveAudio()
//	: m_waveout(NULL)
//	, m_buffers{}
//	, m_format{}
//{
//}
//
//WaveAudio::~WaveAudio()
//{
//	Close();
//}
//
//bool WaveAudio::Open(int sampleRate, int frameRate, int sampleChannels, int sampleBytes)
//{
//	if (!m_waveout)
//	{
//		int samplesPerFrame = sampleRate / frameRate;
//		int bytesPerSample = sampleBytes * sampleChannels;
//
//		// create buffers
//		for (int i = 0; i < 4; ++i)
//		{
//			memset(&m_buffers[i], 0, sizeof(WAVEHDR));
//			m_buffers[i].dwBufferLength = samplesPerFrame * bytesPerSample;
//			m_buffers[i].lpData = (char*)malloc(samplesPerFrame * bytesPerSample);
//			memset(m_buffers[i].lpData, 0, m_buffers[i].dwBufferLength);
//		}
//
//		// init audio format
//		memset(&m_format, 0, sizeof(WAVEFORMATEX));
//		m_format.wFormatTag = WAVE_FORMAT_PCM;
//		m_format.nSamplesPerSec = sampleRate;
//		m_format.wBitsPerSample = (bytesPerSample / sampleChannels) * 8;
//		m_format.nChannels = sampleChannels;
//		m_format.nBlockAlign = bytesPerSample;
//		m_format.cbSize = 0;
//
//		// open audio device
//		HWAVEOUT hwo = NULL;
//		MMRESULT res = waveOutOpen(&hwo, WAVE_MAPPER, &m_format, (DWORD_PTR)WaveAudio::WaveOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
//		if (res != MMSYSERR_NOERROR) return false;
//
//		// assign all available buffers to audio playback
//		for (int i = 0; i < 4; ++i)
//		{
//			MMRESULT res = waveOutPrepareHeader(hwo, &m_buffers[i], sizeof(WAVEHDR));
//			if (res != MMSYSERR_NOERROR) return false;
//
//			res = waveOutWrite(hwo, &m_buffers[i], sizeof(WAVEHDR));
//			if (res != MMSYSERR_NOERROR) return false;
//		}
//
//		// everything if fine!
//		m_waveout = hwo;
//	}
//	return (m_waveout != NULL);
//}
//
//void WaveAudio::Close()
//{
//	if (m_waveout)
//	{
//		HWAVEOUT hwo = m_waveout;
//		m_waveout = NULL;
//
//		std::lock_guard<std::mutex> lock(m_mutex);
//		MMRESULT res = waveOutReset(hwo);
//
//		for (int i = 0; i < 4; ++i)
//		{
//			MMRESULT res = waveOutUnprepareHeader(hwo, &m_buffers[i], sizeof(WAVEHDR));
//			free(m_buffers[i].lpData);
//		}
//
//		waveOutClose(hwo);
//	}
//}
//
//void WaveAudio::OnBufferDone(WAVEHDR* hdr)
//{
//	if (m_waveout)
//	{
//		std::lock_guard<std::mutex> lock(m_mutex);
//		FillBuffer((unsigned char*)hdr->lpData, hdr->dwBufferLength);
//		hdr->dwFlags &= ~WHDR_DONE;
//		MMRESULT res = waveOutWrite(m_waveout, hdr, sizeof(WAVEHDR));
//	}
//}
//
//void WaveAudio::WaveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
//{
//	if (uMsg == WOM_DONE) 
//	{
//		((WaveAudio*)dwInstance)->OnBufferDone((WAVEHDR*)dwParam1);
//	}
//}

WaveAudio::WaveAudio()
{
}

WaveAudio::~WaveAudio()
{
	Close();
}

bool WaveAudio::Open(int sampleRate, int frameRate, int sampleChannels, int sampleBytes)
{
	// Device is available
	WAVEFORMATEX waveFormat;
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nSamplesPerSec = sampleRate;
	waveFormat.wBitsPerSample = sizeof(short) * 8;
	waveFormat.nChannels = sampleChannels;
	waveFormat.nBlockAlign = (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	waveFormat.cbSize = 0;

	// Open Device if valid
	if (waveOutOpen(&m_hwDevice, WAVE_MAPPER, &waveFormat, (DWORD_PTR)WaveAudio::waveOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION) != S_OK)
		return false;

	// Allocate array of wave header objects, one per block
	m_pWaveHeaders = std::make_unique<WAVEHDR[]>(_GetBlocks());

	// Allocate block memory - I dont like vector of vectors, so going with this mess instead
	// My std::vector's content will change, but their size never will - they are basically array now
	m_pvBlockMemory = std::make_unique<std::vector<short>[]>(_GetBlocks());
	for (size_t i = 0; i < _GetBlocks(); i++)
		m_pvBlockMemory[i].resize(_GetBlockSampleCount() * sampleChannels, 0);

	// Link headers to block memory - clever, so we only move headers about
	// rather than memory...
	for (unsigned int n = 0; n < _GetBlocks(); n++)
	{
		m_pWaveHeaders[n].dwBufferLength = DWORD(m_pvBlockMemory[0].size() * sizeof(short));
		m_pWaveHeaders[n].lpData = (LPSTR)(m_pvBlockMemory[n].data());
	}

	// To begin with, all blocks are free
	m_nBlockFree = _GetBlocks();

	Start();
	return true;
}

bool WaveAudio::Start()
{
	// Prepare driver thread for activity
	m_bDriverLoopActive = true;

	// and get it going!
	m_thDriverLoop = std::thread(&WaveAudio::DriverLoop, this);
	return true;
}

void WaveAudio::Stop()
{
	// Signal the driver loop to exit
	m_bDriverLoopActive = false;

	// Wait for driver thread to exit gracefully
	if (m_thDriverLoop.joinable())
		m_thDriverLoop.join();
}

void WaveAudio::Close()
{
	Stop();
	waveOutClose(m_hwDevice);
}

void WaveAudio::DriverLoop()
{
	// We will be using this vector to transfer to the host for filling, with 
	// user sound data (float32, -1.0 --> +1.0)
//	std::vector<float> vFloatBuffer(m_pHost->GetBlockSampleCount() * m_pHost->GetChannels(), 0.0f);

	// While the system is active, start requesting audio data
	while (m_bDriverLoopActive)
	{
		// Are there any blocks available to fill? ...
		if (m_nBlockFree == 0)
		{
			// ...no, So wait until one is available
			std::unique_lock<std::mutex> lm(m_muxBlockNotZero);
			while (m_nBlockFree == 0) // sometimes, Windows signals incorrectly
			{
				// This thread will suspend until this CV is signalled
				// from FreeAudioBlock.
				m_cvBlockNotZero.wait(lm);
			}
		}

		// ...yes, so use next one, by indicating one fewer
		// block is available
		m_nBlockFree--;

		// Prepare block for processing, by oddly, marking it as unprepared :P
		if (m_pWaveHeaders[m_nBlockCurrent].dwFlags & WHDR_PREPARED)
		{
			waveOutUnprepareHeader(m_hwDevice, &m_pWaveHeaders[m_nBlockCurrent], sizeof(WAVEHDR));
		}

		// Give the userland the opportunity to fill the buffer. Note that the driver
		// doesnt give a hoot about timing. Thats up to the SoundWave interface to 
		// maintain

		// Userland will populate a float buffer, that gets cleanly converted to
		// a buffer of shorts for DAC
//		ProcessOutputBlock(vFloatBuffer, m_pvBlockMemory[m_nBlockCurrent]);
		FillBuffer((unsigned char*)m_pWaveHeaders[m_nBlockCurrent].lpData, m_pWaveHeaders[m_nBlockCurrent].dwBufferLength);

		// Send block to sound device
		waveOutPrepareHeader(m_hwDevice, &m_pWaveHeaders[m_nBlockCurrent], sizeof(WAVEHDR));
		waveOutWrite(m_hwDevice, &m_pWaveHeaders[m_nBlockCurrent], sizeof(WAVEHDR));
		m_nBlockCurrent++;
		m_nBlockCurrent %= _GetBlocks();
	}
}

void WaveAudio::FreeAudioBlock()
{
	// Audio subsystem is done with the block it was using, thus
	// making it available again
	m_nBlockFree++;

	// Signal to driver loop that a block is now available. It 
	// could have been suspended waiting for one
	std::unique_lock<std::mutex> lm(m_muxBlockNotZero);
	m_cvBlockNotZero.notify_one();
}

void WaveAudio::waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	// All sorts of messages may be pinged here, but we're only interested
	// in audio block is finished...
	if (uMsg != WOM_DONE) return;

	// ...which has happened so allow driver object to free resource
	WaveAudio* driver = (WaveAudio*)dwInstance;
	driver->FreeAudioBlock();
}
