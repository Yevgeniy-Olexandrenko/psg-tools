#include "WaveAudio.h"

#pragma comment(lib, "winmm.lib")

namespace
{
	// a block plays for a few ms: when the oldest one does not come back
	// for this time, the device is lost or switched
	constexpr DWORD k_stallTimeMs = 500;
}

WaveAudio::WaveAudio()
	: m_blockDone(CreateEvent(nullptr, FALSE, FALSE, nullptr))
{
}

WaveAudio::~WaveAudio()
{
	Close();
	CloseHandle(m_blockDone);
}

bool WaveAudio::Open(int sampleRate, int channels, int blocks, int blockSamples)
{
	Close();

	m_format.wFormatTag = WAVE_FORMAT_PCM;
	m_format.nChannels = WORD(channels);
	m_format.nSamplesPerSec = DWORD(sampleRate);
	m_format.wBitsPerSample = 16;
	m_format.nBlockAlign = WORD(channels * sizeof(short));
	m_format.nAvgBytesPerSec = DWORD(sampleRate * m_format.nBlockAlign);
	m_format.cbSize = 0;

	m_blocks.assign(blocks, std::vector<short>(size_t(blockSamples) * channels));
	m_headers.assign(blocks, WAVEHDR{});
	for (int i = 0; i < blocks; ++i)
	{
		m_headers[i].lpData = LPSTR(m_blocks[i].data());
		m_headers[i].dwBufferLength = DWORD(m_blocks[i].size() * sizeof(short));
	}

	if (!OpenDevice()) return false;
	Resume();
	return true;
}

void WaveAudio::Pause()
{
	m_active = false;
	SetEvent(m_blockDone);
	if (m_thread.joinable()) m_thread.join();
}

void WaveAudio::Resume()
{
	if (!m_thread.joinable())
	{
		m_active = true;
		m_thread = std::thread(&WaveAudio::DriverLoop, this);
	}
}

void WaveAudio::Close()
{
	Pause();
	CloseDevice();
}

bool WaveAudio::OpenDevice()
{
	if (waveOutOpen(&m_waveOut, WAVE_MAPPER, &m_format, DWORD_PTR(m_blockDone), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR)
	{
		m_waveOut = nullptr;
		return false;
	}
	for (WAVEHDR& header : m_headers)
	{
		header.dwFlags = 0;
		waveOutPrepareHeader(m_waveOut, &header, sizeof(WAVEHDR));
	}
	return true;
}

void WaveAudio::CloseDevice()
{
	if (m_waveOut)
	{
		// take the queued blocks back, else the device cannot close
		waveOutReset(m_waveOut);
		for (WAVEHDR& header : m_headers)
		{
			waveOutUnprepareHeader(m_waveOut, &header, sizeof(WAVEHDR));
		}
		waveOutClose(m_waveOut);
		m_waveOut = nullptr;
	}
}

void WaveAudio::DriverLoop()
{
	std::vector<float> buffer(m_blocks[0].size());
	size_t next = 0;

	while (m_active)
	{
		// the device plays the blocks in order: wait for the oldest one
		WAVEHDR& header = m_headers[next];
		if (!m_waveOut || (header.dwFlags & WHDR_INQUEUE))
		{
			if (WaitForSingleObject(m_blockDone, k_stallTimeMs) == WAIT_TIMEOUT)
			{
				CloseDevice();
				OpenDevice();
			}
			continue;
		}

		FillBuffer(buffer);
		std::vector<short>& block = m_blocks[next];
		for (size_t i = 0; i < block.size(); ++i)
		{
			float sample = (buffer[i] < -1.f ? -1.f : buffer[i] > 1.f ? 1.f : buffer[i]);
			block[i] = short(sample * 32767.f);
		}

		header.dwFlags &= ~WHDR_DONE;
		if (waveOutWrite(m_waveOut, &header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			CloseDevice();
			OpenDevice();
		}
		next = (next + 1) % m_headers.size();
	}
}
