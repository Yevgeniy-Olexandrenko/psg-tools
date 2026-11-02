#include "LevelMeter.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include "stream/Chip.h"
#include "stream/Frame.h"
#include "emulation/AY89XXPsg.h"

// Fixed-point formats: DAC output Q15, power Q30, gates Q10, shares and band
// weights Q16, log2 Q8, envelope phase Q16 steps.

namespace
{
    // the meter shows this range below the loudest output
    const int k_rangeDb = 30;

    // The weighting of the power by frequency, with the gain 1 at 1 kHz. K is
    // the loudness curve of ITU-R BS.1770 (LUFS), it mainly cuts the deep bass.
    // A is the curve of sound level meters, it cuts more bass. None keeps the
    // power. All of them use only the bands in the audible range.
    enum class Weighting { None, A, K };
    const Weighting k_weighting = Weighting::K;

    const double k_pi = 3.14159265358979323846;

    // The fundamental of a periodic sound can be this number of bands below
    // the first band. A sound that repeats more slowly (about 1.3 Hz) is not
    // heard in a frame, so the meter ignores it.
    const int k_bandsBelow = 48;
    const int k_harmonicBands = (LevelMeter::k_bands + k_bandsBelow);

    // the noise shares start at this band, relative to the band of the noise rate
    const int k_noiseFirst = -192;
    const int k_noiseBands = 289;

    // the high part of the tone period in the AY8930 expanded mode, in 32nds
    const uint32_t k_dutySteps[9] = { 1, 2, 4, 8, 16, 24, 28, 30, 31 };

    // the bands show this range below a tone at the maximum volume
    const int k_bandsRangeDb = 36;

    // the level for one log2 unit of power, Q16
    const int32_t k_levelScale = int32_t(255 * 3.01029995664 / k_rangeDb / 256 * 65536 + 0.5);
    const int32_t k_bandScale = int32_t(255 * 3.01029995664 / k_bandsRangeDb / 256 * 65536 + 0.5);

    // log2 of the power of a tone at the maximum volume (a square wave, power 1/4)
    const int k_log2MaxBand = (28 << 8);

    // The envelope shapes as EnvelopeUnit runs them: a ramp of 32 steps, then
    // the level holds at 0 or 31, or the ramp repeats, or it goes up and down.
    enum class After { Hold0, Hold31, Repeat, Alternate };
    struct Shape { bool down; After after; };

    const Shape k_shapes[16] =
    {
        { true,  After::Hold0  }, { true,  After::Hold0  }, { true,  After::Hold0     }, { true,  After::Hold0  },
        { false, After::Hold0  }, { false, After::Hold0  }, { false, After::Hold0     }, { false, After::Hold0  },
        { true,  After::Repeat }, { true,  After::Hold0  }, { true,  After::Alternate }, { true,  After::Hold31 },
        { false, After::Repeat }, { false, After::Hold31 }, { false, After::Alternate }, { false, After::Hold0  },
    };

    ////////////////////////////////////////////////////////////////////////////
    // The tables

    // power gain of the A curve (IEC 61672), not normalized
    double CurveA(double f)
    {
        double f2 = (f * f);
        double r = (148693636.0 * f2 * f2) / ((f2 + 424.36) * sqrt((f2 + 11599.29) * (f2 + 544496.41)) * (f2 + 148693636.0));
        return (r * r);
    }

    // power gain of a biquad filter at 48 kHz
    double Biquad(const double (&b)[3], const double (&a)[3], double f)
    {
        std::complex<double> z1 = std::polar(1.0, -2 * k_pi * f / 48000);
        std::complex<double> z2 = (z1 * z1);
        return std::norm(b[0] + b[1] * z1 + b[2] * z2) / std::norm(a[0] + a[1] * z1 + a[2] * z2);
    }

    // power gain of the K curve (the shelf and the high pass of ITU-R BS.1770 at 48 kHz), not normalized
    double CurveK(double f)
    {
        const double shelfB[3] { 1.53512485958697, -2.69169618940638, 1.19839281085285 };
        const double shelfA[3] { 1.0, -1.69065929318241, 0.73248077421585 };
        const double passB[3] { 1.0, -2.0, 1.0 };
        const double passA[3] { 1.0, -1.99004745483398, 0.99007225036621 };
        return Biquad(shelfB, shelfA, f) * Biquad(passB, passA, f);
    }

    double Gain(double f)
    {
        switch (k_weighting)
        {
        case Weighting::A: return CurveA(f) / CurveA(1000);
        case Weighting::K: return CurveK(f) / CurveK(1000);
        default: return 1;
        }
    }

    uint16_t ToShare(double share)
    {
        return uint16_t(std::min(65535.0, std::floor(share * 65536 + 0.5)));
    }

    // Shares of a periodic sound by band from the fundamental, when the
    // harmonic n has a[n % period] / n^2 of the power. A band holds the
    // harmonics from 2^((band - 0.5) / 12) to the next band. The first
    // harmonics are added one by one, the others by their average.
    void BuildHarmonics(const double* a, int period, uint16_t* shares)
    {
        const int exact = 1024;
        const double step = std::pow(2.0, 1 / 12.0);

        double average = 0;
        for (int r = 0; r < period; ++r) average += a[r];
        average /= period;

        int n = 1;
        double lo = std::pow(2.0, -0.5 / 12);
        for (int band = 0; band < k_harmonicBands; ++band, lo *= step)
        {
            double hi = (lo * step), sum = 0;
            for (; n <= exact && n < hi; ++n)
            {
                sum += (a[n % period] / (double(n) * n));
            }

            double from = std::max(lo, exact + 0.5);
            if (hi > from) sum += (average * (1 / from - 1 / hi));
            shares[band] = ToShare(sum);
        }
    }

    // Shares of a waveform of 'count' equal steps. The harmonic n has the n-th
    // DFT term of the steps times sinc(pi * n / count)^2. Both repeat with the
    // period 'count', except the 1 / n^2 of the sinc.
    void BuildSteps(const double* steps, int count, uint16_t* shares)
    {
        double mean = 0, power = 0;
        std::complex<double> turns[64];
        for (int k = 0; k < count; ++k)
        {
            mean += steps[k];
            power += (steps[k] * steps[k]);
            turns[k] = std::polar(1.0, -2 * k_pi * k / count);
        }
        mean /= count;
        power = (power / count - mean * mean);

        double a[64] {};
        for (int r = 1; r < count && power > 0; ++r)
        {
            std::complex<double> c = 0;
            for (int k = 0; k < count; ++k)
            {
                c += (steps[k] * turns[r * k % count]);
            }
            double s = (std::sin(k_pi * r / count) * count / k_pi);
            a[r] = (2 * std::norm(c / double(count)) * s * s / power);
        }
        BuildHarmonics(a, count, shares);
    }

    // sinc(pi * u)^2
    double Sinc2(double u)
    {
        double s = (std::sin(k_pi * u) / (k_pi * u));
        return (s * s);
    }

    // The noise holds a random bit for each period, so its power spectrum is
    // sinc(pi * f / rate)^2. A band gets its part of the integral. Far above
    // the rate, sin^2 is replaced by its average.
    void BuildNoise(uint16_t* shares)
    {
        const double step = std::pow(2.0, 1 / 12.0);
        double lo = std::pow(2.0, (k_noiseFirst - 0.5) / 12);
        for (int i = 0; i < k_noiseBands; ++i, lo *= step)
        {
            double hi = (lo * step);
            double share = 0;
            if (lo > 4)
            {
                share = ((1 / lo - 1 / hi) / (k_pi * k_pi));
            }
            else
            {
                // Simpson's rule
                const int steps = 8;
                double h = ((hi - lo) / steps);
                for (int k = 0; k <= steps; ++k)
                {
                    double w = (k == 0 || k == steps ? 1 : (k & 1 ? 4 : 2));
                    share += (w * Sinc2(lo + k * h));
                }
                share *= (2 * h / 3);
            }
            shares[i] = ToShare(share);
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // The measurement

    // log2 of x > 0, Q8
    int Log2(const uint8_t* table, uint32_t x)
    {
        int msb = 31;
        if (!(x & 0xFFFF0000)) { x <<= 16; msb -= 16; }
        if (!(x & 0xFF000000)) { x <<= 8; msb -= 8; }
        if (!(x & 0xF0000000)) { x <<= 4; msb -= 4; }
        if (!(x & 0xC0000000)) { x <<= 2; msb -= 2; }
        if (!(x & 0x80000000)) { x <<= 1; msb -= 1; }
        return ((msb << 8) + table[(x >> 23) & 0xFF]);
    }

    int Log2(const uint8_t* table, uint64_t x)
    {
        int shift = 0;
        while (x >> 32) { x >>= 8; shift += 8; }
        return (Log2(table, uint32_t(x)) + (shift << 8));
    }

    // sum of the first n steps of a ramp
    uint64_t SumRamp(const uint64_t* sum, bool down, uint32_t n)
    {
        return (down ? sum[32] - sum[32 - n] : sum[n]);
    }

    // sum of the first n steps after the envelope restart
    uint64_t SumSteps(const uint64_t* sum, const Shape& shape, uint32_t n)
    {
        if (n <= 32) return SumRamp(sum, shape.down, n);

        uint32_t q = (n - 32) / 32;
        uint32_t r = (n - 32) % 32;
        switch (shape.after)
        {
        case After::Hold0:  return sum[32] + (n - 32) * sum[1];
        case After::Hold31: return sum[32] + (n - 32) * (sum[32] - sum[31]);
        case After::Repeat: return sum[32] + q * sum[32] + SumRamp(sum, shape.down, r);
        default: return sum[32] + q * sum[32] + SumRamp(sum, (q & 1) ? shape.down : !shape.down, r);
        }
    }

    // integral from the envelope restart to x (Q16 steps), Q16
    uint64_t Integral(const uint64_t* sum, const Shape& shape, uint32_t x)
    {
        uint32_t n = (x >> 16);
        uint64_t s = SumSteps(sum, shape, n);
        return ((s << 16) + (SumSteps(sum, shape, n + 1) - s) * (x & 0xFFFF));
    }

    // after the first ramp the envelope repeats, so the phase stays small
    uint32_t WrapPhase(const Shape& shape, uint64_t x)
    {
        const uint64_t ramp = (32 << 16);
        if (x < ramp) return uint32_t(x);
        switch (shape.after)
        {
        case After::Repeat:    return uint32_t(ramp + (x - ramp) % ramp);
        case After::Alternate: return uint32_t(ramp + (x - ramp) % (2 * ramp));
        default: return uint32_t(ramp);
        }
    }
}

struct LevelMeter::Steps
{
    // prefix sums of the DAC output and of its square over the 32 envelope steps
    uint64_t sum1[33];
    uint64_t sum2[33];

    // shares of the periodic envelopes by band from the fundamental
    uint16_t saw[k_harmonicBands];
    uint16_t triangle[k_harmonicBands];
};

struct LevelMeter::Tables
{
    Tables();

    uint8_t log2[256];        // log2(1 + m / 256), Q8
    uint32_t weights[k_bands];
    uint16_t dac[2][32];      // [AY, YM]

    // [AY, YM, YM on the odd steps for the AY8930 compatibility mode]
    Steps steps[3];

    // shares of the tone by band from the fundamental, [duty cycle]
    uint16_t squares[9][k_harmonicBands];

    // shares of the noise by band from the band of its rate
    uint16_t noise[k_noiseBands];
};

LevelMeter::Tables::Tables()
{
    for (int m = 0; m < 256; ++m)
    {
        log2[m] = uint8_t(std::min(255.0, std::floor(256 * std::log2(1 + m / 256.0) + 0.5)));
    }

    for (int band = 0; band < k_bands; ++band)
    {
        weights[band] = uint32_t(Gain(k_firstBandFreq * std::pow(2.0, band / 12.0)) * 65536 + 0.5);
    }

    for (int type = 0; type < 2; ++type)
    {
        const double* table = AY89XXPsg::GetDACTable(type == 1);
        for (int i = 0; i < 32; ++i) dac[type][i] = uint16_t(table[i] * 32768 + 0.5);
    }

    for (int type = 0; type < 3; ++type)
    {
        Steps& s = steps[type];
        double saw[32], triangle[64];

        s.sum1[0] = s.sum2[0] = 0;
        for (int step = 0; step < 32; ++step)
        {
            uint64_t out = dac[type ? 1 : 0][type == 2 ? (step | 1) : step];
            s.sum1[step + 1] = s.sum1[step] + out;
            s.sum2[step + 1] = s.sum2[step] + out * out;
            saw[step] = triangle[step] = triangle[63 - step] = (out / 32768.0);
        }

        BuildSteps(saw, 32, s.saw);
        BuildSteps(triangle, 64, s.triangle);
    }

    for (int index = 0; index < 9; ++index)
    {
        // the harmonic n of a pulse wave has sin(pi * n * duty)^2 / n^2,
        // all of them add up to pi^2 * duty * (1 - duty) / 2
        double duty = (k_dutySteps[index] / 32.0);
        double power = (k_pi * k_pi * duty * (1 - duty) / 2);
        double a[32];
        for (int r = 0; r < 32; ++r)
        {
            double s = std::sin(k_pi * r * duty);
            a[r] = (s * s / power);
        }
        BuildHarmonics(a, 32, squares[index]);
    }

    BuildNoise(noise);
}

const LevelMeter::Tables& LevelMeter::GetTables()
{
    static const Tables s_tables;
    return s_tables;
}

////////////////////////////////////////////////////////////////////////////////

LevelMeter::LevelMeter()
    : m_tables(nullptr)
    , m_chips(0)
    , m_stepsPerFrame(0)
    , m_clockBand(0)
    , m_hasExpMode{}
    , m_isGateOR{}
    , m_dac{}
    , m_steps{}
    , m_phase{}
    , m_panL{}
    , m_panR{}
    , m_log2MaxPower(0)
    , m_spectrum{}
{
}

void LevelMeter::Init(const Chip& chip, int frameRate)
{
    m_tables = &GetTables();
    m_chips = chip.count;

    // the envelope makes a step every 'period' ticks of clock / 8 (see EnvelopeUnit)
    int clock = std::max(int(chip.clockValue), 1);
    m_stepsPerFrame = ((uint64_t(clock) << 16) / (8 * uint64_t(std::max(frameRate, 1))));
    m_clockBand = int(std::floor(256 * 12 * std::log2(clock / k_firstBandFreq) + 0.5));

    for (int i = 0; i < m_chips; ++i)
    {
        // the emulator uses the YM table for the AY8930 too; in the compatibility
        // mode the AY8930 envelope uses the odd levels only
        Chip::Model model = chip.model(i);
        bool isYM = (model == Chip::Model::YM2149 || model == Chip::Model::AY8930);
        m_hasExpMode[i] = chip.hasExpMode(i);
        m_isGateOR[i] = (model == Chip::Model::AY8930);
        m_dac[i] = m_tables->dac[isYM];

        for (int mode = 0; mode < 2; ++mode)
        {
            m_steps[i][mode] = &m_tables->steps[isYM ? (m_hasExpMode[i] && !mode ? 2 : 1) : 0];
        }
        for (uint32_t& phase : m_phase[i]) phase = 0;
    }

    // the pans are the same as in PCMOutput::PCMInit
    bool isStereo = (chip.output == Chip::Output::Stereo);
    const double pans[3] = { isStereo ? 0.1 : 0.5, 0.5, isStereo ? 0.9 : 0.5 };

    uint64_t sumL = 0, sumR = 0;
    for (int chan = 0; chan < 3; ++chan)
    {
        m_panL[chan] = uint32_t((1 - pans[chan]) * (1 - pans[chan]) * 65536 + 0.5);
        m_panR[chan] = uint32_t(pans[chan] * pans[chan] * 65536 + 0.5);
        sumL += m_panL[chan];
        sumR += m_panR[chan];
    }

    // the loudest output: all channels of all chips play a tone at the maximum volume
    uint64_t maxPower = ((uint64_t(1) << 28) * m_chips * std::max(sumL, sumR));
    m_log2MaxPower = Log2(m_tables->log2, maxPower);
}

void LevelMeter::Measure(const Frame& frame, uint8_t& L, uint8_t& R)
{
    std::fill(&m_spectrum[0][0], &m_spectrum[0][0] + k_sounds * k_bands, 0u);

    uint64_t powerL = 0, powerR = 0;
    for (int chip = 0; chip < m_chips; ++chip)
    {
        MeasureChip(chip, frame, powerL, powerR);
    }
    L = ToLevel(powerL, m_log2MaxPower, k_levelScale);
    R = ToLevel(powerR, m_log2MaxPower, k_levelScale);
}

void LevelMeter::GetBands(uint8_t* levels, Sound* sounds) const
{
    for (int band = 0; band < k_bands; ++band)
    {
        uint64_t power = 0;
        uint32_t loudest = 0;
        sounds[band] = Tone;
        for (int sound = 0; sound < k_sounds; ++sound)
        {
            uint32_t part = m_spectrum[sound][band];
            if (part > loudest)
            {
                loudest = part;
                sounds[band] = Sound(sound);
            }
            power += part;
        }
        levels[band] = ToLevel(power, k_log2MaxBand, k_bandScale);
    }
}

void LevelMeter::MeasureChip(int chip, const Frame& frame, uint64_t& powerL, uint64_t& powerR)
{
    const Frame::Registers& regs = frame[chip];
    bool isExpMode = (m_hasExpMode[chip] && regs.IsExpMode());

    // in the expanded mode a write to a bank B register switches the banks
    // through R13, and this restarts the envelope of channel A (see Output::Write)
    bool switchBanks = false;
    if (isExpMode)
    {
        for (Register reg = Register::BankB_Fst; reg < Register::BankB_Lst; ++reg)
        {
            switchBanks |= regs.IsChanged(reg);
        }
    }

    // the expanded mode has an envelope generator for each channel,
    // the compatibility mode has one for all channels
    Level envelopes[3] {};
    for (int env = 0; env < (isExpMode ? 3 : 1); ++env)
    {
        envelopes[env] = MeasureEnvelope(chip, env, isExpMode, frame, env == 0 && switchBanks);
    }

    bool isOR = m_isGateOR[chip];
    for (int chan = 0; chan < 3; ++chan)
    {
        Level level;
        if (regs.IsEnvelopeEnabled(chan))
        {
            level = envelopes[isExpMode ? chan : 0];
        }
        else
        {
            uint8_t volume = regs.GetVolume(chan);
            uint32_t out = m_dac[chip][isExpMode ? volume : (volume << 1 | 1)];
            level = { out, out * out, nullptr, -1 };
        }

        // The tone and the noise are gates that let the level out. On the
        // AY8930 one open gate is sufficient, on the other chips both gates
        // must be open (see AY89XXPsg::Process). A gate has a mean and a
        // variance, Q10. A tone above the bands has only its mean: this plays
        // samples through the volume register.
        uint32_t toneMean = (isOR ? 0 : 1024), toneVar = 0;
        const uint16_t* toneShares = nullptr;
        int toneBand = 0;
        if (regs.IsToneEnabled(chan))
        {
            // in the compatibility mode the tone toggles every 'period' ticks of
            // clock / 8, in the expanded mode it makes its 32 duty steps in
            // 'period' ticks (see ToneUnit)
            uint8_t duty = (regs.GetData(Register::t_duty[chan]) & 0x0F);
            int index = (isExpMode ? ((duty & 0x08) ? 0x08 : duty) : 4);
            uint32_t share = k_dutySteps[index];
            uint32_t period = std::max<uint32_t>(regs.Read(Register::t_period[chan]), 1);

            toneMean = (share << 5);
            toneShares = m_tables->squares[index];
            toneBand = ToBand((isExpMode ? 8 : 16) * period);
            if (toneBand < k_bands) toneVar = (share * (32 - share));
        }

        // in the compatibility mode the noise gets a new random bit every
        // 2 * period ticks of clock / 8; in the expanded mode it is about two
        // times faster, and its masks are ignored (see NoiseUnit)
        uint32_t noiseMean = (isOR ? 0 : 1024), noiseVar = 0;
        int noiseBand = 0;
        if (regs.IsNoiseEnabled(chan))
        {
            uint32_t period = std::max<uint32_t>(regs.Read(Register::Period::N), 1);
            noiseMean = 512;
            noiseVar = 256;
            noiseBand = ToBand((isExpMode ? 8 : 16) * period);
        }

        // The gate is t * n (AND) or 1 - (1 - t) * (1 - n) (OR) of the
        // independent tone and noise. One part of its variance has the
        // spectrum of the tone, the other part has the spectrum of the noise.
        // The gate modulates the level, and the level can change too (a buzzer).
        uint32_t t = (isOR ? 1024 - toneMean : toneMean);
        uint32_t n = (isOR ? 1024 - noiseMean : noiseMean);
        uint32_t tn = ((t * n) >> 10);
        uint32_t mean = (isOR ? 1024 - tn : tn);
        uint32_t varT = ((n * n * toneVar) >> 10);                      // Q20
        uint32_t varN = (((t * t + (toneVar << 10)) * noiseVar) >> 10); // Q20
        uint32_t varL = (level.m2 - std::min(level.m2, level.m1 * level.m1));

        uint32_t powerT = uint32_t((uint64_t(varT) * level.m2) >> 20);
        uint32_t powerN = uint32_t((uint64_t(varN) * level.m2) >> 20);
        uint32_t powerE = uint32_t((uint64_t(mean * mean) * varL) >> 20);

        // a single ramp has no jumps, so only its fundamental counts
        uint64_t weighted = 0;
        weighted += AddShares(Tone, powerT, toneShares, 0, k_harmonicBands, toneBand);
        weighted += AddShares(Noise, powerN, m_tables->noise, k_noiseFirst, k_noiseBands, noiseBand);
        weighted += (level.shares
            ? AddShares(Envelope, powerE, level.shares, 0, k_harmonicBands, level.band)
            : AddLine(Envelope, powerE, level.band));

        uint64_t power = (weighted >> 16);
        powerL += (power * m_panL[chan]);
        powerR += (power * m_panR[chan]);
    }
}

LevelMeter::Level LevelMeter::MeasureEnvelope(int chip, int env, bool isExpMode, const Frame& frame, bool restart)
{
    const Frame::Registers& regs = frame[chip];
    Register shapeReg = (isExpMode ? Register::e_shape[env] : Register::E_Shape);
    uint32_t period = std::max<uint32_t>(regs.Read(isExpMode ? Register::e_period[env] : Register::Period::E), 1);

    // a write to the shape register restarts the envelope, also with the same shape
    uint32_t& phase = m_phase[chip][env];
    if (restart || regs.IsChanged(shapeReg)) phase = 0;

    // To keep the sums in 64 bits, measure not more than 16384 steps of a
    // frame: after them the envelope only repeats or holds.
    const Shape& shape = k_shapes[regs.GetData(shapeReg) & 0x0F];
    const Steps& steps = *m_steps[chip][isExpMode];
    uint64_t count = std::max<uint64_t>(m_stepsPerFrame / period, 1);
    uint32_t span = uint32_t(std::min<uint64_t>(count, uint64_t(16384) << 16));

    Level level;
    level.m1 = uint32_t((Integral(steps.sum1, shape, phase + span) - Integral(steps.sum1, shape, phase)) / span);
    level.m2 = uint32_t((Integral(steps.sum2, shape, phase + span) - Integral(steps.sum2, shape, phase)) / span);
    phase = WrapPhase(shape, phase + count);

    // the envelope changes sound as a sawtooth of 32 steps or a triangle of
    // 64 steps; a single ramp counts by its fundamental only
    switch (shape.after)
    {
    case After::Repeat:    level.shares = steps.saw; level.band = ToBand(256 * period); break;
    case After::Alternate: level.shares = steps.triangle; level.band = ToBand(512 * period); break;
    default: level.shares = nullptr; level.band = ToBand(256 * period); break;
    }
    return level;
}

// the nearest band of clock / divisor
int LevelMeter::ToBand(uint32_t divisor) const
{
    int semitones = (m_clockBand - 12 * Log2(m_tables->log2, divisor) + 128);
    return (semitones >= 0 ? semitones >> 8 : -((255 - semitones) >> 8));
}

// Adds the power, spread by its shares over the bands from 'band + first',
// to the spectrum of the sound. Returns the power weighted by the bands, Q46.
// If the shares stop below the last band, the sound is too slow to hear in a frame.
uint64_t LevelMeter::AddShares(Sound sound, uint32_t power, const uint16_t* shares, int first, int count, int band)
{
    int start = (band + first);
    if (!power || !shares || start + count < k_bands) return 0;

    uint32_t* spectrum = m_spectrum[sound];
    const uint32_t* weights = m_tables->weights;
    int from = std::max(0, -start);
    int to = std::min(count, k_bands - start);

    uint64_t weighted = 0;
    for (int i = from; i < to; ++i)
    {
        uint32_t part = uint32_t((uint64_t(power) * shares[i]) >> 16);
        spectrum[start + i] += part;
        weighted += (uint64_t(part) * weights[start + i]);
    }
    return weighted;
}

// the same for a power in one band
uint64_t LevelMeter::AddLine(Sound sound, uint32_t power, int band)
{
    if (!power || band < 0 || band >= k_bands) return 0;

    m_spectrum[sound][band] += power;
    return (uint64_t(power) * m_tables->weights[band]);
}

uint8_t LevelMeter::ToLevel(uint64_t power, int log2Max, int32_t scale) const
{
    if (!power) return 0;
    int32_t level = (255 + (Log2(m_tables->log2, power) - log2Max) * scale / 65536);
    return uint8_t(std::clamp<int32_t>(level, 0, 255));
}
