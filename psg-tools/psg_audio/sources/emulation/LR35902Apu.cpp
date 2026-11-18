#include "LR35902Apu.h"

namespace
{
    // the frame sequencer steps at 512 Hz: every 8192 CPU clocks
    const uint16_t c_stepUnits = 4096;

    // the noise LFSR shifts every divisor << shift CPU clocks
    const uint32_t c_noiseDivisors[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };

    // the duty steps 0-7 of 12.5, 25, 50 and 75%, as bits 0-7
    const uint8_t c_dutyPatterns[4] = { 0x80, 0x81, 0xE1, 0x7E };

    // MAME: a digital value 0-15 goes to the analog output as 15 - 2 * value
    int32_t DacOutput(int32_t value) { return 15 - 2 * value; }
}

LR35902Apu::LR35902Apu()
    : m_clock(4194304)
{
    Reset();
}

void LR35902Apu::Init(int clock)
{
    m_clock = clock;
}

void LR35902Apu::Reset()
{
    m_pulse[0] = m_pulse[1] = Pulse{};
    m_sweep = Sweep{};
    m_wave = Wave{};
    m_noise = Noise{};
    m_volumes = 0;
    m_routing = 0;
    m_cycles = 0;
    m_outLeft = m_outRight = 0;
    m_outCount = 0;

    // the timers count down to 0 from at least 1
    m_pulse[0].timer = m_pulse[1].timer = 2 * 2048;
    m_wave.timer = 2048;
    m_noise.timer = c_noiseDivisors[0] / 2;
    m_noise.lfsr = 0x7FFF;

    // The power is on: a log can start after the game switched it on.
    // When it switches on, the next step of the sequencer is 0.
    m_power = true;
    m_step = 7;
    m_stepTimer = c_stepUnits;
}

void LR35902Apu::Write(uint16_t addr, uint8_t data)
{
    // the wave RAM: on the DMG, a write while the channel plays is lost
    if (addr >= WAVE && addr < WAVE + 16)
    {
        if (!m_wave.on) m_wave.ram[addr - WAVE] = data;
        return;
    }

    if (addr == NR52)
    {
        bool power = (data & 0x80);
        if (m_power && !power) PowerOff();
        if (!m_power && power) m_step = 7;
        m_power = power;
        return;
    }

    // With the power off, the DMG takes only the length timers.
    if (!m_power)
    {
        switch (addr)
        {
        case NR11: m_pulse[0].length.counter = 64 - (data & 0x3F); break;
        case NR21: m_pulse[1].length.counter = 64 - (data & 0x3F); break;
        case NR31: m_wave.length.counter = 256 - data; break;
        case NR41: m_noise.length.counter = 64 - (data & 0x3F); break;
        }
        return;
    }

    switch (addr)
    {
    case NR10:
    {
        // MAME: a clear of the negate bit after a calculation in negate mode stops channel 1
        bool negate = (data & 0x08);
        if (m_sweep.negate && !negate && m_sweep.negated) m_pulse[0].on = false;
        m_sweep.pace = ((data >> 4) & 0x07);
        m_sweep.negate = negate;
        m_sweep.shift = (data & 0x07);
        break;
    }
    case NR11: case NR21:
    {
        Pulse& pulse = m_pulse[addr == NR21];
        pulse.duty = (data >> 6);
        pulse.length.counter = 64 - (data & 0x3F);
        break;
    }
    case NR12: case NR22:
    {
        Pulse& pulse = m_pulse[addr == NR22];
        WriteEnvelope(pulse.envelope, pulse.dac, pulse.on, data);
        break;
    }
    case NR13: case NR23:
    {
        Pulse& pulse = m_pulse[addr == NR23];
        pulse.period = ((pulse.period & 0x700) | data);
        break;
    }
    case NR14: case NR24:
    {
        Pulse& pulse = m_pulse[addr == NR24];
        bool trigger = (data & 0x80);
        pulse.period = ((pulse.period & 0x0FF) | ((data & 0x07) << 8));
        WriteLength(pulse.length, pulse.on, data, trigger, 64);
        if (trigger)
        {
            pulse.on = pulse.dac;
            pulse.timer = 2 * (2048 - pulse.period);
            TriggerEnvelope(pulse.envelope);

            if (addr == NR14)
            {
                m_sweep.shadow = pulse.period;
                m_sweep.timer = m_sweep.pace;
                m_sweep.negated = false;
                m_sweep.enable = (m_sweep.pace || m_sweep.shift);
                if (m_sweep.shift) CalculateSweep();
            }
        }
        break;
    }
    case NR30:
        m_wave.dac = (data & 0x80);
        if (!m_wave.dac) m_wave.on = false;
        break;

    case NR31:
        m_wave.length.counter = 256 - data;
        break;

    case NR32:
        m_wave.level = ((data >> 5) & 0x03);
        break;

    case NR33:
        m_wave.period = ((m_wave.period & 0x700) | data);
        break;

    case NR34:
    {
        bool trigger = (data & 0x80);
        m_wave.period = ((m_wave.period & 0x0FF) | ((data & 0x07) << 8));
        WriteLength(m_wave.length, m_wave.on, data, trigger, 256);
        if (trigger)
        {
            // Pan Docs: the first sample read is sample 1, about 6 clocks later
            m_wave.on = m_wave.dac;
            m_wave.position = 0;
            m_wave.timer = (2048 - m_wave.period) + 3;
        }
        break;
    }
    case NR41:
        m_noise.length.counter = 64 - (data & 0x3F);
        break;

    case NR42:
        WriteEnvelope(m_noise.envelope, m_noise.dac, m_noise.on, data);
        break;

    case NR43:
        m_noise.shift = (data >> 4);
        m_noise.narrow = (data & 0x08);
        m_noise.divider = (data & 0x07);
        break;

    case NR44:
    {
        bool trigger = (data & 0x80);
        WriteLength(m_noise.length, m_noise.on, data, trigger, 64);
        if (trigger)
        {
            m_noise.on = m_noise.dac;
            m_noise.lfsr = 0x7FFF;
            m_noise.timer = NoiseShiftCycles() / 2;
            TriggerEnvelope(m_noise.envelope);
        }
        break;
    }
    case NR50: m_volumes = data; break;
    case NR51: m_routing = data; break;
    }
}

void LR35902Apu::Output(int32_t& left, int32_t& right)
{
    left  = int32_t(m_outCount ? m_outLeft  / m_outCount : 0);
    right = int32_t(m_outCount ? m_outRight / m_outCount : 0);
    m_outLeft = m_outRight = 0;
    m_outCount = 0;
}

void LR35902Apu::Process(uint32_t cycles)
{
    // all the timers step on even CPU clocks
    m_cycles += cycles;
    uint32_t units = (m_cycles >> 1);
    m_cycles &= 1;

    while (units--)
    {
        if (--m_stepTimer == 0)
        {
            m_stepTimer = c_stepUnits;
            if (m_power) ClockSequencer();
        }

        for (Pulse& pulse : m_pulse)
        {
            if (--pulse.timer == 0)
            {
                pulse.timer = 2 * (2048 - pulse.period);
                pulse.step = ((pulse.step + 1) & 0x07);
            }
        }

        if (--m_wave.timer == 0)
        {
            m_wave.timer = (2048 - m_wave.period);
            m_wave.position = ((m_wave.position + 1) & 0x1F);
            uint8_t byte = m_wave.ram[m_wave.position >> 1];
            m_wave.sample = ((m_wave.position & 1) ? byte & 0x0F : byte >> 4);
        }

        // Pan Docs: with the shift 14 or 15 the LFSR gets no clocks
        if (m_noise.shift < 14 && --m_noise.timer == 0)
        {
            m_noise.timer = NoiseShiftCycles() / 2;
            uint16_t feedback = ((m_noise.lfsr >> 1) ^ m_noise.lfsr) & 1;
            m_noise.lfsr = ((m_noise.lfsr >> 1) | (feedback << 14));
            if (m_noise.narrow) m_noise.lfsr = ((m_noise.lfsr & ~0x40) | (feedback << 6));
        }

        // NR51: the bits 0-3 route the channels 1-4 to the right, the bits 4-7 to the left
        const int32_t outputs[4] = { PulseOutput(m_pulse[0]), PulseOutput(m_pulse[1]), WaveOutput(), NoiseOutput() };
        int32_t left = 0, right = 0;
        for (int chan = 0; chan < 4; ++chan)
        {
            if (m_routing & (0x10 << chan)) left  += outputs[chan];
            if (m_routing & (0x01 << chan)) right += outputs[chan];
        }
        m_outLeft  += left  * (1 + ((m_volumes >> 4) & 0x07));
        m_outRight += right * (1 + (m_volumes & 0x07));
        m_outCount++;
    }
}

uint32_t LR35902Apu::NoiseShiftCycles() const
{
    return (c_noiseDivisors[m_noise.divider] << m_noise.shift);
}

void LR35902Apu::WriteEnvelope(Envelope& envelope, bool& dac, bool& on, uint8_t data)
{
    // Pan Docs "zombie mode": a write while the channel plays changes the volume
    if (on)
    {
        uint8_t volume = envelope.volume;
        if (envelope.period == 0 && envelope.active) volume += 1;
        else if (!envelope.up) volume += 2;
        if (envelope.up != bool(data & 0x08)) volume = 16 - volume;
        envelope.volume = (volume & 0x0F);
    }

    envelope.initial = (data >> 4);
    envelope.up = (data & 0x08);
    envelope.period = (data & 0x07);

    // the DAC is on while any of the bits 3-7 is set
    dac = (data & 0xF8);
    if (!dac) on = false;
}

void LR35902Apu::WriteLength(Length& length, bool& on, uint8_t data, bool trigger, uint16_t max)
{
    // The length gets an extra clock when it is enabled or triggered
    // while the next step of the sequencer does not clock it.
    bool extra = !(m_step & 1);
    bool enabled = length.enable;
    length.enable = (data & 0x40);
    if (!enabled && length.enable && extra && length.counter) ClockLength(length, on);

    if (trigger && !length.counter)
    {
        length.counter = max;
        if (length.enable && extra) length.counter--;
    }
}

void LR35902Apu::TriggerEnvelope(Envelope& envelope)
{
    envelope.volume = envelope.initial;
    envelope.timer = envelope.period;
    envelope.active = true;
}

void LR35902Apu::ClockSequencer()
{
    // the steps 0, 2, 4, 6 clock the lengths (256 Hz),
    // 2 and 6 the sweep (128 Hz), 7 the envelopes (64 Hz)
    m_step = ((m_step + 1) & 0x07);
    if (!(m_step & 1))
    {
        if (m_step == 2 || m_step == 6) ClockSweep();
        ClockLength(m_pulse[0].length, m_pulse[0].on);
        ClockLength(m_pulse[1].length, m_pulse[1].on);
        ClockLength(m_wave.length, m_wave.on);
        ClockLength(m_noise.length, m_noise.on);
    }
    else if (m_step == 7)
    {
        ClockEnvelope(m_pulse[0].envelope);
        ClockEnvelope(m_pulse[1].envelope);
        ClockEnvelope(m_noise.envelope);
    }
}

void LR35902Apu::ClockLength(Length& length, bool& on)
{
    if (length.enable && length.counter)
    {
        if (--length.counter == 0) on = false;
    }
}

void LR35902Apu::ClockEnvelope(Envelope& envelope)
{
    // the timer counts 8 ticks for the period 0, the volume does not change then
    envelope.timer = ((envelope.timer - 1) & 0x07);
    if (envelope.timer == 0)
    {
        envelope.timer = envelope.period;
        if (envelope.period && envelope.active)
        {
            int volume = envelope.volume + (envelope.up ? 1 : -1);
            if (volume < 0 || volume > 15) envelope.active = false;
            else envelope.volume = uint8_t(volume);
        }
    }
}

void LR35902Apu::ClockSweep()
{
    // the timer counts 8 ticks for the pace 0, the period does not change then
    m_sweep.timer = ((m_sweep.timer - 1) & 0x07);
    if (m_sweep.timer == 0)
    {
        m_sweep.timer = m_sweep.pace;
        if (m_sweep.enable && m_sweep.pace)
        {
            uint16_t period = CalculateSweep();
            if (period <= 0x7FF && m_sweep.shift)
            {
                m_sweep.shadow = period;
                m_pulse[0].period = period;
                CalculateSweep();
            }
        }
    }
}

uint16_t LR35902Apu::CalculateSweep()
{
    // a result over 11 bits stops channel 1, also when it is not applied
    uint16_t delta = (m_sweep.shadow >> m_sweep.shift);
    if (m_sweep.negate) m_sweep.negated = true;
    uint16_t period = (m_sweep.negate ? m_sweep.shadow - delta : m_sweep.shadow + delta);
    if (period > 0x7FF) m_pulse[0].on = false;
    return period;
}

void LR35902Apu::PowerOff()
{
    // All the registers but NR52 are cleared. The DMG keeps the length
    // timers and the wave RAM. The duty steps start from 0.
    for (uint16_t addr = NR10; addr <= NR51; ++addr)
    {
        if (addr != NR11 && addr != NR21 && addr != NR31 && addr != NR41) Write(addr, 0);
    }
    m_pulse[0].duty = m_pulse[1].duty = 0;
    m_pulse[0].step = m_pulse[1].step = 0;
    m_pulse[0].on = m_pulse[1].on = false;
    m_wave.on = m_noise.on = false;
    m_wave.sample = 0;
    m_sweep.shadow = 0;
    m_sweep.negated = false;
}

int32_t LR35902Apu::PulseOutput(const Pulse& pulse) const
{
    if (!pulse.on) return 0;
    bool high = ((c_dutyPatterns[pulse.duty] >> pulse.step) & 1);
    return DacOutput(high ? pulse.envelope.volume : 0);
}

int32_t LR35902Apu::WaveOutput() const
{
    if (!m_wave.on) return 0;
    return DacOutput(m_wave.level ? m_wave.sample >> (m_wave.level - 1) : 0);
}

int32_t LR35902Apu::NoiseOutput() const
{
    if (!m_noise.on) return 0;
    bool high = !(m_noise.lfsr & 1);
    return DacOutput(high ? m_noise.envelope.volume : 0);
}