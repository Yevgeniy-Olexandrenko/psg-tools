#include "WaveAudio.h"

#undef min
#undef max

#pragma comment(lib, "winmm.lib")

WaveAudio::WaveAudio()
{
}

WaveAudio::~WaveAudio()
{
	Close();
}

bool WaveAudio::Open(int sampleRate, int channels, int blocks, int blockSamples)
{
	WAVEFORMATEX waveFormat;
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nSamplesPerSec = sampleRate;
	waveFormat.wBitsPerSample = sizeof(Sample) * 8;
	waveFormat.nChannels = channels;
	waveFormat.nBlockAlign = (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	waveFormat.cbSize = 0;

	if (waveOutOpen(&m_waveOut, WAVE_MAPPER, &waveFormat, (DWORD_PTR)WaveAudio::waveOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION) != S_OK)
		return false;

	m_blocks = blocks;
	m_freeBlocks = blocks;
	m_floatBufferSize = (blockSamples * channels);

	m_blockHeader = std::make_unique<WAVEHDR[]>(m_blocks);
	m_blockMemory = std::make_unique<std::vector<Sample>[]>(m_blocks);
	for (size_t i = 0; i < m_blocks; i++)
	{
		m_blockMemory[i].resize(blockSamples * channels, 0);
		m_blockHeader[i].dwBufferLength = DWORD(m_blockMemory[i].size() * sizeof(Sample));
		m_blockHeader[i].lpData = (LPSTR)(m_blockMemory[i].data());
	}

	Resume();
	return true;
}

void WaveAudio::Pause()
{
	m_driverLoopActive = false;
	if (m_driverLoopThread.joinable()) m_driverLoopThread.join();
}

void WaveAudio::Resume()
{
	m_driverLoopActive = true;
	m_driverLoopThread = std::thread(&WaveAudio::DriverLoop, this);
}

void WaveAudio::Close()
{
	Pause();
	waveOutClose(m_waveOut);
}

void WaveAudio::DriverLoop()
{
	constexpr float sampleMax = float(std::numeric_limits<short>::max());
	constexpr float sampleMin = float(std::numeric_limits<short>::min());

	// We will be using this vector to transfer to the host for filling, with 
	// user sound data (float32, -1.0 --> +1.0)
	std::vector<float> floatBuffer(m_floatBufferSize, 0.0f);
	
	// While the system is active, start requesting audio data
	while (m_driverLoopActive)
	{
		// Are there any blocks available to fill? ...
		if (m_freeBlocks == 0)
		{
			// ...no, So wait until one is available
			std::unique_lock<std::mutex> lock(m_blockAvailableMutex);
			while (m_freeBlocks == 0) // sometimes, Windows signals incorrectly
			{
				// This thread will suspend until this CV is signalled
				// from FreeAudioBlock.
				m_blockAvailableConVar.wait(lock);
			}
		}

		// ...yes, so use next one, by indicating one fewer
		// block is available
		m_freeBlocks--;

		// Prepare block for processing, by oddly, marking it as unprepared :P
		if (m_blockHeader[m_currentBlock].dwFlags & WHDR_PREPARED)
		{
			waveOutUnprepareHeader(m_waveOut, &m_blockHeader[m_currentBlock], sizeof(WAVEHDR));
		}

		// Populate a float buffer, that gets cleanly converted to
		// a buffer of shorts for DAC
		FillBuffer(floatBuffer);
		for (size_t i = 0; i < m_floatBufferSize; ++i)
		{
			float sample = (floatBuffer[i] * sampleMax);
			if (sample > sampleMax) sample = sampleMax;
			if (sample < sampleMin) sample = sampleMin;
			m_blockMemory[m_currentBlock][i] = Sample(sample);
		}

		// Send block to sound device
		waveOutPrepareHeader(m_waveOut, &m_blockHeader[m_currentBlock], sizeof(WAVEHDR));
		waveOutWrite(m_waveOut, &m_blockHeader[m_currentBlock], sizeof(WAVEHDR));
		m_currentBlock++;
		m_currentBlock %= m_blocks;
	}
}

void WaveAudio::FreeAudioBlock()
{
	// Audio subsystem is done with the block it was using, thus
	// making it available again
	m_freeBlocks++;

	// Signal to driver loop that a block is now available. It 
	// could have been suspended waiting for one
	std::unique_lock<std::mutex> lock(m_blockAvailableMutex);
	m_blockAvailableConVar.notify_one();
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
