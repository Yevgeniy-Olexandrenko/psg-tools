#pragma once

#include "ChipSim.h"
#include "stream/Chip.h"
#include "emulation/RP2A03Apu.h"

class SimRP2A03 : public ChipSim, public RP2A03Apu
{
public:
    enum class OutputType
    {
        SingleChip, DoubleChip, AY8930Chip
    };

    SimRP2A03();

    void ConfigureClock(int srcClock, int dstClock) override;
    void ConfigureOutput(OutputType outputType, Chip::Model model);

    void Reset() override;
    void Write(int chip, Register reg, uint8_t data) override;
    void Simulate(int samples) override;
    void Convert(Frame& frame) override;

private:
    struct State;

    void ConvertToSingleChip(const State& state, Frame& frame);
    void ConvertToDoubleChip(const State& state, Frame& frame);
    void ConvertToAY8930Chip(const State& state, Frame& frame);

    template<Register::Value shape_reg, Register::Period period_reg>
    void ConvertTriangle(const State& state, Frame& frame, uint8_t& mixer, uint8_t* volumes);
    void UpdateVolumes(Frame::Registers& regs, const uint8_t* volumes);

    void    PlaceNoise(State& state);
    uint8_t SharedNoiseChannels(const State& state) const;
    void    MixNoise(const State& state, uint8_t& mixer, uint8_t* volumes) const;
    uint16_t YMNoisePeriod(const State& state) const;
    void    UpdateAY8930Noise(const State& state, Frame::Registers& regs) const;
    bool    ShortNoiseAsTone(const State& state) const;
    void    ShortNoiseMasks(const State& state, uint8_t& orMask, uint8_t& period) const;

    float   VolumeToLevel(uint8_t volume) const;
    uint8_t LevelToVolume(float   level ) const;
    float   PulseLevel   (uint8_t volume) const;
    uint8_t PulseVolume  (uint8_t volume) const;
    float   NoiseLevel   (const State& state) const;
    uint8_t NoiseVolume  (const State& state) const;
    float   ShortNoiseGain(const State& state) const;

private:
    OutputType m_outputType;
    int m_triangleHold;      // frames left to hold the level of a stopped triangle

    uint8_t m_noiseChannels; // mask of the channels of the noise in the last frame
    int m_noiseAge;          // frames since the last hit of the noise
    uint8_t m_noiseVolume;   // NES volume of the noise in the last frame

    // output levels of the volumes of the destination chip
    const float* m_levels;
    int m_levelsCount;
    int m_levelsStep;
    float m_psgTriangleRms;
};
