#pragma once

#include "ChipSim.h"

#define APU_NTSC 0
#define APU_PAL  1

#define CPU_CLOCK_NTSC 1789773L // Hz
#define CPU_CLOCK_PAL  1662607L // Hz

class RP2A03
{
    struct Envelope
    {
        uint8_t loop_halt      :  1;
        uint8_t const_vol      :  1;
        uint8_t vol_period     :  4;
        uint8_t counter        :  4;
        uint8_t divider        :  5;
        uint8_t start          :  1;
        uint8_t out            :  4;
    };

    struct Pulse
    {
        uint8_t  duty          :  2; // duty cycle of the pulse
        Envelope env;

        uint8_t  sweep_enable  :  1;
        uint8_t  sweep_period  :  3;
        uint8_t  sweep_negate  :  1;
        uint8_t  sweep_shift   :  3;
        uint8_t  sweep_divider :  4;
        uint16_t sweep_target  : 12;
        uint8_t  sweep_silence :  1;
        uint8_t  sweep_reload  :  1;

        uint16_t timer_period  : 11; // controls the frequency of the pulse
        uint8_t  counter       :  8; // length counter; when reaches zero, channel is silenced

        uint16_t timer         : 12; // set to timer_period * 8 (left shift) when timer period set, or this overflows
        uint8_t  phase         :  3; // pulse wave phase/sequence

        uint8_t  enabled       :  1;
    };

    struct Triangle
    {
        uint8_t  control       :  1;
        uint8_t  halt          :  1;
        uint8_t  cnt_reload    :  7; // linear counter reload value

        uint16_t timer_period  : 11; // period for the wave frequency timer
        uint8_t  counter       :  8; // length counter
        uint8_t  lincount      :  7; // linear counter
        uint16_t timer         : 11; // sequence timer
        uint8_t  phase         :  5; // triangle wave phase/sequence

        uint8_t  enabled       :  1;
    };

    struct Noise
    {
        Envelope env;

        uint8_t  mode          :  1;
        uint8_t  period        :  4;
        uint16_t period_actual : 12;
        uint16_t timer         : 12;
        uint8_t  counter       :  8;
        uint16_t shiftreg      : 15; // shift reg for noise

        uint8_t  enabled       :  1;
    };

    struct DMC
    {
        uint8_t  irq           :  1;
        uint8_t  loop          :  1;
        uint8_t  rate          :  4;
        uint16_t rate_actual   :  9;
        uint16_t timer         :  9;

        // memory reader
        uint16_t address       : 16;
        uint16_t addresscur    : 16;
        uint16_t length        : 12;
        uint16_t bytesleft     : 12;
        uint8_t  sample        :  8;
        uint8_t  buffered      :  1;

        // output unit
        uint8_t  counter       :  7;
        uint8_t  shiftreg      :  8;
        uint8_t  bitsleft      :  8;
        uint8_t  silence       :  8;

        uint8_t  control       :  1;
    };

    struct FrameCnt
    {
        uint8_t  mode          :  1;
        uint8_t  interrupt     :  1;
        uint8_t  int_inhibit   :  1;
        uint8_t  updated       :  1;
        uint16_t count         : 15;
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
    RP2A03(int samplerate, uint8_t clockstandard);

    void Reset();
    void Write(uint16_t addr, uint8_t data);
    uint8_t Read(uint16_t addr);
    int32_t Output();

protected:
    void Process(uint32_t cpu_cycles);

private:
    void CalculateSweepPulse1();
    void CalculateSweepPulse2();
    void UpdateQuarterFrame();
    void UpdateHalfFrame();
    void UpdateNoise();
    
    uint8_t CpuRead(uint16_t addr);

protected:
    Pulse    m_pulse1;
    Pulse    m_pulse2;
    Triangle m_triangle;
    Noise    m_noise;
    DMC      m_dmc;
    FrameCnt m_frame;
    uint32_t m_cpuCycles;

    uint32_t m_cpuClock;
    uint32_t m_cpuCyclesPerSample;
    const uint16_t* m_noisePeriods;
    const uint16_t* m_dmcPeriods;

    uint32_t m_P12MixLut[31];
    uint32_t m_TNDMixLut[203];
};

class SimRP2A03_ : public ChipSim, public RP2A03
{
public:
    SimRP2A03_(int samplerate, uint8_t clockstandard);

    void Reset() override;
    void Write(uint8_t chip, uint8_t reg, uint8_t data) override;
    void Simulate(int samples) override;
    void ConvertToPSG(Frame& frame) override;
    void PostProcess(Stream& stream) override;
};
