#include "RP2A03Apu.h"
#include "RP2A03Cpu.h"
#include <string>
#include <cmath>
#include <initializer_list>

#define BIT(v, b) (((v>>b)&1) == 1)

namespace
{
    // impulses of the band-limited output (as blip_buf of the popular emulators),
    // Blackman windowed sinc, one per fraction of an output sample, each sums to 1
    const int BLIP_TAPS   = 32; // impulse length in output samples
    const int BLIP_PHASES = 64; // impulse phases per output sample

    struct BlipKernel
    {
        double taps[BLIP_PHASES][BLIP_TAPS];

        BlipKernel()
        {
            const double pi = 3.14159265358979323846;
            const double cutoff = 0.45; // of the sample rate, below the Nyquist frequency
            const double half = BLIP_TAPS / 2;

            for (int phase = 0; phase < BLIP_PHASES; ++phase)
            {
                double sum = 0;
                for (int tap = 0; tap < BLIP_TAPS; ++tap)
                {
                    double x = (tap - half - double(phase) / BLIP_PHASES);
                    double sinc = (x ? std::sin(2 * pi * cutoff * x) / (pi * x) : 2 * cutoff);
                    double window = (std::abs(x) < half ? 0.42 + 0.5 * std::cos(pi * x / half) + 0.08 * std::cos(2 * pi * x / half) : 0);
                    sum += (taps[phase][tap] = sinc * window);
                }
                for (double& tap : taps[phase]) tap /= sum;
            }
        }
    };

    const BlipKernel& GetBlipKernel()
    {
        static const BlipKernel kernel;
        return kernel;
    }
}

const uint8_t pulse_seq[4][8] =
{
    { 0, 1, 0, 0, 0, 0, 0, 0 },  // 12.5%
    { 0, 1, 1, 0, 0, 0, 0, 0 },  // 25.0%
    { 0, 1, 1, 1, 1, 0, 0, 0 },  // 50.0%
    { 1, 0, 0, 1, 1, 1, 1, 1 }   // 25.0% negated
};

const uint8_t triangle_seq[32] =
{
    0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
    0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

const uint16_t noise_periods_ntsc[16] =
{
     4, 8, 16, 32, 64, 96, 128, 160,
     202, 254, 380, 508, 762, 1016, 2034, 4068
};

const uint16_t noise_periods_pal[16] =
{
     4, 7, 14, 30, 60, 88, 118, 148,
     188, 236, 354, 472, 708,  944, 1890, 3778
};

const uint16_t dmc_periods_ntsc[16] =
{
    428, 380, 340, 320, 286, 254, 226, 214,
    190, 160, 142, 128, 106,  84,  72,  54
};

const uint16_t dmc_periods_pal[16] =
{
    398, 354, 316, 298, 276, 236, 210, 198,
    176, 148, 132, 118,  98,  78,  66,  50
};

// frame counter steps in APU cycles: 1st, 2nd, 3rd, last of 4 step and 5 step modes
const uint16_t frame_steps_ntsc[5] = { 3728, 7456, 11185, 14914, 18640 };
const uint16_t frame_steps_pal [5] = { 4156, 8313, 12469, 16626, 20782 };

const uint8_t length_lut[32] =
{
    10, 254, 20,  2, 40,  4, 80,  6,
    160,  8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22,
    192, 24, 72, 26, 16, 28, 32, 30
};

////////////////////////////////////////////////////////////////////////////////

RP2A03Apu::RP2A03Apu()
    : m_noisePeriods(nullptr)
    , m_dmcPeriods(nullptr)
    , m_frameSteps(nullptr)
    , m_cpuCycles(0)
    , m_oddCycle(false)
{
    Init(44100, (int)RP2A03Cpu::Clock::NTSC);
}

void RP2A03Apu::Init(int sampleRate, int cpuClock)
{
    m_cpuClock = cpuClock;
    m_cpuCyclesPerSample = (uint32_t)((((uint64_t)cpuClock) << 16) / sampleRate);
    m_samplesPerCycle = double(sampleRate) / cpuClock;

    // the clock in files is not always exact (1789772 Hz is common),
    // so the closest of NTSC and PAL is taken, Dendy goes as NTSC
    if (m_cpuClock > ((uint32_t)RP2A03Cpu::Clock::NTSC + (uint32_t)RP2A03Cpu::Clock::PAL) / 2)
    {
        m_noisePeriods = noise_periods_ntsc;
        m_dmcPeriods = dmc_periods_ntsc;
        m_frameSteps = frame_steps_ntsc;
    }
    else
    {
        m_noisePeriods = noise_periods_pal;
        m_dmcPeriods = dmc_periods_pal;
        m_frameSteps = frame_steps_pal;
    }
    Reset();
}

void RP2A03Apu::Reset()
{
    memset(&m_pulse1,   0, sizeof(m_pulse1)   );
    memset(&m_pulse2,   0, sizeof(m_pulse2)   );
    memset(&m_triangle, 0, sizeof(m_triangle) );
    memset(&m_noise,    0, sizeof(m_noise)    );
    memset(&m_dmc,      0, sizeof(m_dmc)      );
    memset(&m_frame,    0, sizeof(m_frame)    );
    m_cpuCycles = 0;
    m_oddCycle = false;
    m_blip = {};

    for (int i = PULSE1DUTYVOL; i <= DMCLENGTH; ++i) Write(i, 0x00);
    Write(STATUS, 0x0f);
    Write(FRAMECNTR, 0x40);
    m_noise.shiftreg = 1;
    m_dmc.bitsleft = 8;
    m_dmc.silence = true;
}

void RP2A03Apu::Write(uint16_t addr, uint8_t data)
{
    switch (addr)
    {
    case PULSE1DUTYVOL:
        m_pulse1.duty = (data >> 6);
        m_pulse1.envelope.loop_halt = BIT(data, 5);
        m_pulse1.envelope.const_vol = BIT(data, 4);
        m_pulse1.envelope.vol_period = (data & 0xF);
        UpdateEnvelopeOut(m_pulse1.envelope);
        break;

    case PULSE1SWEEP:
        m_pulse1.sweep.enable = BIT(data, 7);
        m_pulse1.sweep.period = ((data >> 4) & 0x07);
        m_pulse1.sweep.negate = BIT(data, 3);
        m_pulse1.sweep.shift = (data & 0x07);
        m_pulse1.sweep.reload = true;
        CalculateSweep(m_pulse1, true);
        break;

    case PULSE1TMRL:
        m_pulse1.timer_period = ((m_pulse1.timer_period & 0x0700) | data);
        CalculateSweep(m_pulse1, true);
        break;

    case PULSE1TMRH:
        m_pulse1.timer_period = ((m_pulse1.timer_period & 0x00FF) | ((data & 0x07) << 8));
        CalculateSweep(m_pulse1, true);
        if (m_pulse1.enabled) m_pulse1.len_counter = length_lut[data >> 3];
        m_pulse1.phase = 0;
        m_pulse1.envelope.start = true;
        break;

    case PULSE2DUTYVOL:
        m_pulse2.duty = (data >> 6);
        m_pulse2.envelope.loop_halt = BIT(data, 5);
        m_pulse2.envelope.const_vol = BIT(data, 4);
        m_pulse2.envelope.vol_period = (data & 0xF);
        UpdateEnvelopeOut(m_pulse2.envelope);
        break;

    case PULSE2SWEEP:
        m_pulse2.sweep.enable = BIT(data, 7);
        m_pulse2.sweep.period = ((data >> 4) & 0x07);
        m_pulse2.sweep.negate = BIT(data, 3);
        m_pulse2.sweep.shift = (data & 0x07);
        m_pulse2.sweep.reload = true;
        CalculateSweep(m_pulse2, false);
        break;

    case PULSE2TMRL:
        m_pulse2.timer_period = ((m_pulse2.timer_period & 0x0700) | data);
        CalculateSweep(m_pulse2, false);
        break;

    case PULSE2TMRH:
        m_pulse2.timer_period = ((m_pulse2.timer_period & 0x00FF) | ((data & 0x07) << 8));
        CalculateSweep(m_pulse2, false);
        if (m_pulse2.enabled) m_pulse2.len_counter = length_lut[data >> 3];
        m_pulse2.phase = 0;
        m_pulse2.envelope.start = true;
        break;

    case TRICOUNTER:
        // only $400B sets the reload flag
        m_triangle.control = BIT(data, 7);
        m_triangle.lin_reload = (data & 0x7f);
        break;

    case TRITMRL:
        m_triangle.timer_period = ((m_triangle.timer_period & 0x0700) | data);
        break;

    case TRITMRH:
        m_triangle.timer_period = ((m_triangle.timer_period & 0x00FF) | (data & 0x07) << 8);
        if (m_triangle.enabled) m_triangle.len_counter = length_lut[data >> 3];
        m_triangle.reload = true;
        break;

    case NOISEVOL:
        m_noise.envelope.loop_halt = BIT(data, 5);
        m_noise.envelope.const_vol = BIT(data, 4);
        m_noise.envelope.vol_period = (data & 0x0F);
        UpdateEnvelopeOut(m_noise.envelope);
        break;

    case NOISEPERIOD:
        m_noise.mode = BIT(data, 7);
        m_noise.period_lut = (data & 0x0F);
        m_noise.timer_period = m_noisePeriods[m_noise.period_lut];
        break;

    case NOISELCL:
        if (m_noise.enabled) m_noise.len_counter = length_lut[data >> 3];
        m_noise.envelope.start = true;
        break;

    case DMCIRQ:
        m_dmc.irq_enable = BIT(data, 7);
        if (!m_dmc.irq_enable) m_dmc.irq = false;
        m_dmc.loop = BIT(data, 6);
        m_dmc.rate = (data & 0x0f);
        m_dmc.rate_actual = m_dmcPeriods[m_dmc.rate];
        break;

    case DMCCOUNTER:
        m_dmc.counter = (data & 0x7f);
        break;

    case DMCADDR:
        m_dmc.address = ((data << 6) | 0xC000);
        break;

    case DMCLENGTH:
        m_dmc.length = ((data << 4) | 1);
        break;

    case STATUS:
        m_dmc.control = BIT(data, 4);
        m_noise.enabled = BIT(data, 3);
        m_triangle.enabled = BIT(data, 2);
        m_pulse2.enabled = BIT(data, 1);
        m_pulse1.enabled = BIT(data, 0);

        // a disabled channel loses its length counter at once
        if (!m_pulse1.enabled) m_pulse1.len_counter = 0;
        if (!m_pulse2.enabled) m_pulse2.len_counter = 0;
        if (!m_triangle.enabled) m_triangle.len_counter = 0;
        if (!m_noise.enabled) m_noise.len_counter = 0;

        // enabling DMC restarts a finished sample, disabling stops it
        if (!m_dmc.control) m_dmc.bytesleft = 0;
        else if (!m_dmc.bytesleft)
        {
            m_dmc.addresscur = m_dmc.address;
            m_dmc.bytesleft = m_dmc.length;
        }
        m_dmc.irq = false;
        break;

    case FRAMECNTR:
        m_frame.count = 0;
        m_frame.mode = BIT(data, 7);
        m_frame.int_inhibit = BIT(data, 6);
        if (m_frame.int_inhibit) m_frame.interrupt = false;

        // only the 5-step mode clocks the units at once
        m_frame.updated = m_frame.mode;
        break;
    }
}

uint8_t RP2A03Apu::Read(uint16_t addr)
{
    if (addr == STATUS)
    {
        uint8_t status = (m_dmc.irq << 7
            | m_frame.interrupt << 6
            | (m_dmc.bytesleft > 0) << 4
            | (m_noise.len_counter > 0) << 3
            | (m_triangle.len_counter > 0) << 2
            | (m_pulse2.len_counter > 0) << 1
            | (m_pulse1.len_counter > 0));

        // reading clears the frame interrupt flag
        m_frame.interrupt = false;
        return status;
    }
    return 0;
}

void RP2A03Apu::WriteMemory(uint16_t addr, const uint8_t* data, uint32_t size)
{
    for (uint32_t i = 0; i < size; ++i, ++addr)
    {
        if (addr & 0x8000) m_memory[addr & 0x7FFF] = data[i];
    }
}

int32_t RP2A03Apu::Output()
{
    // run the CPU cycles of one output sample, every change
    // of the mixer output is added as a band-limited impulse
    for (; m_blip.time < 1.0; m_blip.time += m_samplesPerCycle)
    {
        Process(1);
        double level = Mix();
        if (level != m_blip.level)
        {
            AddDelta(m_blip.time, level - m_blip.level);
            m_blip.level = level;
        }
    }
    m_blip.time -= 1.0;

    // the sample integrates the impulses that reached it
    m_blip.sum += m_blip.buffer[m_blip.index];
    m_blip.buffer[m_blip.index] = 0;
    m_blip.index = (m_blip.index + 1) & (BLIP_SIZE - 1);

    // the mixer output of 0..1 is centered to the full int32 range
    double output = (m_blip.sum - 0.5) * 4294967295.0;
    return int32_t(std::fmax(-2147483647.0, std::fmin(2147483647.0, output)));
}

void RP2A03Apu::AddDelta(double time, double delta)
{
    static_assert(BLIP_TAPS <= BLIP_SIZE && !(BLIP_SIZE & (BLIP_SIZE - 1)), "the ring holds an impulse");
    const auto& taps = GetBlipKernel().taps[int(time * BLIP_PHASES) & (BLIP_PHASES - 1)];
    for (int tap = 0; tap < BLIP_TAPS; ++tap)
    {
        m_blip.buffer[(m_blip.index + tap) & (BLIP_SIZE - 1)] += delta * taps[tap];
    }
}

uint8_t RP2A03Apu::PulseOutput(const Pulse& pulse) const
{
    bool audible = (pulse.len_counter && !pulse.sweep.silence);
    return (audible ? pulse_seq[pulse.duty][pulse.phase] * pulse.envelope.out : 0);
}

double RP2A03Apu::Mix() const
{
    uint8_t pulse1 = PulseOutput(m_pulse1);
    uint8_t pulse2 = PulseOutput(m_pulse2);

    // a halted or ultrasonic triangle holds its level, as in blargg's Nes_Apu
    uint8_t triangle = triangle_seq[m_triangle.phase];

    // the noise is muted while bit 0 of the shift register is set
    uint8_t noise = !(m_noise.shiftreg & 1) * m_noise.envelope.out * (m_noise.len_counter > 0);
    uint8_t dmc = m_dmc.counter;

    // nonlinear mixer, the formulas used by Mesen
    double pulse = pulse1 + pulse2;
    double tnd = triangle / 8227.0 + noise / 12241.0 + dmc / 22638.0;
    double pulseOut = (pulse ? 95.88 / (8128.0 / pulse + 100.0) : 0);
    double tndOut = (tnd ? 159.79 / (1.0 / tnd + 100.0) : 0);
    return (pulseOut + tndOut);
}

void RP2A03Apu::Process(uint32_t cpu_cycles)
{
    for (uint32_t cycle = 0; cycle < cpu_cycles; ++cycle)
    {
        // an APU cycle is every other CPU cycle, the parity is
        // kept between the calls so that no APU cycle is lost
        bool apuCycle = m_oddCycle;
        m_oddCycle = !m_oddCycle;

        if (m_frame.updated)
        {
            // writing $4017 in the 5 step mode clocks the units at once
            UpdateQuarterFrame();
            UpdateHalfFrame();
            m_frame.updated = false;
        }

        if (apuCycle)
        {
            // frame counter: quarter frames on steps 1-3 and the last one, half frames
            // on step 2 and the last one; the last is the 4th or 5th by the mode
            const uint16_t* steps = m_frameSteps;
            uint16_t last = steps[m_frame.mode ? 4 : 3];

            if (m_frame.count == steps[0] || m_frame.count == steps[2])
            {
                UpdateQuarterFrame();
            }
            else if (m_frame.count == steps[1] || m_frame.count == last)
            {
                UpdateQuarterFrame();
                UpdateHalfFrame();
            }

            if (m_frame.count == last)
            {
                m_frame.count = 0;
                if (!m_frame.mode && !m_frame.int_inhibit) m_frame.interrupt = true;
            }
            else ++m_frame.count;

            // process timers for pulse waves
            if (m_pulse1.timer) --m_pulse1.timer;
            else
            {
                m_pulse1.timer = m_pulse1.timer_period;
                if (m_pulse1.phase) --m_pulse1.phase;
                else m_pulse1.phase = 7;
            }

            if (m_pulse2.timer) --m_pulse2.timer;
            else
            {
                m_pulse2.timer = m_pulse2.timer_period;
                if (m_pulse2.phase) --m_pulse2.phase;
                else m_pulse2.phase = 7;
            }
        }

        // noise periods are given in CPU cycles
        if (m_noise.timer) --m_noise.timer;
        else
        {
            m_noise.timer = m_noise.timer_period - 1;
            UpdateNoise();
        }

        // DMC memory reader: fills the empty sample buffer
        if (!m_dmc.buffered && m_dmc.bytesleft)
        {
            m_dmc.sample = CpuRead(m_dmc.addresscur);
            m_dmc.buffered = true;
            m_dmc.addresscur = (m_dmc.addresscur == 0xFFFF ? 0x8000 : m_dmc.addresscur + 1);

            if (!(--m_dmc.bytesleft))
            {
                if (m_dmc.loop)
                {
                    m_dmc.addresscur = m_dmc.address;
                    m_dmc.bytesleft = m_dmc.length;
                }
                else if (m_dmc.irq_enable)
                    m_dmc.irq = true;
            }
        }

        // DMC output unit, periods are given in CPU cycles
        if (m_dmc.timer) --m_dmc.timer;
        else
        {
            m_dmc.timer = m_dmc.rate_actual - 1;

            if (!m_dmc.silence)
            {
                if ((m_dmc.counter > 1) && !(m_dmc.shiftreg & 1))
                    m_dmc.counter -= 2;
                else if ((m_dmc.counter < 126) && (m_dmc.shiftreg & 1))
                    m_dmc.counter += 2;
            }
            m_dmc.shiftreg >>= 1;

            // a new output cycle starts after 8 bits
            if (!(--m_dmc.bitsleft))
            {
                m_dmc.bitsleft = 8;
                if (!m_dmc.buffered) m_dmc.silence = true;
                else
                {
                    m_dmc.silence = false;
                    m_dmc.shiftreg = m_dmc.sample;
                    m_dmc.buffered = false;
                }
            }
        }

        // clocked on every cycle, an ultrasonic triangle is not clocked to avoid pops
        if (m_triangle.lin_counter > 0 && m_triangle.len_counter > 0 && m_triangle.timer_period >= 2)
        {
            if (m_triangle.timer) --m_triangle.timer;
            else
            {
                m_triangle.timer = m_triangle.timer_period;
                if (m_triangle.phase) --m_triangle.phase;
                else m_triangle.phase = 31;
            }
        }
    }
}

void RP2A03Apu::CalculateSweep(Pulse& pulse, bool onesComplement)
{
    // pulse 1 negates in ones' complement, so it subtracts one more;
    // the channel is muted by the target even when the sweep is off
    int change = (pulse.timer_period >> pulse.sweep.shift);
    int target = pulse.sweep.negate
        ? pulse.timer_period - change - onesComplement
        : pulse.timer_period + change;
    pulse.sweep.target = uint16_t(target < 0 ? 0 : target);
    pulse.sweep.silence = ((pulse.timer_period < 8) || (pulse.sweep.target > 0x7FF));
}

void RP2A03Apu::ClockSweep(Pulse& pulse, bool onesComplement)
{
    // the divider counts period + 1 half frames,
    // the period is not updated while the channel is muted
    if (!pulse.sweep.divider && pulse.sweep.enable && pulse.sweep.shift && !pulse.sweep.silence)
    {
        pulse.timer_period = pulse.sweep.target;
        CalculateSweep(pulse, onesComplement);
    }
    if (!pulse.sweep.divider || pulse.sweep.reload)
    {
        pulse.sweep.divider = pulse.sweep.period;
        pulse.sweep.reload = false;
    }
    else --pulse.sweep.divider;
}

void RP2A03Apu::UpdateEnvelopeOut(Envelope& envelope)
{
    // the volume follows the registers at once, not only on the frame counter
    envelope.out = (envelope.const_vol ? envelope.vol_period : envelope.counter);
}

void RP2A03Apu::UpdateQuarterFrame()
{
    // process envelopes (pulses and noise)
    for (Envelope* envelope : { &m_pulse1.envelope, &m_pulse2.envelope, &m_noise.envelope })
    {
        if (envelope->start)
        {
            envelope->start = false;
            envelope->counter = 0xF;
            envelope->divider = (envelope->vol_period + 1);
        }
        else
        {
            if (!(--envelope->divider))
            {
                if (envelope->counter) --envelope->counter;
                else if (envelope->loop_halt) envelope->counter = 0xF;
                envelope->divider = (envelope->vol_period + 1);
            }
        }
        UpdateEnvelopeOut(*envelope);
    }

    // process triangle's linear counter
    if (m_triangle.reload) m_triangle.lin_counter = m_triangle.lin_reload;
    else if (m_triangle.lin_counter) --m_triangle.lin_counter;
    if (!m_triangle.control) m_triangle.reload = false;
}

void RP2A03Apu::UpdateHalfFrame()
{
    // length counters, halted by the envelope loop flag or the triangle's control
    // flag; those of disabled channels are kept at zero by $4015
    const auto ClockLength = [](uint8_t& counter, bool halt) { if (counter && !halt) --counter; };
    ClockLength(m_pulse1.len_counter, m_pulse1.envelope.loop_halt);
    ClockLength(m_pulse2.len_counter, m_pulse2.envelope.loop_halt);
    ClockLength(m_triangle.len_counter, m_triangle.control);
    ClockLength(m_noise.len_counter, m_noise.envelope.loop_halt);

    ClockSweep(m_pulse1, true);
    ClockSweep(m_pulse2, false);
}

void RP2A03Apu::UpdateNoise()
{
    // the short mode takes the feedback from bit 6 instead of bit 1
    uint16_t feedback = (m_noise.shiftreg & 1) ^ ((m_noise.shiftreg >> (m_noise.mode ? 6 : 1)) & 1);
    m_noise.shiftreg = (m_noise.shiftreg >> 1) | (feedback << 14);
}

uint8_t RP2A03Apu::CpuRead(uint16_t addr)
{
    // DMC reads its samples from $8000-$FFFF only
    return m_memory[addr & 0x7FFF];
}
