#pragma once

#include <stdint.h>

// The APU of the Game Boy (DMG). It follows the model of MAME gb.cpp
// (BSD-3-Clause, Wilbert Pol, Anthony Kruize) with the corrections of
// Pan Docs. The CPU clock (4194304 Hz) runs it.
class LR35902Apu
{
protected:
    struct Envelope
    {
        uint8_t initial; // volume on trigger
        bool    up;
        uint8_t period;  // in ticks of 64 Hz, 0 stops the envelope
        uint8_t timer;
        uint8_t volume;  // current, 0-15
        bool    active;  // stops at volume 0 or 15
    };

    struct Length
    {
        uint16_t counter; // ticks of 256 Hz left
        bool     enable;
    };

    struct Pulse
    {
        uint8_t  duty;   // 12.5, 25, 50, 75%
        uint8_t  step;   // of the 8 steps of the duty
        uint16_t period; // 11 bits, the frequency is 131072 / (2048 - period) Hz
        uint16_t timer;  // in units of 2 clocks
        Envelope envelope;
        Length   length;
        bool     dac;
        bool     on;
    };

    struct Sweep
    {
        uint8_t  pace;    // in ticks of 128 Hz
        bool     negate;
        uint8_t  shift;
        uint8_t  timer;
        uint16_t shadow;  // period
        bool     enable;
        bool     negated; // a calculation in negate mode since the trigger
    };

    struct Wave
    {
        uint8_t  level;    // 0 mute, 1 100%, 2 50%, 3 25%
        uint16_t period;   // 11 bits, the frequency is 65536 / (2048 - period) Hz
        uint16_t timer;    // in units of 2 clocks
        uint8_t  position; // of the 32 samples
        uint8_t  sample;   // the last sample read
        uint8_t  ram[16];  // 32 samples of 4 bits, the high nibble first
        Length   length;
        bool     dac;
        bool     on;
    };

    struct Noise
    {
        uint8_t  divider; // code 0-7
        uint8_t  shift;   // 14 and 15 stop the LFSR
        bool     narrow;  // 7-bit LFSR
        uint16_t lfsr;    // 15 bits, the output is the inverted bit 0
        uint32_t timer;   // in units of 2 clocks
        Envelope envelope;
        Length   length;
        bool     dac;
        bool     on;
    };

public:
    enum
    {
        NR10 = 0xFF10, NR11 = 0xFF11, NR12 = 0xFF12, NR13 = 0xFF13, NR14 = 0xFF14,
        NR21 = 0xFF16, NR22 = 0xFF17, NR23 = 0xFF18, NR24 = 0xFF19,
        NR30 = 0xFF1A, NR31 = 0xFF1B, NR32 = 0xFF1C, NR33 = 0xFF1D, NR34 = 0xFF1E,
        NR41 = 0xFF20, NR42 = 0xFF21, NR43 = 0xFF22, NR44 = 0xFF23,
        NR50 = 0xFF24, NR51 = 0xFF25, NR52 = 0xFF26,
        WAVE = 0xFF30
    };

    LR35902Apu();

    void Init(int clock);
    void Reset();
    void Write(uint16_t addr, uint8_t data);

    // Mean output since the last call, of the channels routed by NR51 and
    // scaled by NR50. Full scale is 480 (15 x 4 channels x 8).
    void Output(int32_t& left, int32_t& right);

protected:
    void Process(uint32_t cycles);

    // the noise LFSR shifts every this number of CPU clocks
    uint32_t NoiseShiftCycles() const;

private:
    void WriteEnvelope(Envelope& envelope, bool& dac, bool& on, uint8_t data);
    void WriteLength(Length& length, bool& on, uint8_t data, bool trigger, uint16_t max);
    void TriggerEnvelope(Envelope& envelope);

    void ClockSequencer();
    void ClockLength(Length& length, bool& on);
    void ClockEnvelope(Envelope& envelope);
    void ClockSweep();
    uint16_t CalculateSweep();
    void PowerOff();

    int32_t PulseOutput(const Pulse& pulse) const;
    int32_t WaveOutput() const;
    int32_t NoiseOutput() const;

protected:
    Pulse    m_pulse[2]{};
    Sweep    m_sweep{};
    Wave     m_wave{};
    Noise    m_noise{};
    uint8_t  m_volumes; // NR50
    uint8_t  m_routing; // NR51
    bool     m_power;
    uint8_t  m_step;    // of the frame sequencer
    uint16_t m_stepTimer;
    uint32_t m_clock;
    uint32_t m_cycles;  // the odd CPU clock left from the last call

    int64_t  m_outLeft;
    int64_t  m_outRight;
    uint32_t m_outCount;
};
