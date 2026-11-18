#include "ChipSim.h"
#include <algorithm>
#include <cmath>

namespace
{
	// output levels of the YM2149 32 volumes, its 16 volumes are the odd ones
	const float c_ymLevels[] =
	{
		0.000f, 0.000f, 0.005f, 0.008f, 0.011f, 0.014f, 0.017f, 0.020f,
		0.024f, 0.030f, 0.035f, 0.040f, 0.049f, 0.058f, 0.068f, 0.078f,
		0.093f, 0.111f, 0.130f, 0.148f, 0.177f, 0.212f, 0.246f, 0.281f,
		0.334f, 0.400f, 0.467f, 0.534f, 0.635f, 0.758f, 0.880f, 1.000f
	};

	// output levels of the AY8910 16 volumes
	const float c_ayLevels[] =
	{
		0.000f, 0.010f, 0.014f, 0.021f, 0.031f, 0.046f, 0.064f, 0.107f,
		0.127f, 0.205f, 0.292f, 0.373f, 0.493f, 0.635f, 0.806f, 1.000f
	};

	// the noise shares the channels of the pulses A and C
	const uint8_t c_pulseMasks[2] = { 1 << Frame::Channel::A, 1 << Frame::Channel::C };

	// mixing the noise in a pulse halves its tone (-6 dB): the loss counts in the cost
	// of a choice with this weight, scaled by the pulse level relative to the louder one
	const float c_toneLossDb = 6.f;
	const float c_toneLossWeight = 0.5f;

	// the noise stays in its channels while another choice costs not less by this
	const float c_noiseHysteresisDb = 1.f;

	const double c_pi = 3.14159265358979;

	// the upper frequency of hearing, Hz
	const double c_hearingLimit = 20000;
}

const float ChipSim::k_dutyRatios[4] = { 0.125f, 0.25f, 0.5f, 0.75f };

// levels of the square of one chip for the duties: its 50% duty has
// the same power as the pulse, 2 * sqrt(d * (1 - d))
const float ChipSim::k_squareGains[4] = { 0.661f, 0.866f, 1.000f, 0.866f };

// levels of the square and of its octave on two chips for the duties:
// (a^2 + b^2) / 4 = d * (1 - d), b = 0.707 * a for 25% and 75%, b = a for 12.5%
const float ChipSim::k_dutyGains[4][2] =
{
	{ 0.468f, 0.468f }, { 0.707f, 0.500f }, { 1.000f, 0.000f }, { 0.707f, 0.500f }
};

ChipSim::ChipSim(Type type)
	: m_type(type)
	, m_srcClock(1)
	, m_dstClock(1)
{
	ConfigureLevels(Chip::Model::YM2149, false);
}

float ChipSim::Decibels(float ratio)
{
	return 20.f * std::log10(std::max(ratio, 1e-6f));
}

void ChipSim::ConfigureLevels(Chip::Model model, bool expMode)
{
	bool ay8910 = (!expMode && model == Chip::Model::AY8910);
	m_levels = (ay8910 ? c_ayLevels : c_ymLevels);
	m_levelsCount = (expMode ? 32 : 16);
	m_levelsStep = (ay8910 || expMode ? 1 : 2);
}

float ChipSim::VolumeToLevel(uint8_t volume) const
{
	volume = std::min(volume, uint8_t(m_levelsCount - 1));
	return m_levels[volume * m_levelsStep + m_levelsStep - 1];
}

uint8_t ChipSim::LevelToVolume(float level) const
{
	// the nearest level in decibels among the ones the chip has,
	// a level nearer to silence than to the lowest one is silent
	uint8_t volume = 0;
	float lowest = 0, error = 0;
	for (uint8_t i = 1; i < m_levelsCount; ++i)
	{
		float output = VolumeToLevel(i);
		if (output <= 0) continue;
		if (!lowest) lowest = output;

		float e = std::abs(Decibels(level / output));
		if (!volume || e < error) { volume = i; error = e; }
	}
	return (level < lowest / 2 ? 0 : volume);
}

void ChipSim::UpdateVolumes(Frame::Registers& regs, const uint8_t* volumes) const
{
	// a channel with tone and noise disabled outputs its volume as a constant
	// level, so the volume of a channel without sound must be zeroed
	for (int chan = 0; chan < 3; ++chan)
	{
		regs.Update(Register::volume[chan], volumes[chan]);
	}
}

uint16_t ChipSim::NoisePeriod(uint32_t shift) const
{
	// the PSG noise shifts every 16 * period chip clocks
	return std::min(ConvertPeriod(shift, 16), uint16_t(0x1F));
}

bool ChipSim::PeriodicNoiseAsTone(int steps, uint32_t shift) const
{
	// lower than the longest tone period, the buzz is a rattle, closer to the usual noise
	return (PeriodicNoisePeriod(steps, shift) <= 0x0FFF);
}

uint16_t ChipSim::PeriodicNoisePeriod(int steps, uint32_t shift) const
{
	return ConvertPeriod(steps * shift, 16);
}

float ChipSim::PeriodicNoiseGain(int steps, uint32_t shift) const
{
	// The steps of the sequence put most of its power above hearing, the square of
	// the same pitch has nearly all of it below: they are leveled by the power heard.
	// The harmonics of the sequence are equal but for the sinc of a step.
	double pitch = double(m_srcClock) / (steps * shift);
	double sequence = 0, square = 0;
	for (int k = 1; k * pitch < c_hearingLimit; ++k)
	{
		double x = c_pi * k / steps;
		sequence += (std::sin(x) / x) * (std::sin(x) / x);
		if (k & 1) square += 1.0 / (k * k);
	}
	sequence /= (steps - 1) / 2.0; // of the power of all the harmonics
	square *= 8 / (c_pi * c_pi);
	return float(std::sqrt(sequence / square));
}

void ChipSim::UpdateAY8930Noise(Frame::Registers& regs, uint32_t shift, int steps) const
{
	// the expanded mode noise toggles its output after (LFSR & AND | OR) periods of 8 clocks
	if (steps)
	{
		// The AND mask 0 makes it a square of half period OR * period. The half period
		// of the buzz, steps * shift / 2 source clocks, takes the pair of the least error.
		double target = double(steps) * shift * m_dstClock / (16.0 * m_srcClock);
		double error = target;
		uint8_t orMask = 1, period = 1;
		for (int mask = 1; mask <= 0xFF; ++mask)
		{
			int p = int(target / mask + 0.5);
			if (p < 1 || p > 0xFF) continue;
			if (std::abs(mask * p - target) < error)
			{
				error = std::abs(mask * p - target);
				orMask = uint8_t(mask);
				period = uint8_t(p);
			}
		}
		regs.Update(Register::N_AndMask, 0x00);
		regs.Update(Register::N_OrMask, orMask);
		regs.Update(Register::Period::N, period);
	}
	else
	{
		// the period of 8 * period chip clocks is half the source shift
		uint16_t period = ConvertPeriod(shift, 16);
		regs.Update(Register::N_AndMask, k_ay8930NoiseAndMask);
		regs.Update(Register::N_OrMask, 0x00);
		regs.Update(Register::Period::N, std::min(period, uint16_t(0xFF)));
	}
}

uint8_t ChipSim::ShareNoise(const float* levels, const float* duties, float target, int age, uint8_t last, bool orMixing, float& gain,
	float maskingDb, int attackFrames) const
{
	gain = 1;
	// a noise masked by the pulses is mixed in for its attack only
	float masker = std::sqrt(levels[0] * levels[0] + levels[1] * levels[1]);
	if (age >= attackFrames && Decibels(masker / target) > maskingDb) return 0;

	// t AND n (YM) or t OR n (AY8930) of a tone of duty d and the noise keeps half of the
	// tone; the noise parts g * L (g = d or 1 - d) add as one noise over the channels, the
	// products with the tones do not correlate and add d * (1 - d) * L^2 in power each
	const auto NoiseLevelOf = [&](uint8_t mask)
	{
		float parts = 0, products = 0;
		for (int i = 0; i < 2; ++i)
		{
			if (!(mask & c_pulseMasks[i])) continue;
			float d = duties[i];
			parts += (orMixing ? 1 - d : d) * levels[i];
			products += d * (1 - d) * levels[i] * levels[i];
		}
		return std::sqrt(parts * parts + products);
	};

	// a quiet noise raises the volumes of its channels up to twice, so far as they go:
	// that gives the tones back their lost half too
	const auto Gain = [&](uint8_t mask)
	{
		float highest = 1e-6f;
		for (int i = 0; i < 2; ++i)
		{
			if (mask & c_pulseMasks[i]) highest = std::max(highest, levels[i]);
		}
		return std::max(1.f, std::min({ target / std::max(NoiseLevelOf(mask), 1e-6f), 2.f, 1.f / highest }));
	};

	// the cost: the error of the noise level and the tone loss of the pulses mixed in,
	// a louder pulse is more likely the melody and is spoiled the least
	float louder = std::max(std::max(levels[0], levels[1]), 1e-6f);
	const auto Cost = [&](uint8_t mask)
	{
		float gain = Gain(mask);
		float cost = std::abs(Decibels(gain * NoiseLevelOf(mask) / target));
		for (int i = 0; i < 2; ++i)
		{
			float toneLossDb = c_toneLossDb - Decibels(gain);
			if (mask & c_pulseMasks[i]) cost += c_toneLossWeight * toneLossDb * levels[i] / louder;
		}
		return cost;
	};

	// the cheapest choice, the last one while it costs not much more
	const uint8_t maskA = c_pulseMasks[0];
	const uint8_t maskC = c_pulseMasks[1];
	const uint8_t choices[] = { maskA, maskC, uint8_t(maskA | maskC) };
	uint8_t best = choices[0];
	for (uint8_t mask : choices)
	{
		if (Cost(mask) < Cost(best)) best = mask;
	}
	for (uint8_t mask : choices)
	{
		if (mask == last && Cost(mask) <= Cost(best) + c_noiseHysteresisDb) best = mask;
	}
	gain = Gain(best);
	return best;
}
