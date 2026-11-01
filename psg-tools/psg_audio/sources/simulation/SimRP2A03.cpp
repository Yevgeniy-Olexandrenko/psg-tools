#include "SimRP2A03.h"
#include "stream/Frame.h"
#include "stream/Stream.h"
#include <algorithm>
#include <cmath>

struct SimRP2A03::State
{
    struct Pulse
    {
        uint16_t period;
        uint8_t  volume;
        uint8_t  duty;
        bool     enable;
    }
    pulse[2];

    uint16_t triangle_period;
    bool     triangle_enable;  // plays
    bool     triangle_hold;    // silent, but its level is held in channel B
    bool     triangle_restart; // starts after a pause, the envelope restarts

    uint16_t noise_period;
    uint8_t  noise_volume;
    bool     noise_mode;
    bool     noise_enable;
    uint8_t  noise_channels;   // mask of the channels the noise is played in
    bool     noise_shared;     // mixed in the pulses of the channels
    float    noise_gain;       // of the volumes of the shared channels

    // channel B is taken by the triangle
    bool TriangleOn() const { return (triangle_enable || triangle_hold); }
};

namespace
{
    // the pulses go to the side channels, the triangle to the middle one
    const int c_pulseChannels[2] = { Frame::Channel::A, Frame::Channel::C };

    // the PSG triangle has a fixed full scale level, the other channels are leveled to it:
    // AC RMS of the NES triangle through the NES mixer, and of the PSG envelope triangle
    const float c_nesTriangleRms = 0.0756f;
    const float c_ymTriangleRms  = 0.2712f;
    const float c_ayTriangleRms  = 0.3044f;

    // frames the triangle level is held after it stops: less than one envelope step
    // at the longest period (8 * 0xFFFF clocks, 0.26 s at 2 MHz), also at 50 Hz
    const int c_triangleHoldFrames = 12;

    // the NES duties 12.5, 25, 50 and 75%
    const float c_dutyRatios[4] = { 0.125f, 0.25f, 0.5f, 0.75f };

    // levels of the square and of its octave on two chips for the NES duties:
    // (a^2 + b^2) / 4 = d * (1 - d), b = 0.707 * a for 25% and 75%, b = a for 12.5%
    const float c_dutyGains[4][2] =
    {
        { 0.468f, 0.468f }, { 0.707f, 0.500f }, { 1.000f, 0.000f }, { 0.707f, 0.500f }
    };

    // mixing the noise in a pulse halves its tone (-6 dB): the loss counts in the cost
    // of a choice with this weight, scaled by the pulse level relative to the louder one
    const float c_toneLossDb = 6.f;
    const float c_toneLossWeight = 0.5f;

    // the noise stays in its channels while another choice costs not less by this
    const float c_noiseHysteresisDb = 1.f;

    // a noise this much quieter than the pulses is masked by them on the NES,
    // it is mixed in them for the first frames of a hit only
    const float c_noiseMaskingDb = 12.f;
    const int c_noiseAttackFrames = 2;

    // frames of the head of a hit played in channel B when a triangle note starts with it
    // and no channel is free: the note waits and starts after it
    const int c_noiseHeadFrames = 2;

    // the AY8930 expanded mode noise toggles after (LFSR & AND | OR) periods: with AND 13
    // its runs are 1, 4, 5, 8, 9, 12 or 13 periods, close to the white noise of the NES
    // by ear at the period of half the NES shift
    const uint8_t c_ay8930NoiseAndMask = 0x0D;

    // the short mode noise repeats a sequence of 93 steps
    const int c_shortNoiseSteps = 93;

    // the upper frequency of hearing, Hz
    const double c_hearingLimit = 20000;

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

    float Decibels(float ratio)
    {
        return 20.f * std::log10(std::max(ratio, 1e-6f));
    }

    uint8_t ChannelMask(int chan)
    {
        return uint8_t(1 << chan);
    }
}

SimRP2A03::SimRP2A03()
    : ChipSim(Type::RP2A03)
    , m_triangleHold(0)
    , m_trianglePlayed(false)
    , m_triangleInterrupted(false)
    , m_noiseHead(0)
    , m_noiseChannels(0)
    , m_noiseAge(0)
    , m_noiseVolume(0)
{
    ConfigureOutput(OutputType::SingleChip, Chip::Model::YM2149);
}

void SimRP2A03::ConfigureClock(int srcClock, int dstClock)
{
    RP2A03Apu::Init(44100, srcClock);
    ChipSim::ConfigureClock(srcClock, dstClock);
}

void SimRP2A03::ConfigureOutput(OutputType outputType, Chip::Model model)
{
    m_outputType = outputType;

    // AY8930 has the YM2149 DAC, in the expanded mode with all 32 volumes
    bool ay8910 = (outputType != OutputType::AY8930Chip && model == Chip::Model::AY8910);
    m_levels = (ay8910 ? c_ayLevels : c_ymLevels);
    m_levelsCount = (outputType == OutputType::AY8930Chip ? 32 : 16);
    m_levelsStep = (ay8910 || outputType == OutputType::AY8930Chip ? 1 : 2);
    m_psgTriangleRms = (ay8910 ? c_ayTriangleRms : c_ymTriangleRms);
}

void SimRP2A03::Reset()
{
    RP2A03Apu::Reset();
    m_triangleHold = 0;
    m_trianglePlayed = false;
    m_triangleInterrupted = false;
    m_noiseHead = 0;
    m_noiseChannels = 0;
    m_noiseAge = 0;
    m_noiseVolume = 0;
}

void SimRP2A03::Write(int chip, Register reg, uint8_t data)
{
    if (chip == 0)
    {
        RP2A03Apu::Write(0x4000 | reg, data);
    }
}

void SimRP2A03::Simulate(int samples)
{
    while (samples--)
    {
        m_cpuCycles += m_cpuCyclesPerSample;
        RP2A03Apu::Process(m_cpuCycles >> 16);
        m_cpuCycles &= 0xFFFF;
    }
}

void SimRP2A03::Convert(Frame& frame)
{
    State state{};

    // pulse channels
    const auto GetPulse = [](const auto& pulse, State::Pulse& out)
    {
        out.period = pulse.timer_period;
        out.volume = pulse.envelope.out;
        out.duty   = pulse.duty;
        out.enable = (pulse.len_counter && !pulse.sweep.silence && pulse.envelope.out);
    };
    GetPulse(m_pulse1, state.pulse[0]);
    GetPulse(m_pulse2, state.pulse[1]);

    // triangle channel
    state.triangle_period = m_triangle.timer_period;
    state.triangle_enable = (m_triangle.lin_counter > 0 && m_triangle.len_counter > 0 && m_triangle.timer_period >= 2);

    // a stopped NES triangle holds its level, cutting the PSG envelope at a random level
    // would click: it is held too, for a while, and the next note resumes from there
    bool holding = (m_triangleHold > 0);
    state.triangle_hold = (!state.triangle_enable && holding);
    state.triangle_restart = (state.triangle_enable && !holding);
    if (state.triangle_enable) m_triangleHold = c_triangleHoldFrames;
    else if (holding) --m_triangleHold;

    // noise channel
    state.noise_period = m_noise.timer_period;
    state.noise_volume = m_noise.envelope.out;
    state.noise_mode   = m_noise.mode;
    state.noise_enable = (m_noise.len_counter && m_noise.envelope.out);
    PlaceNoise(state);

    switch (m_outputType)
    {
    case OutputType::SingleChip: ConvertToSingleChip(state, frame); break;
    case OutputType::DoubleChip: ConvertToDoubleChip(state, frame); break;
    case OutputType::AY8930Chip: ConvertToAY8930Chip(state, frame); break;
    }
}

void SimRP2A03::ConvertToSingleChip(const State& state, Frame& frame)
{
    uint8_t mixer = 0x3F;
    uint8_t volumes[3]{};

    // Pulses -> Squares A and C
    for (int i = 0; i < 2; ++i)
    {
        const auto& pulse = state.pulse[i];
        if (pulse.enable)
        {
            // the NES timer counts period + 1 clocks
            int chan = c_pulseChannels[i];
            EnableTone(mixer, chan);
            frame[0].Update(Register::t_period[chan], ConvertPeriod(pulse.period + 1));
            volumes[chan] = PulseVolume(pulse.volume);
        }
    }

    // Triangle -> Envelope B
    if (state.TriangleOn())
    {
        ConvertTriangle<Register::E_Shape, Register::Period::E>(state, frame, mixer, volumes);
    }

    // Noise -> the channels chosen for it
    if (state.noise_channels)
    {
        frame[0].Update(Register::Period::N, YMNoisePeriod(state));
        MixNoise(state, mixer, volumes);
    }

    // Mixer and volumes
    {
        frame[0].Update(Register::Mixer, mixer);
        UpdateVolumes(frame[0], volumes);
    }
}

void SimRP2A03::ConvertToDoubleChip(const State& state, Frame& frame)
{
    uint8_t mixer0 = 0x3F;
    uint8_t mixer1 = 0x3F;
    uint8_t volumes0[3]{};
    uint8_t volumes1[3]{};

    // Pulses -> Chip 0 Squares A and C + Chip 1 Squares A and C
    for (int i = 0; i < 2; ++i)
    {
        const auto& pulse = state.pulse[i];
        if (pulse.enable)
        {
            // the 2nd chip adds the duty as a square an octave up: its even harmonics
            // match 25% and 75% exactly at -3 dB, at any phase; 12.5% gets the same level
            const auto& gains = c_dutyGains[pulse.duty];
            int   chan  = c_pulseChannels[i];
            float level = PulseLevel(pulse.volume);

            EnableTone(mixer0, chan);
            frame[0].Update(Register::t_period[chan], ConvertPeriod(pulse.period + 1));
            volumes0[chan] = LevelToVolume(gains[0] * level);

            if (gains[1] > 0)
            {
                // the octave is rounded on its own, not as a half of the rounded period
                EnableTone(mixer1, chan);
                frame[1].Update(Register::t_period[chan], ConvertPeriod(pulse.period + 1, 2));
                volumes1[chan] = LevelToVolume(gains[1] * level);
            }
        }
    }

    // Triangle -> Chip 0 Envelope in Channel B
    if (state.TriangleOn())
    {
        ConvertTriangle<Register::E_Shape, Register::Period::E>(state, frame, mixer0, volumes0);
    }

    // Short noise -> Chip 1 Square in Channel B
    if (ShortNoiseAsTone(state))
    {
        EnableTone(mixer1, Frame::Channel::B);
        frame[1].Update(Register::Period::B, ConvertPeriod(c_shortNoiseSteps * state.noise_period, 16));
        volumes1[Frame::Channel::B] = NoiseVolume(state);
    }

    // Noise -> Chip 1 Noise in Channel B
    else if (state.noise_enable)
    {
        EnableNoise(mixer1, Frame::Channel::B);
        frame[1].Update(Register::Period::N, YMNoisePeriod(state));
        volumes1[Frame::Channel::B] = NoiseVolume(state);
    }

    // Mixers and volumes
    {
        frame[0].Update(Register::Mixer, mixer0);
        frame[1].Update(Register::Mixer, mixer1);
        UpdateVolumes(frame[0], volumes0);
        UpdateVolumes(frame[1], volumes1);
    }
}

void SimRP2A03::ConvertToAY8930Chip(const State& state, Frame& frame)
{
    uint8_t mixer = 0x3F;
    uint8_t volumes[3]{};

    // go to expanded mode
    if (!frame[0].IsExpMode()) frame[0].SetExpMode();

    // Pulses -> Squares A and C
    for (int i = 0; i < 2; ++i)
    {
        const auto& pulse = state.pulse[i];
        if (pulse.enable)
        {
            // the expanded mode tone period is 8 chip clocks,
            // its duties 2-5 are 12.5-75% as the NES duties 0-3
            int chan = c_pulseChannels[i];
            EnableTone(mixer, chan);
            frame[0].Update(Register::t_period[chan], ConvertPeriod((pulse.period + 1) << 1));
            frame[0].Update(Register::t_duty[chan], uint8_t(0x02 + pulse.duty));
            volumes[chan] = PulseVolume(pulse.volume);
        }
    }

    // Triangle -> Envelope B
    if (state.TriangleOn())
    {
        ConvertTriangle<Register::EB_Shape, Register::Period::EB>(state, frame, mixer, volumes);
    }

    // Noise -> the channels chosen for it
    if (state.noise_channels)
    {
        UpdateAY8930Noise(state, frame[0]);
        MixNoise(state, mixer, volumes);
    }

    // Mixer and volumes
    {
        frame[0].Update(Register::Mixer, mixer);
        UpdateVolumes(frame[0], volumes);
    }
}

template<Register::Value shape_reg, Register::Period period_reg>
void SimRP2A03::ConvertTriangle(const State& state, Frame& frame, uint8_t& mixer, uint8_t* volumes)
{
    // the divider is 8, not 16 of the same pitch: the log-shaped envelope has a strong
    // 2nd harmonic and sounds right an octave lower; a held level takes the longest period
    uint16_t period = (state.triangle_enable ? ConvertPeriod(state.triangle_period + 1, 8) : 0xFFFF);

    if (m_outputType == OutputType::AY8930Chip)
    {
        // the AY8930 mixer outputs nothing with tone and noise off,
        // a tone of the highest duty and pitch lets the envelope through
        EnableTone(mixer, Frame::Channel::B);
        frame[0].Update(Register::Period::B, uint16_t(0));
        frame[0].Update(Register::B_Duty, 0x08);
    }

    // writing the shape restarts the envelope: only after a pause,
    // from zero and rising first, so that the start does not click
    if (state.triangle_restart)
        frame[0].Update(shape_reg, 0x0E);

    frame[0].Update(period_reg, period);
    volumes[Frame::Channel::B] = frame[0].emask();
}

void SimRP2A03::UpdateVolumes(Frame::Registers& regs, const uint8_t* volumes)
{
    // a channel with tone and noise disabled outputs its volume as a constant
    // level, so the volume of a channel without sound must be zeroed
    for (int chan = 0; chan < 3; ++chan)
    {
        regs.Update(Register::volume[chan], volumes[chan]);
    }
}

////////////////////////////////////////////////////////////////////////////////

void SimRP2A03::PlaceNoise(State& state)
{
    // a hit: the noise starts or gets louder
    uint8_t volume = (state.noise_enable ? state.noise_volume : 0);
    m_noiseAge = (volume > m_noiseVolume ? 0 : m_noiseAge + 1);
    m_noiseVolume = volume;

    // the 2nd chip has a channel for the noise, one chip does not play the short noise
    bool place = (state.noise_enable && m_outputType != OutputType::DoubleChip &&
        !(m_outputType == OutputType::SingleChip && state.noise_mode));

    // a triangle note and a hit start together while no channel is free:
    // the head of the hit takes channel B, the note waits for it
    bool pulsesOn = (state.pulse[0].enable && state.pulse[1].enable);
    bool noteStarts = (state.triangle_enable && !m_trianglePlayed);
    m_trianglePlayed = state.triangle_enable;
    if (!place || !state.triangle_enable) m_noiseHead = 0;
    else if (pulsesOn && noteStarts && m_noiseAge == 0) m_noiseHead = c_noiseHeadFrames;

    state.noise_channels = 0;
    if (place && !state.triangle_enable)
    {
        // a free channel first: the middle one, as on the NES; a held triangle
        // level gives way, the next triangle note restarts the envelope
        state.noise_channels = ChannelMask(Frame::Channel::B);
        state.triangle_hold = false;
        m_triangleHold = 0;
    }
    else if (place && m_noiseHead > 0)
    {
        --m_noiseHead;
        state.noise_channels = ChannelMask(Frame::Channel::B);
    }
    else if (place && !pulsesOn)
    {
        // the channel of a silent pulse, the same one while both are silent
        uint8_t maskC = ChannelMask(c_pulseChannels[1]);
        bool useC = (state.pulse[0].enable || (!state.pulse[1].enable && m_noiseChannels == maskC));
        state.noise_channels = (useC ? maskC : ChannelMask(c_pulseChannels[0]));
    }
    else if (place)
    {
        // no free channel: the noise is mixed in the pulses, their volumes raised for a quiet one
        state.noise_channels = SharedNoiseChannels(state, state.noise_gain);
        state.noise_shared = true;
    }
    m_noiseChannels = state.noise_channels;

    // the envelope ran on under the head of the hit: it restarts from zero after it,
    // going back to the level it ran to would click; a stopped triangle is not held
    bool interrupted = (state.triangle_enable && state.noise_channels == ChannelMask(Frame::Channel::B));
    if (m_triangleInterrupted && !interrupted)
    {
        if (state.triangle_enable) state.triangle_restart = true;
        state.triangle_hold = false;
        m_triangleHold = 0;
    }
    m_triangleInterrupted = interrupted;
}

uint8_t SimRP2A03::SharedNoiseChannels(const State& state, float& gain) const
{
    gain = 1;
    // the levels the pulses play at, one chip plays every duty as a square
    float levels[2], duties[2];
    for (int i = 0; i < 2; ++i)
    {
        levels[i] = VolumeToLevel(PulseVolume(state.pulse[i].volume));
        duties[i] = (m_outputType == OutputType::AY8930Chip ? c_dutyRatios[state.pulse[i].duty] : 0.5f);
    }

    // a noise masked by the pulses on the NES is mixed in for its attack only
    float target = NoiseLevel(state);
    float masker = std::sqrt(levels[0] * levels[0] + levels[1] * levels[1]);
    if (m_noiseAge >= c_noiseAttackFrames && Decibels(masker / target) > c_noiseMaskingDb) return 0;

    // t AND n (YM) or t OR n (AY8930) of a tone of duty d and the noise keeps half of the
    // tone; the noise parts g * L (g = d or 1 - d) add as one noise over the channels, the
    // products with the tones do not correlate and add d * (1 - d) * L^2 in power each
    const auto NoiseLevelOf = [&](uint8_t mask)
    {
        float parts = 0, products = 0;
        for (int i = 0; i < 2; ++i)
        {
            if (!(mask & ChannelMask(c_pulseChannels[i]))) continue;
            float d = duties[i];
            parts += (m_outputType == OutputType::AY8930Chip ? 1 - d : d) * levels[i];
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
            if (mask & ChannelMask(c_pulseChannels[i])) highest = std::max(highest, levels[i]);
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
            if (mask & ChannelMask(c_pulseChannels[i])) cost += c_toneLossWeight * toneLossDb * levels[i] / louder;
        }
        return cost;
    };

    // the cheapest choice, the last one while it costs not much more
    const uint8_t maskA = ChannelMask(c_pulseChannels[0]);
    const uint8_t maskC = ChannelMask(c_pulseChannels[1]);
    const uint8_t choices[] = { maskA, maskC, uint8_t(maskA | maskC) };
    uint8_t best = choices[0];
    for (uint8_t mask : choices)
    {
        if (Cost(mask) < Cost(best)) best = mask;
    }
    for (uint8_t mask : choices)
    {
        if (mask == m_noiseChannels && Cost(mask) <= Cost(best) + c_noiseHysteresisDb) best = mask;
    }
    gain = Gain(best);
    return best;
}

void SimRP2A03::MixNoise(const State& state, uint8_t& mixer, uint8_t* volumes) const
{
    for (int chan = 0; chan < 3; ++chan)
    {
        if (state.noise_channels & ChannelMask(chan))
        {
            // the AY8930 tone that lets the envelope through would let no noise through
            if (chan == Frame::Channel::B && state.TriangleOn() && m_outputType == OutputType::AY8930Chip)
                mixer |= ChannelMask(chan);

            EnableNoise(mixer, chan);
            if (!state.noise_shared) volumes[chan] = NoiseVolume(state);
            else if (state.noise_gain > 1) volumes[chan] = LevelToVolume(state.noise_gain * VolumeToLevel(volumes[chan]));
        }
    }
}

uint16_t SimRP2A03::YMNoisePeriod(const State& state) const
{
    // the NES noise shifts every period CPU clocks, the PSG noise every 16 * period chip clocks
    return std::min(ConvertPeriod(state.noise_period, 16), uint16_t(0x1F));
}

void SimRP2A03::UpdateAY8930Noise(const State& state, Frame::Registers& regs) const
{
    // the expanded mode noise toggles its output after (LFSR & AND | OR) periods of 8 clocks
    if (ShortNoiseAsTone(state))
    {
        // the AND mask 0 makes it a square of half period OR * period
        uint8_t orMask, period;
        ShortNoiseMasks(state, orMask, period);
        regs.Update(Register::N_AndMask, 0x00);
        regs.Update(Register::N_OrMask,  orMask);
        regs.Update(Register::Period::N, period);
    }
    else
    {
        // the period of 8 * period chip clocks is half the NES shift of period CPU clocks
        uint16_t period = ConvertPeriod(state.noise_period, 16);
        regs.Update(Register::N_AndMask, c_ay8930NoiseAndMask);
        regs.Update(Register::N_OrMask,  0x00);
        regs.Update(Register::Period::N, std::min(period, uint16_t(0xFF)));
    }
}

bool SimRP2A03::ShortNoiseAsTone(const State& state) const
{
    // the short noise is a buzz of pitch CPU / (93 * period), a tone imitates it while
    // the pitch is audible; lower, it is a rattle, closer to the usual noise
    return (state.noise_enable && state.noise_mode &&
        ConvertPeriod(c_shortNoiseSteps * state.noise_period, 16) <= 0x0FFF);
}

void SimRP2A03::ShortNoiseMasks(const State& state, uint8_t& orMask, uint8_t& period) const
{
    // the half period of the buzz, 93 * period / 2 CPU clocks, as OR * period
    // of 8 chip clocks: the pair of the least error is taken
    double target = double(c_shortNoiseSteps) * state.noise_period * m_dstClock / (16.0 * m_srcClock);
    double error = target;
    orMask = period = 1;
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
}

////////////////////////////////////////////////////////////////////////////////

float SimRP2A03::VolumeToLevel(uint8_t volume) const
{
    volume = std::min(volume, uint8_t(m_levelsCount - 1));
    return m_levels[volume * m_levelsStep + m_levelsStep - 1];
}

uint8_t SimRP2A03::LevelToVolume(float level) const
{
    // the nearest level among the ones the chip has
    uint8_t volume = 0;
    for (uint8_t i = 1; i < m_levelsCount; ++i)
    {
        if (std::abs(level - VolumeToLevel(i)) < std::abs(level - VolumeToLevel(volume))) volume = i;
    }
    return volume;
}

float SimRP2A03::PulseLevel(uint8_t volume) const
{
    // the pulse level of the NES mixer, relative to the triangle
    float level = (volume ? 95.88f / (8128.f / volume + 100.f) : 0.f);
    return (level * m_psgTriangleRms / c_nesTriangleRms);
}

uint8_t SimRP2A03::PulseVolume(uint8_t volume) const
{
    return LevelToVolume(PulseLevel(volume));
}

float SimRP2A03::NoiseLevel(const State& state) const
{
    // the noise level of the NES mixer, relative to the triangle
    uint8_t volume = (state.noise_enable ? state.noise_volume : 0);
    float level = (volume ? 159.79f / (12241.f / volume + 100.f) : 0.f);
    level *= (m_psgTriangleRms / c_nesTriangleRms);
    return (ShortNoiseAsTone(state) ? level * ShortNoiseGain(state) : level);
}

uint8_t SimRP2A03::NoiseVolume(const State& state) const
{
    return LevelToVolume(NoiseLevel(state));
}

float SimRP2A03::ShortNoiseGain(const State& state) const
{
    // the steps of the sequence put most of its power above hearing, the square of the
    // same pitch has nearly all of it below: they are leveled by the power heard; the
    // harmonics of the sequence are equal but for the sinc of a step
    const double pi = 3.14159265358979;
    double pitch = double(m_srcClock) / (c_shortNoiseSteps * state.noise_period);
    double sequence = 0, square = 0;
    for (int k = 1; k * pitch < c_hearingLimit; ++k)
    {
        double x = pi * k / c_shortNoiseSteps;
        sequence += (std::sin(x) / x) * (std::sin(x) / x);
        if (k & 1) square += 1.0 / (k * k);
    }
    sequence /= (c_shortNoiseSteps - 1) / 2.0; // of the power of all the harmonics
    square *= 8 / (pi * pi);
    return float(std::sqrt(sequence / square));
}
