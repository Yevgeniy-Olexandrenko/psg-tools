#include <cmath>
#include "SimAY8910.h"

namespace
{
	const int k_sampleRate = 44100;
	const float k_pi = 3.14159265f;

	// The writes of a volume make a sound of their own when there are at least
	// this many of them, spread over at least half of the frame.
	const int k_minSoundWrites = 3;

	// A tone above this frequency is not heard as a tone of the gate:
	// the volume is the mean level, as for a digital sound.
	const int k_maxToneFrequency = 5000;

	// The integral of the tone (high in the first half of its period)
	// times exp(-i * angle) from the start of the period up to the angle.
	std::complex<float> ToneIntegral(float angle)
	{
		const std::complex<float> i(0, 1);
		return (angle < k_pi ? i * (std::exp(-i * angle) - 1.f) : -2.f * i);
	}
}

SimAY8910::SimAY8910()
	: ChipSim(Type::AY8910)
{
	ConfigureLevels(Chip::Model::AY8910, false);
	Reset();
}

void SimAY8910::ConfigureClock(int srcClock, int dstClock)
{
	ChipSim::ConfigureClock(srcClock, dstClock);

	// always process both chips: an unused second chip holds only
	// zeros, they stay zeros after conversion and are never changed
	m_convert.reset(new ChipClockRateConvert(srcClock, dstClock, 2));
}

void SimAY8910::Reset()
{
	m_frame.ResetData();
	m_frame.ResetChanges();
	m_decoder.Init();
	for (auto& chip : m_channels)
	{
		for (Channel& channel : chip) channel.phase = 0;
	}
	m_clockRest = 0;
	ResetFrame();
}

void SimAY8910::ResetFrame()
{
	for (auto& chip : m_channels)
	{
		for (Channel& channel : chip)
		{
			channel.writes = 0;
			channel.envelope = false;
			channel.firstWrite = channel.lastWrite = 0;
			channel.output = channel.reference = 0;
			channel.levelSum = channel.time = 0;
		}
	}
}

void SimAY8910::Write(int chip, Register reg, uint8_t data)
{
	m_decoder.Decode(chip, { reg, data }, m_frame[chip]);

	const Frame::Registers& regs = m_frame[chip];
	for (int chan = 0; chan < 3; ++chan)
	{
		if (reg == Register::volume[chan] && !regs.IsExpMode())
		{
			Channel& channel = m_channels[chip][chan];
			if (!channel.writes++) channel.firstWrite = channel.time;
			channel.lastWrite = channel.time;
			if (data & 0x10) channel.envelope = true;
		}
	}
}

uint8_t SimAY8910::Read(int chip, Register reg)
{
	return m_decoder.Read(chip, reg, m_frame[chip]);
}

void SimAY8910::Simulate(int samples)
{
	// the clocks of the chip in the samples, the rest goes to the next time
	m_clockRest += uint64_t(samples) * m_srcClock;
	uint32_t clocks = uint32_t(m_clockRest / k_sampleRate);
	m_clockRest %= k_sampleRate;
	if (!clocks) return;

	for (int chip = 0; chip < 2; ++chip)
	{
		const Frame::Registers& regs = m_frame[chip];
		for (int chan = 0; chan < 3; ++chan)
		{
			// a period of the tone is 16 clocks of its period register
			Channel& channel = m_channels[chip][chan];
			uint32_t period = 16 * std::max<uint32_t>(regs.Read(Register::t_period[chan]), 1);
			float level = VolumeToLevel(regs.GetVolume(chan) & 0x0F);
			uint32_t start = channel.phase % period;

			// the integral over the angle of the tone, in the units of a period / 2pi
			uint64_t end = uint64_t(start) + clocks;
			float scale = float(period) / (2 * k_pi);
			std::complex<float> integral = float(end / period) * ToneIntegral(2 * k_pi);
			integral += ToneIntegral(2 * k_pi * float(end % period) / period);
			integral -= ToneIntegral(2 * k_pi * float(start) / period);
			integral *= scale;

			channel.output += level * integral;
			channel.reference += integral;
			channel.levelSum += level * clocks;
			channel.time += float(clocks);
			channel.phase = uint32_t(end % period);
		}
	}
}

uint8_t SimAY8910::FrameVolume(int chip, int chan) const
{
	const Frame::Registers& regs = m_frame[chip];
	const Channel& channel = m_channels[chip][chan];
	uint32_t period = 16 * std::max<uint32_t>(regs.Read(Register::t_period[chan]), 1);

	float level;
	if (regs.IsToneEnabled(chan) && period * k_maxToneFrequency >= m_srcClock && std::abs(channel.reference) > 0)
	{
		level = std::abs(channel.output) / std::abs(channel.reference);
	}
	else
	{
		level = (channel.time > 0 ? channel.levelSum / channel.time : 0);
	}
	return LevelToVolume(level);
}

void SimAY8910::Convert(Frame& frame)
{
	// The writes of a volume over the frame: the volume of the same loudness.
	// The frame of the chip keeps the last write, as the reads need it.
	Frame source = m_frame;
	for (int chip = 0; chip < 2; ++chip)
	{
		for (int chan = 0; chan < 3; ++chan)
		{
			const Channel& channel = m_channels[chip][chan];
			bool sound = (channel.writes >= k_minSoundWrites && 2 * (channel.lastWrite - channel.firstWrite) >= channel.time);
			if (sound && !channel.envelope && !m_frame[chip].IsExpMode())
			{
				source[chip].Update(Register::volume[chan], FrameVolume(chip, chan));
			}
		}
	}
	ResetFrame();

	// convert the source clock rate to the closest one supported by chip,
	// the changes in the converted frame are not reliable, but += takes
	// only its data and recomputes the changes against the previous frame
	frame += m_convert->Execute(source);
	m_frame.ResetChanges();
}
