#include "SimLR35902.h"
#include <algorithm>
#include <cmath>

struct SimLR35902::State
{
	struct Pulse
	{
		uint16_t period; // of the duty steps: (2048 - period) * 4 CPU clocks
		uint8_t  duty;
		float    level;
		bool     enable;
	}
	pulse[2];

	uint16_t wave_period;  // of the samples: (2048 - period) * 2 CPU clocks
	float    wave_rms;     // AC RMS of the output, full scale 1
	const SimLR35902::WaveForm* wave_form;
	bool     wave_enable;

	uint32_t noise_shift;    // CPU clocks of a shift of the LFSR
	float    noise_level;
	bool     noise_narrow;   // the 7-bit LFSR repeats 127 shifts
	bool     noise_tone;     // the narrow noise is high enough to play as a tone
	bool     noise_enable;
	uint8_t  noise_channels; // mask of the channels the noise is played in
	bool     noise_shared;   // mixed in the pulses of the channels
	float    noise_gain;     // of the volumes of the shared channels
};

namespace
{
	// the pulses go to the side channels, the wave to the middle one
	const int c_pulseChannels[2] = { Frame::Channel::A, Frame::Channel::C };

	// the 7-bit LFSR repeats a sequence of 127 steps
	const int c_narrowNoiseSteps = 127;

	const int c_sampleRate = 44100;
	const double c_pi = 3.14159265358979;

	// frames of the head of a hit played in channel B when a wave note starts with it
	// and no channel is free: the note waits and starts after it (as on the NES)
	const int c_noiseHeadFrames = 2;

	// the level of the noise relative to the other channels, to tune by ear
	const float c_noiseGainDb = 0.f;

	// Mixed in a pulse, the noise sounds about 3 dB under it, while the Game Boy plays
	// the noise mostly 10-20 dB under the other channels. So a noise more than this much
	// quieter than the pulses is not mixed in, its attack too (the NES mixes it in).
	const float c_noiseMaskingDb = 6.f;
	const int c_noiseAttackFrames = 0;

	// the highest frequency heard, for the power of the noise heard
	const double c_hearingLimit = 20000; // Hz

	// Sine integral Si(x).
	double SineIntegral(double x)
	{
		if (x < 16)
		{
			// the series x - x^3/(3*3!) + x^5/(5*5!) - ...
			double sum = 0, term = x;
			for (int n = 0; n < 80 && std::abs(term) > 1e-16; ++n)
			{
				sum += term / (2 * n + 1);
				term *= -x * x / ((2 * n + 2) * (2 * n + 3));
			}
			return sum;
		}
		// the asymptotic expansion
		double x2 = x * x;
		return c_pi / 2 - std::cos(x) / x * (1 - 2 / x2 + 24 / (x2 * x2)) - std::sin(x) / x2 * (1 - 6 / x2 + 120 / (x2 * x2));
	}

	// The share of the power of random steps at the rate given that is heard. Their
	// spectrum is sinc^2 with zeros at the multiples of the rate: the share below B
	// is 2/pi * (Si(2X) - sin^2(X) / X), X = pi * B / rate.
	double HeardShare(double rate)
	{
		double x = c_pi * c_hearingLimit / rate;
		double s = std::sin(x);
		return std::min(1.0, 2 / c_pi * (SineIntegral(2 * x) - s * s / x));
	}

	// the AY8930 duties 0-4: 3.125, 6.25, 12.5, 25 and 50%, the
	// duties over 50% have the same harmonics as their complements
	const double c_dutyCycles[5] = { 0.03125, 0.0625, 0.125, 0.25, 0.5 };
	const uint8_t c_squareDuty = 4;

	// The gate of an ultrasonic tone scales the envelope by its duty (the mean output).
	// The AY8930 duty codes 0-8 have these high steps of 32: the envelope from -30 dB
	// (code 0) to -0.3 dB (code 8), documents/AY8930.md. The AY and the YM have the 50%
	// tone only: the envelope at -6 dB, used when the wave is this many decibels quieter.
	const int c_dutySteps[9] = { 1, 2, 4, 8, 16, 24, 28, 30, 31 };
	const uint8_t c_envelopeDutyCodes = 9;
	const float c_halfEnvelopeDb = 3.f;

	// the harmonics compared to choose the timbre of a waveform
	const int c_waveHarmonics = 8;

	// The RAM holds 2 or 4 periods of the pitch when the harmonics that are
	// multiples of 2 or 4 have at least this share of the power.
	const double c_waveMultipleShare = 0.9;

	// the analyses of the waveforms kept, to limit the memory
	const size_t c_waveFormsMax = 64;

	// on the AY8930 another duty than 50% only when its error is under this part
	// of the error of the 50% square
	const float c_waveDutyBias = 0.75f;

	// the envelope gated by the tone only when its error is under this part of the
	// error of the envelope alone: it costs a frame of a stopped tone at each note
	const float c_waveComboBias = 0.9f;

	// the envelope gated by the tone only when its two starts (the tone high or low)
	// differ by not more than this many decibels in level
	const float c_comboLevelSpreadDb = 1.5f;

	// a change of the period by more than this part starts a note, as a trigger does
	const double c_waveNoteChange = 0.03;

	// a note starts the tone with the envelope again when the last start was at least
	// this many frames before: fast arpeggios and slides keep the tone running
	const int c_waveSyncFrames = 4;

	// the envelope shapes: a repeated rising saw and a triangle that rises first
	const uint8_t c_sawShape = 0x0C;
	const uint8_t c_triangleShape = 0x0E;

	// octaves the envelope plays above the wave, to tune by ear
	const int c_waveEnvelopeOctaves = 0;

	// the harmonic profile model of a pulse of duty d: sin(pi j d)^2 / j^2
	void PulseProfile(double d, double* profile)
	{
		double sum = 0;
		for (int j = 1; j <= c_waveHarmonics; ++j)
		{
			double s = std::sin(c_pi * j * d);
			sum += (profile[j - 1] = s * s / (j * j));
		}
		for (int j = 0; j < c_waveHarmonics; ++j) profile[j] /= sum;
	}

	// the distance of two power profiles, compared as the square roots of the shares
	double ProfileError(const double* a, const double* b)
	{
		double error = 0;
		for (int j = 0; j < c_waveHarmonics; ++j)
		{
			double d = std::sqrt(a[j]) - std::sqrt(b[j]);
			error += d * d;
		}
		return error;
	}

	// the powers of the harmonics 1..count of a period of samples, and its AC RMS
	void Harmonics(const double* x, int n, double* powers, int count, double& rms)
	{
		double mean = 0, power = 0;
		for (int i = 0; i < n; ++i) mean += x[i];
		mean /= n;
		for (int i = 0; i < n; ++i) power += (x[i] - mean) * (x[i] - mean);
		rms = std::sqrt(power / n);

		for (int k = 1; k <= count; ++k)
		{
			double re = 0, im = 0;
			for (int i = 0; i < n; ++i)
			{
				re += (x[i] - mean) * std::cos(2 * c_pi * k * i / n);
				im += (x[i] - mean) * std::sin(2 * c_pi * k * i / n);
			}
			powers[k - 1] = (k <= n / 2 ? re * re + im * im : 0);
		}
	}
}

SimLR35902::SimLR35902()
	: ChipSim(Type::LR35902)
	, m_cyclesPerSample(0)
	, m_cpuCycles(0)
	, m_waveEnvelope(false)
	, m_waveCombo(-1)
	, m_waveSyncing(false)
	, m_waveRestarted(false)
	, m_waveNote(0)
	, m_waveNoteAge(0)
	, m_waveShape(0)
	, m_wavePlayed(false)
	, m_noiseHead(0)
	, m_noiseChannels(0)
	, m_noiseAge(0)
	, m_noiseVolume(0)
	, m_noiseRestarted(false)
	, m_noiseShift(0)
	, m_noisePeriod(1)
	, m_noiseHearing(1)
{
	ConfigureOutput(OutputType::SingleChip, Chip::Model::YM2149);
}

void SimLR35902::ConfigureClock(int srcClock, int dstClock)
{
	LR35902Apu::Init(srcClock);
	ChipSim::ConfigureClock(srcClock, dstClock);
	m_cyclesPerSample = uint32_t((uint64_t(srcClock) << 16) / c_sampleRate);
	m_noiseShift = 0;
}

void SimLR35902::ConfigureOutput(OutputType outputType, Chip::Model model)
{
	m_outputType = outputType;
	m_noiseShift = 0;
	ConfigureLevels(model, outputType == OutputType::AY8930Chip);
	BuildEnvelopeModels();
	m_waveForms.clear();
}

void SimLR35902::Reset()
{
	LR35902Apu::Reset();
	m_cpuCycles = 0;
	m_waveEnvelope = false;
	m_waveCombo = -1;
	m_waveSyncing = false;
	m_waveRestarted = false;
	m_waveNote = 0;
	m_waveNoteAge = 0;
	m_wavePlayed = false;
	m_waveShape = 0;
	m_noiseHead = 0;
	m_noiseChannels = 0;
	m_noiseAge = 0;
	m_noiseVolume = 0;
	m_noiseRestarted = false;
}

void SimLR35902::Write(int chip, Register reg, uint8_t data)
{
	// VGM: the registers 0x00-0x3F are FF10-FF4F
	if (chip == 0)
	{
		LR35902Apu::Write(NR10 + reg, data);
		if (NR10 + reg == NR34 && (data & 0x80)) m_waveRestarted = true;
		if (NR10 + reg == NR44 && (data & 0x80)) m_noiseRestarted = true;
	}
}

void SimLR35902::Simulate(int samples)
{
	while (samples--)
	{
		m_cpuCycles += m_cyclesPerSample;
		LR35902Apu::Process(m_cpuCycles >> 16);
		m_cpuCycles &= 0xFFFF;
	}
}

void SimLR35902::Convert(Frame& frame)
{
	State state{};

	for (int i = 0; i < 2; ++i)
	{
		const auto& pulse = m_pulse[i];
		auto& out = state.pulse[i];
		out.period = pulse.period;
		out.duty = pulse.duty;
		out.level = ChannelLevel(i, pulse.envelope.volume);
		out.enable = (pulse.on && out.level > 0);
	}

	// the output level shifts the samples right by 0, 1 or 2 bits
	state.wave_period = m_wave.period;
	state.wave_form = (m_wave.on && m_wave.level ? AnalyzeWave() : nullptr);
	if (state.wave_form)
	{
		float shift = float(1 << (m_wave.level - 1));
		state.wave_rms = state.wave_form->rms / 15.f / shift * ChannelLevel(2, 15);
		state.wave_enable = (state.wave_rms > 0);
	}

	// with the shift 14 or 15 the LFSR stops, its output does not change
	state.noise_shift = NoiseShiftCycles();
	state.noise_level = ChannelLevel(3, m_noise.envelope.volume);
	state.noise_narrow = m_noise.narrow;
	state.noise_enable = (m_noise.on && m_noise.shift < 14 && state.noise_level > 0);

	state.noise_level *= std::pow(10.f, c_noiseGainDb / 20);

	// the PSG noise, but on the AY8930 and for the narrow noise played as a tone
	state.noise_tone = (state.noise_narrow && PeriodicNoiseAsTone(c_narrowNoiseSteps, state.noise_shift));
	if (state.noise_enable && m_outputType != OutputType::AY8930Chip && !state.noise_tone)
	{
		UpdateNoisePeriod(state.noise_shift);
		state.noise_level *= m_noiseHearing;
	}

	// a hit: the noise starts, gets louder or is triggered again
	uint8_t volume = (state.noise_enable ? m_noise.envelope.volume : 0);
	bool restarted = (m_noiseRestarted && volume);
	m_noiseAge = (volume > m_noiseVolume || restarted ? 0 : m_noiseAge + 1);
	m_noiseVolume = volume;
	m_noiseRestarted = false;
	PlaceNoise(state);

	switch (m_outputType)
	{
	case OutputType::SingleChip: ConvertToSingleChip(state, frame); break;
	case OutputType::DoubleChip: ConvertToDoubleChip(state, frame); break;
	case OutputType::AY8930Chip: ConvertToAY8930Chip(state, frame); break;
	}
}

void SimLR35902::ConvertToSingleChip(State& state, Frame& frame)
{
	uint8_t mixer = 0x3F;
	uint8_t volumes[3]{};

	// Pulses -> Squares A and C, one chip plays every duty as 50%, of the same power
	for (int i = 0; i < 2; ++i)
	{
		const auto& pulse = state.pulse[i];
		if (pulse.enable)
		{
			// the 8 duty steps take (2048 - period) * 32 CPU clocks
			int chan = c_pulseChannels[i];
			EnableTone(mixer, chan);
			frame[0].Update(Register::t_period[chan], ConvertPeriod((2048 - pulse.period) * 2));
			volumes[chan] = PulseVolume(state, i);
		}
	}

	// Wave -> Channel B, Noise -> the channels chosen for it
	ConvertWave(state, frame[0], mixer, volumes, false);
	ConvertNoise(state, frame[0], mixer, volumes, false);

	frame[0].Update(Register::Mixer, mixer);
	UpdateVolumes(frame[0], volumes);
}

void SimLR35902::ConvertToDoubleChip(State& state, Frame& frame)
{
	uint8_t mixer0 = 0x3F;
	uint8_t mixer1 = 0x3F;
	uint8_t volumes0[3]{};
	uint8_t volumes1[3]{};

	// Pulses -> Chip 0 Squares A and C + Chip 1 Squares A and C an octave up
	for (int i = 0; i < 2; ++i)
	{
		const auto& pulse = state.pulse[i];
		if (pulse.enable)
		{
			// the octave is rounded on its own, not as a half of the rounded period
			const auto& gains = k_dutyGains[pulse.duty];
			int chan = c_pulseChannels[i];

			EnableTone(mixer0, chan);
			frame[0].Update(Register::t_period[chan], ConvertPeriod((2048 - pulse.period) * 2));
			volumes0[chan] = LevelToVolume(gains[0] * pulse.level);

			if (gains[1] > 0)
			{
				EnableTone(mixer1, chan);
				frame[1].Update(Register::t_period[chan], ConvertPeriod((2048 - pulse.period) * 2, 2));
				volumes1[chan] = LevelToVolume(gains[1] * pulse.level);
			}
		}
	}

	// Wave -> Chip 0 Channel B
	ConvertWave(state, frame[0], mixer0, volumes0, false);

	// Noise -> Chip 1 Channel B
	ConvertNoise(state, frame[1], mixer1, volumes1, false);

	frame[0].Update(Register::Mixer, mixer0);
	frame[1].Update(Register::Mixer, mixer1);
	UpdateVolumes(frame[0], volumes0);
	UpdateVolumes(frame[1], volumes1);
}

void SimLR35902::ConvertToAY8930Chip(State& state, Frame& frame)
{
	uint8_t mixer = 0x3F;
	uint8_t volumes[3]{};
	if (!frame[0].IsExpMode()) frame[0].SetExpMode();

	// Pulses -> Pulses A and C: the expanded mode tone period is 8 chip clocks
	for (int i = 0; i < 2; ++i)
	{
		const auto& pulse = state.pulse[i];
		if (pulse.enable)
		{
			int chan = c_pulseChannels[i];
			EnableTone(mixer, chan);
			frame[0].Update(Register::t_period[chan], ConvertPeriod((2048 - pulse.period) * 4));
			frame[0].Update(Register::t_duty[chan], uint8_t(k_ay8930DutyOffset + pulse.duty));
			volumes[chan] = PulseVolume(state, i);
		}
	}

	// Wave -> Channel B, Noise -> the channels chosen for it
	ConvertWave(state, frame[0], mixer, volumes, true);
	ConvertNoise(state, frame[0], mixer, volumes, true);

	frame[0].Update(Register::Mixer, mixer);
	UpdateVolumes(frame[0], volumes);
}

float SimLR35902::ChannelLevel(int chan, uint8_t volume) const
{
	// The DAC is linear. NR51 routes the channel to the left and the right,
	// NR50 scales each side by 1-8 / 8. One PSG plays the mean of both sides.
	int left  = ((m_routing & (0x10 << chan)) ? 1 + ((m_volumes >> 4) & 0x07) : 0);
	int right = ((m_routing & (0x01 << chan)) ? 1 + (m_volumes & 0x07) : 0);
	return (volume / 15.f) * (left + right) / 16.f;
}

////////////////////////////////////////////////////////////////////////////////

void SimLR35902::BuildEnvelopeModels()
{
	// the envelope steps through the levels of the destination chip: 32 on the
	// YM2149 and the AY8930, 16 on the AY8910; the triangle goes up and down
	int steps = (m_levelsCount * m_levelsStep);
	double saw[32], triangle[64], powers[c_waveHarmonics], rms;
	for (int i = 0; i < steps; ++i)
	{
		saw[i] = triangle[i] = m_levels[i];
		triangle[2 * steps - 1 - i] = m_levels[i];
	}

	const double* shapes[2] = { saw, triangle };
	for (int e = 0; e < 2; ++e)
	{
		Harmonics(shapes[e], steps * (e + 1), powers, c_waveHarmonics, rms);
		double sum = 0;
		for (double p : powers) sum += p;
		for (int j = 0; j < c_waveHarmonics; ++j) m_envelopes[e].profile[j] = float(powers[j] / sum);
		m_envelopes[e].rms = float(rms);
	}

	// The envelope gated by the tone. The AY8930 does not take part: in the expanded
	// mode the tone cannot be started at a known step of its duty cycle.
	m_combos.clear();
	if (m_outputType == OutputType::AY8930Chip) return;

	// One envelope cycle is the pitch period: more cycles would make the envelope
	// period shorter and its rounding would put the notes out of tune.
	struct Shape { uint8_t shape; bool triangle; bool rises; };
	const Shape shapeList[] = { { 0x08, false, false }, { 0x0C, false, true }, { 0x0E, true, true }, { 0x0A, true, false } };
	const int n = 256; // samples of the pitch period
	double x[n];

	for (const Shape& shape : shapeList)
	{
		for (int periods : { 1, 2, 4, 8 })
		{
			ComboModel combo{ shape.shape, shape.triangle, periods };
			double levels[2];
			for (int phase = 0; phase < 2; ++phase)
			{
				for (int i = 0; i < n; ++i)
				{
					double t = (i + 0.5) / n;
					int s = int(t * steps * (shape.triangle ? 2 : 1));
					if (shape.triangle && s >= steps) s = 2 * steps - 1 - s; // the 2nd half goes back
					if (!shape.rises) s = steps - 1 - s;

					// the tone starts high (phase 0) or low (phase 1)
					double v = t * combo.periods + 0.5 * phase; v -= std::floor(v);
					x[i] = (v < 0.5 ? m_levels[s] : 0);
				}
				Harmonics(x, n, powers, c_waveHarmonics, rms);
				double sum = 0;
				for (double p : powers) sum += p;
				for (int j = 0; j < c_waveHarmonics; ++j) combo.profiles[phase][j] = float(sum > 0 ? powers[j] / sum : 0);
				levels[phase] = rms;
			}

			// The two starts must sound equally loud. The tone of a saw passes its low half
			// at one start and its high half at the other (22 dB apart for one tone period
			// per cycle); a triangle gives the same level at both, by its symmetry.
			if (levels[0] > 0 && levels[1] > 0 && std::abs(Decibels(float(levels[0] / levels[1]))) <= c_comboLevelSpreadDb)
			{
				m_combos.push_back(combo);
			}
		}
	}
}

const SimLR35902::WaveForm* SimLR35902::AnalyzeWave()
{
	WaveRam ram;
	std::copy(std::begin(m_wave.ram), std::end(m_wave.ram), ram.begin());
	auto found = m_waveForms.find(ram);
	if (found != m_waveForms.end()) return (found->second.multiple ? &found->second : nullptr);

	// 32 samples of 4 bits, the high nibble first
	double x[32], powers[16], rms;
	for (int i = 0; i < 32; ++i) x[i] = ((ram[i >> 1] >> ((i & 1) ? 0 : 4)) & 0x0F);
	Harmonics(x, 32, powers, 16, rms);

	WaveForm form{};
	form.rms = float(rms);
	double total = 0;
	for (double p : powers) total += p;
	if (total > 0)
	{
		// the pitch: the harmonics that are multiples of 4 or 2 carry nearly all the power
		form.multiple = 1;
		for (int m : { 4, 2 })
		{
			double share = 0;
			for (int k = m; k <= 16; k += m) share += powers[k - 1];
			if (share >= c_waveMultipleShare * total) { form.multiple = m; break; }
		}

		// the power profile of the harmonics of the pitch
		double profile[c_waveHarmonics]{}, sum = 0;
		for (int j = 1; j <= c_waveHarmonics && j * form.multiple <= 16; ++j)
		{
			sum += (profile[j - 1] = powers[j * form.multiple - 1]);
		}
		if (sum > 0) for (double& p : profile) p /= sum;

		// A waveform of two levels is a pulse: the tone plays it, on the AY8930 with
		// the duty of the least error, another one than 50% only when it fits by far.
		double low = *std::min_element(x, x + 32), high = *std::max_element(x, x + 32);
		if (std::count(x, x + 32, low) + std::count(x, x + 32, high) == 32)
		{
			form.timbre = Timbre::Pulse;
			form.duty = c_squareDuty;
			if (m_outputType == OutputType::AY8930Chip)
			{
				double model[c_waveHarmonics];
				PulseProfile(c_dutyCycles[c_squareDuty], model);
				float squareError = float(ProfileError(profile, model)), best = squareError;
				for (uint8_t duty = 0; duty < c_squareDuty; ++duty)
				{
					PulseProfile(c_dutyCycles[duty], model);
					float error = float(ProfileError(profile, model));
					if (error < c_waveDutyBias * squareError && error < best) { form.duty = duty; best = error; }
				}
			}
		}
		else
		{
			// any other waveform plays on the envelope: the shape of the least error
			form.timbre = Timbre::Envelope;
			float envelopeError = 1e9f;
			for (int e = 0; e < 2; ++e)
			{
				double envelope[c_waveHarmonics];
				for (int j = 0; j < c_waveHarmonics; ++j) envelope[j] = m_envelopes[e].profile[j];
				float error = float(ProfileError(profile, envelope));
				if (error < envelopeError) { form.envelope = e; envelopeError = error; }
			}

			// or gated by the tone, as it sounds at the worse start of the tone
			float comboError = c_waveComboBias * envelopeError;
			for (int c = 0; c < int(m_combos.size()); ++c)
			{
				float error = 0;
				for (const auto& p : m_combos[c].profiles)
				{
					double model[c_waveHarmonics];
					for (int j = 0; j < c_waveHarmonics; ++j) model[j] = p[j];
					error = std::max(error, float(ProfileError(profile, model)));
				}
				if (error < comboError) { form.timbre = Timbre::Combo; form.combo = c; comboError = error; }
			}
		}
	}

	// a tune can stream samples through the RAM: a new waveform in each frame
	if (m_waveForms.size() >= c_waveFormsMax) m_waveForms.clear();
	auto& stored = (m_waveForms[ram] = form);
	return (stored.multiple ? &stored : nullptr);
}

void SimLR35902::ConvertWave(const State& state, Frame::Registers& regs, uint8_t& mixer, uint8_t* volumes, bool expMode)
{
	const int chan = Frame::Channel::B;
	bool envelope = false;
	int combo = -1;

	// the head of a hit takes channel B, the wave waits for it
	bool waits = (m_outputType != OutputType::DoubleChip && (state.noise_channels & (1 << chan)));
	if (state.wave_enable && !waits)
	{
		// the samples take (2048 - period) * 2 CPU clocks, 32 of them the period of
		// the RAM, which holds the given number of periods of the pitch
		const WaveForm& form = *state.wave_form;
		uint32_t clocks = (2048 - state.wave_period);

		// a note starts with a trigger or a jump of the pitch
		bool note = (m_waveRestarted || !m_waveNote || std::abs(double(clocks) / m_waveNote - 1) > c_waveNoteChange);
		if (note) m_waveNote = uint16_t(clocks);

		if (form.timbre == Timbre::Combo)
		{
			// One envelope cycle is the pitch period and holds the tone periods in a whole
			// number. In chip clocks a saw cycle is 256 x the envelope period, a triangle
			// 512 x, a tone cycle 16 x the tone period: no drift between the two.
			const ComboModel& model = m_combos[form.combo];
			uint16_t envelopePeriod = std::max(ConvertPeriod(clocks, (model.triangle ? 8 : 4) * form.multiple), uint16_t(1));
			uint32_t tonePeriod = envelopePeriod * (model.triangle ? 32u : 16u) / model.periods;
			if (tonePeriod <= 0x0FFF)
			{
				// There is no reset of the tone. The tone runs at the period 1 for a frame,
				// then the tone period and the shape are written together: the tone starts
				// with the envelope, high or low by chance. This repeats at each note,
				// unless the notes are too short, as the counters lose the start.
				combo = form.combo;
				bool starts = !m_waveSyncing && (m_waveCombo != combo || (note && m_waveNoteAge >= c_waveSyncFrames));
				EnableTone(mixer, chan);
				regs.Update(Register::Period::E, envelopePeriod);
				regs.Update(Register::t_period[chan], uint16_t(starts ? 1 : tonePeriod));
				if (starts || m_waveSyncing) regs.Update(Register::E_Shape, model.shape);
				volumes[chan] = regs.emask();

				m_waveNoteAge = (starts ? 0 : m_waveNoteAge + 1);
				m_waveSyncing = starts;
			}
		}

		// the envelope alone, also for a combo too low for the tone period
		envelope = (form.timbre != Timbre::Pulse && combo < 0);
		if (envelope)
		{
			// a saw cycle takes 256 envelope periods (clocks), a triangle 512
			uint8_t shape = (form.envelope ? c_triangleShape : c_sawShape);
			uint32_t divider = ((form.envelope ? 8 : 4) * form.multiple) << c_waveEnvelopeOctaves;
			Register shapeReg = (expMode ? Register::EB_Shape : Register::E_Shape);
			Register::Period periodReg = (expMode ? Register::Period::EB : Register::Period::E);

			// The envelope plays at full scale; an ultrasonic tone brings it nearer to the
			// level of the wave. The timbre stays: the gate only scales the envelope.
			float levelDb = Decibels(state.wave_rms / m_envelopes[form.envelope].rms);
			if (expMode)
			{
				// the AY8930 mixer outputs nothing with tone and noise off: the tone
				// lets the envelope through, with the duty that fits the level best
				uint8_t duty = 0;
				for (uint8_t code = 1; code < c_envelopeDutyCodes; ++code)
				{
					float error = std::abs(levelDb - Decibels(c_dutySteps[code] / 32.f));
					if (error < std::abs(levelDb - Decibels(c_dutySteps[duty] / 32.f))) duty = code;
				}
				EnableTone(mixer, chan);
				regs.Update(Register::t_period[chan], uint16_t(0));
				regs.Update(Register::t_duty[chan], duty);
			}
			else if (levelDb < -c_halfEnvelopeDb)
			{
				EnableTone(mixer, chan);
				regs.Update(Register::t_period[chan], uint16_t(1));
			}

			// writing the shape restarts the envelope: only when it starts or changes
			if (!m_waveEnvelope || shape != m_waveShape) regs.Update(shapeReg, shape);
			regs.Update(periodReg, ConvertPeriod(clocks, divider));
			volumes[chan] = regs.emask();
			m_waveShape = shape;
		}
		else if (form.timbre == Timbre::Pulse)
		{
			// a pulse of amplitude V and duty d has the AC RMS V * sqrt(d * (1 - d)),
			// the expanded mode tone period is 8 chip clocks, the standard one 16
			double d = c_dutyCycles[form.duty];
			EnableTone(mixer, chan);
			regs.Update(Register::t_period[chan], ConvertPeriod(clocks * (expMode ? 8 : 4), form.multiple));
			if (expMode) regs.Update(Register::t_duty[chan], form.duty);
			volumes[chan] = LevelToVolume(float(state.wave_rms / std::sqrt(d * (1 - d))));
		}
	}
	m_waveEnvelope = envelope;
	m_waveCombo = combo;
	if (combo < 0) m_waveSyncing = false;
	m_waveRestarted = false;
}

////////////////////////////////////////////////////////////////////////////////

uint8_t SimLR35902::PulseVolume(const State& state, int index) const
{
	// one chip plays every duty as 50%, of the same power
	const auto& pulse = state.pulse[index];
	float level = pulse.level;
	if (m_outputType == OutputType::SingleChip) level *= k_squareGains[pulse.duty];
	return LevelToVolume(level);
}

void SimLR35902::UpdateNoisePeriod(uint32_t shift)
{
	if (shift == m_noiseShift) return;
	m_noiseShift = shift;

	m_noisePeriod = std::max(NoisePeriod(shift), uint16_t(1));

	// The PSG noise is not faster than the period 1: the faster source noise has more
	// of its power above hearing. Make it quieter by that, never louder.
	double srcRate = double(m_srcClock) / shift;
	double dstRate = double(m_dstClock) / (16 * m_noisePeriod);
	m_noiseHearing = float(std::sqrt(std::min(1.0, HeardShare(srcRate) / HeardShare(dstRate))));
}

void SimLR35902::PlaceNoise(State& state)
{
	// As on the NES. The 2nd chip has a channel for the noise. One chip does not
	// play the narrow noise as a tone, the lower one is a rattle as the usual noise.
	const uint8_t maskB = uint8_t(1 << Frame::Channel::B);
	bool place = (state.noise_enable && !(m_outputType == OutputType::SingleChip && state.noise_tone));

	if (m_outputType == OutputType::DoubleChip)
	{
		state.noise_channels = (state.noise_enable ? maskB : 0);
		m_noiseChannels = state.noise_channels;
		return;
	}

	// a wave note and a hit start together while no channel is free:
	// the head of the hit takes channel B, the note waits for it
	bool pulsesOn = (state.pulse[0].enable && state.pulse[1].enable);
	bool noteStarts = (state.wave_enable && !m_wavePlayed);
	m_wavePlayed = state.wave_enable;
	if (!place || !state.wave_enable) m_noiseHead = 0;
	else if (pulsesOn && noteStarts && m_noiseAge == 0) m_noiseHead = c_noiseHeadFrames;

	state.noise_channels = 0;
	if (place && !state.wave_enable)
	{
		// the free middle channel
		state.noise_channels = maskB;
	}
	else if (place && m_noiseHead > 0)
	{
		--m_noiseHead;
		state.noise_channels = maskB;
	}
	else if (place && !pulsesOn)
	{
		// the channel of a silent pulse, the same one while both are silent
		uint8_t maskC = uint8_t(1 << c_pulseChannels[1]);
		bool useC = (state.pulse[0].enable || (!state.pulse[1].enable && m_noiseChannels == maskC));
		state.noise_channels = (useC ? maskC : uint8_t(1 << c_pulseChannels[0]));
	}
	else if (place)
	{
		// no free channel: the noise is mixed in the pulses, at the levels they play at
		bool expMode = (m_outputType == OutputType::AY8930Chip);
		float levels[2], duties[2];
		for (int i = 0; i < 2; ++i)
		{
			levels[i] = VolumeToLevel(PulseVolume(state, i));
			duties[i] = (expMode ? k_dutyRatios[state.pulse[i].duty] : 0.5f);
		}
		state.noise_channels = ShareNoise(levels, duties, state.noise_level, m_noiseAge, m_noiseChannels, expMode, state.noise_gain,
			c_noiseMaskingDb, c_noiseAttackFrames);
		state.noise_shared = true;
	}
	m_noiseChannels = state.noise_channels;
}

void SimLR35902::ConvertNoise(const State& state, Frame::Registers& regs, uint8_t& mixer, uint8_t* volumes, bool expMode) const
{
	if (!state.noise_channels) return;

	// The narrow noise is a buzz: a tone imitates it while its pitch is audible. The
	// AY8930 makes it with its noise generator, the 2nd chip with a square.
	bool asTone = state.noise_tone;
	bool square = (asTone && !expMode && m_outputType == OutputType::DoubleChip);
	float level = state.noise_level;
	if (asTone && (expMode || square)) level *= PeriodicNoiseGain(c_narrowNoiseSteps, state.noise_shift);

	if (expMode)
		UpdateAY8930Noise(regs, state.noise_shift, asTone ? c_narrowNoiseSteps : 0);
	else if (!square)
		regs.Update(Register::Period::N, m_noisePeriod);

	for (int chan = 0; chan < 3; ++chan)
	{
		if (!(state.noise_channels & (1 << chan))) continue;
		if (square)
		{
			EnableTone(mixer, chan);
			regs.Update(Register::t_period[chan], PeriodicNoisePeriod(c_narrowNoiseSteps, state.noise_shift));
			volumes[chan] = LevelToVolume(level);
		}
		else
		{
			EnableNoise(mixer, chan);
			if (!state.noise_shared) volumes[chan] = LevelToVolume(level);
			else if (state.noise_gain > 1) volumes[chan] = LevelToVolume(state.noise_gain * VolumeToLevel(volumes[chan]));
		}
	}
}
