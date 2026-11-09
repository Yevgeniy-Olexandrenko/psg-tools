#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <vector>
#include "processing/ProcessingChain.h"
#include "LevelMeter.h"

class Stream;
class Frame;

class Output
{
public:
	Output();
	virtual ~Output();
	
	bool Open();
	bool Init(const Stream& stream);
	// Writes a frame, the device plays it at its time: the call
	// returns when the device can take the next frame. The position
	// of the frame in the playback goes to GetPosition, -1 keeps
	// the position of the last frame.
	bool Write(const Frame& frame, int position = -1);
	void Close();

	// the position of the frame that sounds now
	int GetPosition() const;

	using Enables = std::array<bool, 5>;
	const Enables& GetEnables() const;
	Enables& GetEnables();

	// the levels and the spectrum of a written frame, see LevelMeter
	struct Levels { uint8_t L, R; };
	struct Bands
	{
		uint8_t levels[LevelMeter::k_bands];
		LevelMeter::Sound sounds[LevelMeter::k_bands];
	};

	// Gets the levels of the frames heard after the first 'count' frames,
	// the oldest first, and sets 'count' to the number of heard frames.
	// The output keeps the levels of about one second and the spectra of
	// a few frames only.
	void GetLevels(uint32_t& count, std::vector<Levels>& levels) const;
	void GetBands(uint32_t& count, std::vector<Bands>& bands) const;

	std::string toString() const;

protected:
	// the register writes of a frame for each chip
	using ChipData  = std::vector<std::pair<uint8_t, uint8_t>>;
	using FrameData = std::array<ChipData, 2>;
	virtual const std::string GetDeviceName() const = 0;

	virtual bool DeviceOpen() = 0;
	virtual bool DeviceInit(const Stream& stream, Chip& dchip) = 0;
	virtual bool DeviceWrite(const FrameData& data) = 0;
	virtual void DeviceClose() = 0;

	// the time from the return of DeviceWrite to the sound of the frame
	virtual double GetDeviceLatency() const { return 0; }

private:
	using Clock = std::chrono::steady_clock;
	uint32_t GetHeardCount() const;

	Chip m_dchip;
	bool m_alive;
	ProcessingChain m_processing;
	FrameData m_data;

	// the player thread writes the times, the levels and the spectra
	// of the frames, the GUI reads them when the frames are heard
	struct Heard { Clock::time_point time; int position; };
	LevelMeter m_meter;
	std::array<Heard, 64> m_heard;
	std::array<Levels, 64> m_levels;
	std::array<Bands, 64> m_bands;
	std::atomic<uint32_t> m_measuredCount;

	Clock::duration m_framePeriod;
	Clock::duration m_latency;
	Clock::time_point m_heardTime;
	int m_position;
};
