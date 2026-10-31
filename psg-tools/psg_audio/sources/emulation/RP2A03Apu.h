#pragma once

#include <stdint.h>

class RP2A03Apu
{
	struct Envelope
    {
        bool    loop_halt;
        bool    const_vol;
        uint8_t vol_period;
        uint8_t counter;
        uint8_t divider;
        bool    start;
        uint8_t out;
    };

    struct Sweep
    {
        bool     enable;
        uint8_t  period;
        bool     negate;
        uint8_t  shift;
        uint8_t  divider;
        uint16_t target;
        bool     silence;
        bool     reload;
    };

    struct Pulse
    {
        uint8_t  duty;         // duty cycle of the pulse
        Envelope envelope;
        Sweep    sweep;
        uint16_t timer_period; // controls the frequency of the pulse
        uint8_t  len_counter;  // length counter; when reaches zero, channel is silenced
        uint16_t timer;        // set to timer_period * 8 (left shift) when timer period set, or this overflows
        uint8_t  phase;        // pulse wave phase/sequence
        bool     enabled;
    };

    struct Triangle
    {
        bool     control;      // halts the length counter and keeps the reload flag
        bool     reload;       // linear counter reload flag, set by $400B
        uint8_t  lin_reload;   // linear counter reload value
        uint16_t timer_period; // period for the wave frequency timer
        uint8_t  len_counter;  // length counter
        uint8_t  lin_counter;  // linear counter
        uint16_t timer;        // sequence timer
        uint8_t  phase;        // triangle wave phase/sequence
        bool     enabled;
    };

    struct Noise
    {
        Envelope envelope;
        bool     mode;
        uint8_t  period_lut;
        uint16_t timer_period;
        uint16_t timer;
        uint8_t  len_counter;
        uint16_t shiftreg;     // shift reg for noise
        bool     enabled;
    };

    struct DMC
    {
        bool     irq_enable;
        bool     irq;
        bool     loop;
        uint8_t  rate;
        uint16_t rate_actual;
        uint16_t timer;

        // memory reader
        uint16_t address;
        uint16_t addresscur;
        uint16_t length;
        uint16_t bytesleft;
        uint8_t  sample;
        bool     buffered;

        // output unit
        uint8_t  counter;
        uint8_t  shiftreg;
        uint8_t  bitsleft;
        uint8_t  silence;

        bool     control;
    };

    struct FrameCnt
    {
        bool     mode;
        bool     interrupt;
        bool     int_inhibit;
        bool     updated;
        uint16_t count;
    };

    // band-limited output: every step of the mixer output is added to the
    // ring as a windowed sinc impulse, the samples integrate the impulses
    static constexpr int BLIP_SIZE = 64;
    struct Blip
    {
        double time;              // of the next CPU cycle, in output samples
        double level;             // last mixer output
        double sum;               // integrated output
        double buffer[BLIP_SIZE]; // ring of impulses to integrate
        int    index;             // of the next output sample in the ring
    };

public:
    enum
    {
        PULSE1DUTYVOL = 0x4000,
        PULSE1SWEEP   = 0x4001,
        PULSE1TMRL    = 0x4002,
        PULSE1TMRH    = 0x4003,
        PULSE2DUTYVOL = 0x4004,
        PULSE2SWEEP   = 0x4005,
        PULSE2TMRL    = 0x4006,
        PULSE2TMRH    = 0x4007,
        TRICOUNTER    = 0x4008,
        TRITMRL       = 0x400A,
        TRITMRH       = 0x400B,
        NOISEVOL      = 0x400C,
        NOISEPERIOD   = 0x400E,
        NOISELCL      = 0x400F,
        DMCIRQ        = 0x4010,
        DMCCOUNTER    = 0x4011,
        DMCADDR       = 0x4012,
        DMCLENGTH     = 0x4013,
        STATUS        = 0x4015,
        FRAMECNTR     = 0x4017
    };

public:
    RP2A03Apu();

    void Init(int sampleRate, int cpuClock);
    void Reset();

    void Write(uint16_t addr, uint8_t data);
    uint8_t Read(uint16_t addr);
    void WriteMemory(uint16_t addr, const uint8_t* data, uint32_t size);

    int32_t Output();

protected:
    void Process(uint32_t cpu_cycles);

private:
    void CalculateSweep(Pulse& pulse, bool onesComplement);
    void ClockSweep(Pulse& pulse, bool onesComplement);
    void UpdateEnvelopeOut(Envelope& envelope);
    void UpdateQuarterFrame();
    void UpdateHalfFrame();
    void UpdateNoise();

    uint8_t PulseOutput(const Pulse& pulse) const;
    double Mix() const;
    void AddDelta(double time, double delta);

    uint8_t CpuRead(uint16_t addr);

protected:
    Pulse    m_pulse1{};
    Pulse    m_pulse2{};
    Triangle m_triangle{};
    Noise    m_noise{};
    DMC      m_dmc{};
    FrameCnt m_frame{};
    uint32_t m_cpuCycles;
    bool     m_oddCycle;

    uint32_t m_cpuClock;
    uint32_t m_cpuCyclesPerSample;
    const uint16_t* m_noisePeriods;
    const uint16_t* m_dmcPeriods;
    const uint16_t* m_frameSteps;

    uint8_t  m_memory[0x8000]{}; // $8000-$FFFF, read by DMC

    double   m_samplesPerCycle;
    Blip     m_blip{};
};
