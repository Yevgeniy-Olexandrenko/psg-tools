#pragma once

#include "stream/Frame.h"
#include "stream/Chip.h"

class ChipSim
{
public:
	enum class Type { RP2A03, SN76489, LR35902, AY8910, Hybrid };

	ChipSim(Type type);
	virtual ~ChipSim() = default;

	Type type() const
	{
		return m_type;
	}

	virtual void ConfigureClock(int srcClock, int dstClock)
	{
		m_srcClock = srcClock;
		m_dstClock = dstClock;
	}

	// Returns the simulator of the chip type. The decoder sends it the writes to that chip.
	virtual ChipSim* Find(Type type)
	{
		return (type == m_type ? this : nullptr);
	}

public:
	virtual void Reset() = 0;
	virtual void Write(int chip, Register reg, uint8_t data) = 0;
	virtual void WriteMemory(uint16_t addr, const uint8_t* data, uint32_t size) {}
	virtual void Simulate(int samples) = 0;
	virtual void Convert(Frame& frame) = 0;

protected:
	// converts period by clock ratio and divides it by the given divider,
	// integer math gives exact rounding, done once for both operations
	uint16_t ConvertPeriod(uint32_t period, uint32_t divider = 1) const
	{
		uint64_t num = uint64_t(period) * m_dstClock;
		uint64_t den = uint64_t(m_srcClock) * divider;
		uint64_t converted = (num + den / 2) / den;
		return uint16_t(converted > 0xFFFF ? 0xFFFF : converted);
	}

	void EnableTone(uint8_t& mixer, int chan) const
	{
		mixer &= ~(1 << chan);
	}

	void EnableNoise(uint8_t& mixer, int chan) const
	{
		mixer &= ~(1 << (3 + chan));
	}

	static float Decibels(float ratio);

	// Levels: the output levels of the volumes of the destination chip, full scale 1.
	// The AY8930 has the YM2149 DAC, in the expanded mode with all 32 volumes.
	void ConfigureLevels(Chip::Model model, bool expMode);
	float   VolumeToLevel(uint8_t volume) const;
	uint8_t LevelToVolume(float level) const;
	void    UpdateVolumes(Frame::Registers& regs, const uint8_t* volumes) const;

	// Pulses of the duties 12.5, 25, 50 and 75% (index 0-3), as on the NES and the
	// Game Boy. One chip plays every duty as a 50% square of the same power, two chips
	// add the square an octave up. The AY8930 duties 2-5 are the same duties.
	static const float k_dutyRatios[4];
	static const float k_squareGains[4];
	static const float k_dutyGains[4][2];
	static const uint8_t k_ay8930DutyOffset = 0x02;

	// The AY8930 expanded mode noise toggles after (LFSR & AND | OR) periods. With
	// AND 13 its runs are 1, 4, 5, 8, 9, 12 or 13 periods. With the period of half
	// the source shift, this is close by ear to the white noise of the source chip
	// (NES, Game Boy).
	static const uint8_t k_ay8930NoiseAndMask = 0x0D;

	// Noise: the source LFSR shifts every shift CPU clocks of the source chip.
	// A short LFSR repeats a sequence of steps shifts: a buzz of the source clock
	// / (steps * shift), a tone imitates it while its pitch is audible.
	uint16_t NoisePeriod(uint32_t shift) const;
	bool     PeriodicNoiseAsTone(int steps, uint32_t shift) const;
	uint16_t PeriodicNoisePeriod(int steps, uint32_t shift) const;
	float    PeriodicNoiseGain(int steps, uint32_t shift) const;
	void     UpdateAY8930Noise(Frame::Registers& regs, uint32_t shift, int steps) const;

	// The noise without a channel of its own is mixed in the pulses A and C, which play at
	// the levels and the duties given. Returns the mask of the channels and the gain of their
	// volumes. A noise more than maskingDb quieter than the pulses is masked by them: it plays
	// only for the first attackFrames after a hit (age counts the frames since). The AY8930
	// mixes tone and noise with OR, the others with AND.
	uint8_t  ShareNoise(const float* levels, const float* duties, float target, int age, uint8_t last, bool orMixing, float& gain,
		float maskingDb = 12.f, int attackFrames = 2) const;

protected:
	Type     m_type;
	uint32_t m_srcClock;
	uint32_t m_dstClock;

	const float* m_levels;
	int m_levelsCount;
	int m_levelsStep;
};
