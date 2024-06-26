#include "SimRP2A03.h"
#include "stream/Frame.h"
#include "stream/Stream.h"
#include <cassert>

struct SimRP2A03::State
{
    uint16_t pulse1_period;
    uint8_t  pulse1_volume;
    uint8_t  pulse1_duty;
    bool     pulse1_enable;

    uint16_t pulse2_period;
    uint8_t  pulse2_volume;
    uint8_t  pulse2_duty;
    bool     pulse2_enable;

    uint16_t triangle_period;
    bool     triangle_enable;

    uint16_t noise_period;
    uint8_t  noise_volume;
    bool     noise_mode;
    bool     noise_enable;
};

SimRP2A03::SimRP2A03()
    : ChipSim(Type::RP2A03)
    , m_outputType(OutputType::SingleChip)
    , m_noiseAndMask(0)
{
}

void SimRP2A03::ConfigureClock(int srcClock, int dstClock)
{
    RP2A03Apu::Init(44100, srcClock);
    ChipSim::ConfigureClock(srcClock, dstClock);

    // destination clock must be from 1.0 to 2.0 MHz
    float extrapolation = ((dstClock - 1e6f) / 1e6f);
    m_noiseAndMask = uint8_t(0x07 + extrapolation * 0x08 + 0.5f);
}

void SimRP2A03::ConfigureOutput(OutputType outputType)
{
    m_outputType = outputType;
}

void SimRP2A03::Reset()
{
    RP2A03Apu::Reset();
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

    // pulse 1 channel
    state.pulse1_period = m_pulse1.timer_period;
    state.pulse1_volume = m_pulse1.envelope.out;
    state.pulse1_duty   = m_pulse1.duty;
    state.pulse1_enable = (m_pulse1.len_counter && !m_pulse1.sweep.silence && m_pulse1.envelope.out);

    // pulse 2 channel
    state.pulse2_period = m_pulse2.timer_period;
    state.pulse2_volume = m_pulse2.envelope.out;
    state.pulse2_duty   = m_pulse2.duty;
    state.pulse2_enable = (m_pulse2.len_counter && !m_pulse2.sweep.silence && m_pulse2.envelope.out);

    // triangle channel
    state.triangle_period = m_triangle.timer_period;
    state.triangle_enable = (m_triangle.lin_counter > 0 && m_triangle.len_counter > 0 && m_triangle.timer_period >= 2);

    // noise channel
    state.noise_period = m_noise.timer_period;
    state.noise_volume = m_noise.envelope.out;
    state.noise_mode   = m_noise.mode;
    state.noise_enable = (m_noise.len_counter && m_noise.envelope.out);
    assert(state.noise_mode == false);

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

    // Pulse 1 -> Square A
    if (state.pulse1_enable)
    {
        uint16_t period = ConvertPeriod(state.pulse1_period);
        uint8_t  volume = ConvertVolume(state.pulse1_volume);

        EnableTone(mixer, Frame::Channel::A);
        frame[0].Update(Register::Period::A, period);
        frame[0].Update(Register::A_Volume, volume);
    }

    // Pulse 2 -> Square C
    if (state.pulse2_enable)
    {
        uint16_t period = ConvertPeriod(state.pulse2_period);
        uint8_t  volume = ConvertVolume(state.pulse2_volume);

        EnableTone(mixer, Frame::Channel::C);
        frame[0].Update(Register::Period::C, period);
        frame[0].Update(Register::C_Volume, volume);
    }

    // Triangle -> Envelope B
    if (state.triangle_enable)
        EnableTriangleEnvelopeOnChannelB<Register::E_Shape, Register::Period::E>(state, frame);
    else
        DisableTriangleEnvelopeOnChannelB(frame);
    
    // Noise -> automatically chosen channel: A, B, C or A + C
    if (state.noise_enable)
    {
        uint16_t period = ConvertPeriod(state.noise_period) >> 5;
        if (period > 0x1F) period = 0x1F;

        frame[0].Update(Register::Period::N, period);
        DistributeNoiseBetweenChannels(state, frame, mixer);
    }
        
    // Mixer
    {
        frame[0].Update(Register::Mixer, mixer);
    }
}

void SimRP2A03::ConvertToDoubleChip(const State& state, Frame& frame)
{
    uint8_t mixer0 = 0x3F;
    uint8_t mixer1 = 0x3F;

    // Pulse 1 -> Chip 0 Square A + Chip 1 Square A
    if (state.pulse1_enable)
    {
        uint16_t period0 = ConvertPeriod(state.pulse1_period);
        uint8_t  volume  = ConvertVolume(state.pulse1_volume);

        uint16_t period1 = period0;
        if (state.pulse1_duty == 0) period1 ^= 1;
        if (state.pulse1_duty == 1 || state.pulse1_duty == 3) period1 >>= 1;

        if (period1 != period0)
        {
            volume = LevelToVolume(0.5f * VolumeToLevel(volume));

            EnableTone(mixer1, Frame::Channel::A);
            frame[1].Update(Register::Period::A, period1);
            frame[1].Update(Register::A_Volume, volume);
        }

        EnableTone(mixer0, Frame::Channel::A);
        frame[0].Update(Register::Period::A, period0);
        frame[0].Update(Register::A_Volume, volume);
    }

    // Pulse 2 -> Chip 0 Square C + Chip 1 Square C
    if (state.pulse2_enable)
    {
        uint16_t period0 = ConvertPeriod(state.pulse2_period);
        uint8_t  volume  = ConvertVolume(state.pulse2_volume);

        uint16_t period1 = period0;
        if (state.pulse2_duty == 0) period1 ^= 1;
        if (state.pulse2_duty == 1 || state.pulse2_duty == 3) period1 >>= 1;

        if (period1 != period0)
        {
            volume = LevelToVolume(0.5f * VolumeToLevel(volume));

            EnableTone(mixer1, Frame::Channel::C);
            frame[1].Update(Register::Period::C, period1);
            frame[1].Update(Register::C_Volume, volume);
        }

        EnableTone(mixer0, Frame::Channel::C);
        frame[0].Update(Register::Period::C, period0);
        frame[0].Update(Register::C_Volume, volume);
    }

    // Triangle -> Chip 0 Envelope in Channel B
    if (state.triangle_enable)
        EnableTriangleEnvelopeOnChannelB<Register::E_Shape, Register::Period::E>(state, frame);
    else
        DisableTriangleEnvelopeOnChannelB(frame);
    
    // Noise -> Chip 1 Noise in Channel B
    if (state.noise_enable)
    {
        uint16_t period = ConvertPeriod(state.noise_period) >> 5;
        uint8_t  volume = ConvertVolume(state.noise_volume);
        if (period > 0x1F) period = 0x1F;

        EnableNoise(mixer1, Frame::Channel::B);
        frame[1].Update(Register::Period::N, period);
        frame[1].Update(Register::B_Volume, volume);
    }
    
    // Mixers
    {
        frame[0].Update(Register::Mixer, mixer0);
        frame[1].Update(Register::Mixer, mixer1);
    }
}

void SimRP2A03::ConvertToAY8930Chip(const State& state, Frame& frame)
{
    uint8_t mixer = 0x3F;

    // go to expanded mode
    if (!frame[0].IsExpMode()) frame[0].SetExpMode();

    // Pulse 1 -> Square A
    if (state.pulse1_enable)
    {
        uint16_t period = ConvertPeriod(state.pulse1_period << 1);
        uint8_t  volume = ConvertVolume(state.pulse1_volume);
        uint8_t  duty = 0x02 + state.pulse1_duty;

        EnableTone(mixer, Frame::Channel::A);
        frame[0].Update(Register::Period::A, period);
        frame[0].Update(Register::A_Volume, volume);
        frame[0].Update(Register::A_Duty, duty);
    }

    // Pulse 2 -> Square C
    if (state.pulse2_enable)
    {
        uint16_t period = ConvertPeriod(state.pulse2_period << 1);
        uint8_t  volume = ConvertVolume(state.pulse2_volume);
        uint8_t  duty = 0x02 + state.pulse2_duty;

        EnableTone(mixer, Frame::Channel::C);
        frame[0].Update(Register::Period::C, period);
        frame[0].Update(Register::C_Volume, volume);
        frame[0].Update(Register::C_Duty, duty);
    }

    // Triangle -> Envelope B
    if (state.triangle_enable)
    {
        // workaround for mixer
        EnableTone(mixer, Frame::Channel::B);
        frame[0].Update(Register::Period::B, uint16_t(0));
        frame[0].Update(Register::B_Duty, 0x08);

        // enable triangle envelope
        EnableTriangleEnvelopeOnChannelB<Register::EB_Shape, Register::Period::EB>(state, frame);
    }
    else
        DisableTriangleEnvelopeOnChannelB(frame);

    // Noise -> automatically chosen channel: A, B, C or A + C
    if (state.noise_enable)
    {
        frame[0].Update(Register::N_AndMask, m_noiseAndMask);
        frame[0].Update(Register::N_OrMask,  0x00);

        uint16_t period = ConvertPeriod(state.noise_period) >> 5;
        frame[0].Update(Register::Period::N, period);
        DistributeNoiseBetweenChannels(state, frame, mixer);
    }

    // Mixers
    {
        frame[0].Update(Register::Mixer, mixer);
    }
}

template<Register::Value shape_reg, Register::Period peiod_reg>
void SimRP2A03::EnableTriangleEnvelopeOnChannelB(const State& state, Frame& frame)
{
    uint16_t period = ConvertPeriod(state.triangle_period) >> 3;
    uint8_t  volume = (frame[0].Read(Register::B_Volume) | frame[0].emask());

    if (frame[0].GetData(shape_reg) != 0x0A)
        frame[0].Update (shape_reg, 0x0A);

    frame[0].Update(peiod_reg, period);
    frame[0].Update(Register::B_Volume, volume);
}

void SimRP2A03::DisableTriangleEnvelopeOnChannelB(Frame& frame)
{
    uint8_t volume = (frame[0].Read(Register::B_Volume) & frame[0].vmask());
    frame[0].Update(Register::B_Volume, volume);
}

void SimRP2A03::DistributeNoiseBetweenChannels(const State& state, Frame& frame, uint8_t& mixer)
{
    int volumeN = ConvertVolume(state.noise_volume);
    if (!state.triangle_enable)
    {
        EnableNoise(mixer, Frame::Channel::B);
        frame[0].Update(Register::B_Volume, volumeN);
    }
    else
    {
        if (!state.pulse1_enable) frame[0].Update(Register::A_Volume, volumeN);
        if (!state.pulse2_enable) frame[0].Update(Register::C_Volume, volumeN);

        auto levelN = VolumeToLevel(volumeN);
        auto levelA = VolumeToLevel(frame[0].Read(Register::A_Volume));
        auto levelC = VolumeToLevel(frame[0].Read(Register::C_Volume));

        const auto Delta = [](float v, float a, float b)
        {   return std::sqrtf(std::abs(v * v - (a * a + b * b))); };

        auto deltaA = Delta(levelN, levelA, 0);
        auto deltaC = Delta(levelN, levelC, 0);
        auto delta2 = Delta(levelN, levelA, levelC);
        auto delta_ = std::min(std::min(deltaA, deltaC), delta2);

        if (delta_ == deltaA) EnableNoise(mixer, Frame::Channel::A);
        else 
        if (delta_ == deltaC) EnableNoise(mixer, Frame::Channel::C);
        else
        {
            EnableNoise(mixer, Frame::Channel::A);
            EnableNoise(mixer, Frame::Channel::C);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

namespace
{
    const float c_volumeToLevel[] =
    {
        0.000f, 0.000f, 0.005f, 0.008f, 0.011f, 0.014f, 0.017f, 0.020f,
        0.024f, 0.030f, 0.035f, 0.040f, 0.049f, 0.058f, 0.068f, 0.078f,
        0.093f, 0.111f, 0.130f, 0.148f, 0.177f, 0.212f, 0.246f, 0.281f,
        0.334f, 0.400f, 0.467f, 0.534f, 0.635f, 0.758f, 0.880f, 1.000f
    };
}

float SimRP2A03::VolumeToLevel(uint8_t volume) const
{
    if (m_outputType != OutputType::AY8930Chip)
    {
        volume <<= 1; ++volume;
    }
    return c_volumeToLevel[volume &= 0x1F];
}

uint8_t SimRP2A03::LevelToVolume(float level) const
{
    level = std::max(0.f, std::min(1.f, level));

    auto index = 0;
    auto delta = std::abs(level - c_volumeToLevel[0]);
    for (int i = 1; i < 32; ++i)
    {
        auto d = std::abs(level - c_volumeToLevel[i]);
        if (d < delta) { delta = d; index = i; }
    }

    if (m_outputType != OutputType::AY8930Chip)
    {
        index >>= 1;
    }
    return uint8_t(index);
}

uint8_t SimRP2A03::ConvertVolume(uint8_t volume) const
{
    return LevelToVolume(float(volume) / 15.f);
}
