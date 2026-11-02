#pragma once

#include <array>
#include <atomic>
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
	bool Write(const Frame& frame);
	void Close();

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

	// Gets the levels of the frames written after the first 'count' frames,
	// the oldest first, and sets 'count' to the number of written frames.
	// The output keeps the levels of about one second and the spectra of
	// a few frames only.
	void GetLevels(uint32_t& count, std::vector<Levels>& levels) const;
	void GetBands(uint32_t& count, std::vector<Bands>& bands) const;

	std::string toString() const;

protected:
	using Data = std::vector<std::pair<uint8_t, uint8_t>>;
	virtual const std::string GetDeviceName() const = 0;

	virtual bool DeviceOpen() = 0;
	virtual bool DeviceInit(const Stream& stream, Chip& dchip) = 0;
	virtual bool DeviceWrite(int chip, const Data& data) = 0;
	virtual void DeviceClose() = 0;

private:
	Chip m_dchip;
	bool m_alive;
	ProcessingChain m_processing;

	// the player thread writes the levels and the spectra, the GUI reads them
	LevelMeter m_meter;
	std::array<Levels, 64> m_levels;
	std::array<Bands, 16> m_bands;
	std::atomic<uint32_t> m_measuredCount;
};
