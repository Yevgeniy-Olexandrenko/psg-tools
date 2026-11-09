#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include "player/PCMOutput.h"
#include "WaveAudio.h"

// Plays the frames on the clock of the sound device. Write puts a frame in
// a short queue, or waits while the queue is full; the audio thread takes
// the next frame every 'sample rate / frame rate' samples.
class Emulator : public PCMOutput, public WaveAudio
{
public:
	Emulator();
	virtual ~Emulator();

protected:
	const std::string GetDeviceName() const override;

	bool DeviceOpen() override;
	bool DeviceInit(const Stream& stream, Chip& dchip) override;
	bool DeviceWrite(const FrameData& data) override;
	void FillBuffer(std::vector<float>& buffer) override;
	void DeviceClose() override;
	double GetDeviceLatency() const override;

private:
	std::deque<FrameData> m_queue;
	std::mutex m_mutex;
	std::condition_variable m_queueNotFull;
	bool m_active;

	double m_samplesPerFrame;
	double m_samplesToFrame; // before the next frame
};
