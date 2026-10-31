#include "SimRP2A03.h"
#include "stream/Frame.h"
#include "stream/Stream.h"
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

    // frames the triangle level is held after it stops: less than
    // one envelope step at the longest period (0.3 s at 1.77 MHz)
    const int c_triangleHoldFrames = 15;

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
}

SimRP2A03::SimRP2A03()
    : ChipSim(Type::RP2A03)
    , m_noiseAndMask(0)
    , m_triangleHold(0)
{
    ConfigureOutput(OutputType::SingleChip, Chip::Model::YM2149);
}

void SimRP2A03::ConfigureClock(int srcClock, int dstClock)
{
    RP2A03Apu::Init(44100, srcClock);
    ChipSim::ConfigureClock(srcClock, dstClock);

    // destination clock must be from 1.0 to 2.0 MHz
    float extrapolation = ((dstClock - 1e6f) / 1e6f);
    m_noiseAndMask = uint8_t(0x07 + extrapolation * 0x08 + 0.5f);
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

    // Noise -> automatically chosen channel: A, B, C or A + C;
    // the short noise is not played, it only spoils the tones and the noise
    if (state.noise_enable && !state.noise_mode)
    {
        // the NES noise shifts every period CPU clocks,
        // the PSG noise shifts every 16 * period chip clocks
        uint16_t period = ConvertPeriod(state.noise_period, 16);
        if (period > 0x1F) period = 0x1F;

        frame[0].Update(Register::Period::N, period);
        DistributeNoiseBetweenChannels(state, mixer, volumes);
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
            int chan = c_pulseChannels[i];
            uint16_t period0 = ConvertPeriod(pulse.period + 1);
            uint8_t  volume  = PulseVolume(pulse.volume);

            // the 2nd chip imitates the duty: a detuned tone for 12.5%,
            // an octave up for 25% and 75%, both chips at half the level
            uint16_t period1 = period0;
            if (pulse.duty == 0) period1 ^= 1;
            if (pulse.duty == 1 || pulse.duty == 3) period1 >>= 1;

            if (period1 != period0)
            {
                volume = LevelToVolume(0.5f * VolumeToLevel(volume));

                EnableTone(mixer1, chan);
                frame[1].Update(Register::t_period[chan], period1);
                volumes1[chan] = volume;
            }

            EnableTone(mixer0, chan);
            frame[0].Update(Register::t_period[chan], period0);
            volumes0[chan] = volume;
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
        volumes1[Frame::Channel::B] = ShortNoiseVolume(state);
    }

    // Noise -> Chip 1 Noise in Channel B
    else if (state.noise_enable)
    {
        uint16_t period = ConvertPeriod(state.noise_period, 16);
        if (period > 0x1F) period = 0x1F;

        EnableNoise(mixer1, Frame::Channel::B);
        frame[1].Update(Register::Period::N, period);
        volumes1[Frame::Channel::B] = NoiseVolume(state.noise_volume);
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

    // Noise -> automatically chosen channel: A, B, C or A + C
    if (state.noise_enable)
    {
        // the expanded mode noise toggles its output after (LFSR & AND | OR) periods
        if (ShortNoiseAsTone(state))
        {
            // the AND mask 0 makes it a square of half period OR * period * 8 clocks
            uint8_t orMask, period;
            ShortNoiseMasks(state, orMask, period);
            frame[0].Update(Register::N_AndMask, 0x00);
            frame[0].Update(Register::N_OrMask,  orMask);
            frame[0].Update(Register::Period::N, period);
        }
        else
        {
            // a random count of periods set by the masks, so it has its own divider
            frame[0].Update(Register::N_AndMask, m_noiseAndMask);
            frame[0].Update(Register::N_OrMask,  0x00);
            frame[0].Update(Register::Period::N, ConvertPeriod(state.noise_period, 32));
        }
        DistributeNoiseBetweenChannels(state, mixer, volumes);
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
        // workaround for mixer
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

bool SimRP2A03::ShortNoiseAsTone(const State& state) const
{
    // the short noise is a buzz of pitch CPU / (93 * period), a tone imitates it while
    // the pitch is audible; lower, it is a rattle, closer to the usual noise
    return (state.noise_enable && state.noise_mode &&
        ConvertPeriod(c_shortNoiseSteps * state.noise_period, 16) <= 0x0FFF);
}

void SimRP2A03::ShortNoiseMasks(const State& state, uint8_t& orMask, uint8_t& period) const
{
    // the half period of the buzz is 93 * period / 2 CPU clocks, OR * period
    // of 8 chip clocks give it: the pair with the least error is taken
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

void SimRP2A03::DistributeNoiseBetweenChannels(const State& state, uint8_t& mixer, uint8_t* volumes)
{
    uint8_t volumeN = (ShortNoiseAsTone(state) ? ShortNoiseVolume(state) : NoiseVolume(state.noise_volume));
    if (!state.TriangleOn())
    {
        EnableNoise(mixer, Frame::Channel::B);
        volumes[Frame::Channel::B] = volumeN;
    }
    else
    {
        // a channel without a pulse takes the noise with its own volume
        uint8_t volumeA = (state.pulse[0].enable ? volumes[Frame::Channel::A] : volumeN);
        uint8_t volumeC = (state.pulse[1].enable ? volumes[Frame::Channel::C] : volumeN);

        auto levelN = VolumeToLevel(volumeN);
        auto levelA = VolumeToLevel(volumeA);
        auto levelC = VolumeToLevel(volumeC);

        const auto Delta = [](float v, float a, float b)
        {   return std::sqrtf(std::abs(v * v - (a * a + b * b))); };

        auto deltaA = Delta(levelN, levelA, 0);
        auto deltaC = Delta(levelN, levelC, 0);
        auto delta2 = Delta(levelN, levelA, levelC);
        auto delta_ = std::min(std::min(deltaA, deltaC), delta2);

        const auto UseChannel = [&](int chan, uint8_t volume)
        {
            EnableNoise(mixer, chan);
            volumes[chan] = volume;
        };

        if (delta_ == deltaA) UseChannel(Frame::Channel::A, volumeA);
        else
        if (delta_ == deltaC) UseChannel(Frame::Channel::C, volumeC);
        else
        {
            UseChannel(Frame::Channel::A, volumeA);
            UseChannel(Frame::Channel::C, volumeC);
        }
    }
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

uint8_t SimRP2A03::PulseVolume(uint8_t volume) const
{
    // the pulse level of the NES mixer, relative to the triangle
    float level = (volume ? 95.88f / (8128.f / volume + 100.f) : 0.f);
    return LevelToVolume(level * m_psgTriangleRms / c_nesTriangleRms);
}

uint8_t SimRP2A03::NoiseVolume(uint8_t volume, float gain) const
{
    // the noise level of the NES mixer, relative to the triangle
    float level = (volume ? 159.79f / (12241.f / volume + 100.f) : 0.f);
    return LevelToVolume(gain * level * m_psgTriangleRms / c_nesTriangleRms);
}

uint8_t SimRP2A03::ShortNoiseVolume(const State& state) const
{
    // the short steps of the sequence put most of its power above hearing, the square
    // has nearly all of it below: they are leveled by the power of their harmonics there;
    // the harmonics of the sequence are equal but for the sinc of a step
    const double pi = 3.14159265358979;
    double pitch = double(m_srcClock) / (c_shortNoiseSteps * state.noise_period);
    double sequence = 0, square = 0;
    for (int k = 1; k * pitch < c_hearingLimit; ++k)
    {
        double x = pi * k / c_shortNoiseSteps;
        sequence += (std::sin(x) / x) * (std::sin(x) / x);
        if (k & 1) square += 1.0 / (k * k);
    }
    sequence /= (c_shortNoiseSteps - 1) / 2.0; // of the power of all harmonics
    square *= 8 / (pi * pi);
    return NoiseVolume(state.noise_volume, float(std::sqrt(sequence / square)));
}
