#pragma once

#include <stdint.h>

struct Chip;
class Frame;

// Finds the spectrum and the output levels of a frame from its registers, so
// it works for all outputs, the hardware one too. For each channel it finds
// the power of the sound in the frame: the DAC level of the volume or of the
// envelope, switched by the tone and the noise, without the DC part. It
// follows the envelope phase from frame to frame. The spectrum of the
// waveform spreads the power over the bands. The levels weigh the bands by
// loudness, add the panned channels and use a decibel scale. The tables use
// floating point and are built one time; a frame uses integers only.
class LevelMeter
{
public:
    // the bands are semitones from E0 (20.6 Hz) to D#10 (19.9 kHz)
    static constexpr int k_bands = 120;
    static constexpr double k_firstBandFreq = 20.6017223070544;

    enum Sound : uint8_t { Tone, Noise, Envelope };
    static constexpr int k_sounds = 3;

    LevelMeter();

    void Init(const Chip& chip, int frameRate);

    // Measures a frame that goes to the chip(s). The levels of the left and
    // the right outputs are 0 (silence or below the range) to 255 (all
    // channels at the maximum volume with a tone at 1 kHz).
    void Measure(const Frame& frame, uint8_t& L, uint8_t& R);

    // Gives the spectrum of the last frame for a display: the level of each
    // band, 0 to 255 (a tone at the maximum volume, all in the band), and the
    // loudest sound in the band.
    void GetBands(uint8_t* levels, Sound* sounds) const;

private:
    struct Steps;
    struct Tables;
    static const Tables& GetTables();

    // the level of a channel in the frame and the spectrum of its changes
    struct Level
    {
        uint32_t m1;            // mean DAC output, Q15
        uint32_t m2;            // mean square of the DAC output, Q30
        const uint16_t* shares; // band shares from the fundamental, none for a single ramp
        int band;               // band of the fundamental
    };

    void MeasureChip(int chip, const Frame& frame, uint64_t& powerL, uint64_t& powerR);
    Level MeasureEnvelope(int chip, int env, bool isExpMode, const Frame& frame, bool restart);
    int ToBand(uint32_t divisor) const;
    uint64_t AddShares(Sound sound, uint32_t power, const uint16_t* shares, int first, int count, int band);
    uint64_t AddLine(Sound sound, uint32_t power, int band);
    uint8_t ToLevel(uint64_t power, int log2Max, int32_t scale) const;

private:
    const Tables* m_tables;
    int m_chips;
    uint64_t m_stepsPerFrame; // envelope steps in a frame at the period 1, Q16
    int m_clockBand;          // semitones from the first band to the clock, Q8

    bool m_hasExpMode[2];
    bool m_isGateOR[2];
    const uint16_t* m_dac[2];
    const Steps* m_steps[2][2]; // [chip][mode]
    uint32_t m_phase[2][3];     // envelope steps after the restart, Q16, [chip][envelope]

    uint32_t m_panL[3]; // squares of the pans, Q16
    uint32_t m_panR[3];
    int m_log2MaxPower;

    uint32_t m_spectrum[k_sounds][k_bands]; // power, Q30
};
