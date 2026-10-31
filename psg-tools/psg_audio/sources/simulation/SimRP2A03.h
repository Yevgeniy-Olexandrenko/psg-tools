#pragma once

#include "ChipSim.h"
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
    void ConfigureOutput(OutputType outputType);

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
    void DistributeNoiseBetweenChannels(const State& state, uint8_t& mixer, uint8_t* volumes);
    void UpdateVolumes(Frame::Registers& regs, const uint8_t* volumes);

    float   VolumeToLevel(uint8_t volume) const;
    uint8_t LevelToVolume(float   level ) const;
    uint8_t PulseVolume  (uint8_t volume) const;
    uint8_t NoiseVolume  (uint8_t volume) const;

private:
    OutputType m_outputType;
    uint8_t m_noiseAndMask;
    int m_triangleHold;
};
