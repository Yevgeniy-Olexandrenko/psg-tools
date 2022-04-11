#pragma once

#include <stdint.h>
#include "Decoder.h"

class DecodeASC : public Decoder
{
#pragma pack(push, 1)
    struct Header
    {
        uint8_t  delay;
        uint8_t  loopingPosition;
        uint16_t patternsPointers;
        uint16_t samplesPointers;
        uint16_t ornamentsPointers;
        uint8_t  numberOfPositions;
        uint8_t  positions[256];
    };
#pragma pack(pop)

    struct Channel
    {
        uint16_t initialPointInSample;
        uint16_t pointInSample;
        uint16_t loopPointInSample;

        uint16_t initialPointInOrnament;
        uint16_t pointInOrnament;
        uint16_t loopPointInOrnament;

        uint16_t addressInPattern;
        uint16_t ton;
        uint16_t tonDeviation;

        uint8_t note;
        uint8_t additionToNote;
        uint8_t numberOfNotesToSkip;
        uint8_t initialNoise;
        uint8_t currentNoise;
        uint8_t volume;
        uint8_t tonSlidingCounter;

        uint8_t amplitude;
        uint8_t amplitudeDelay;
        uint8_t amplitudeDelayCounter;

        int16_t currentTonSliding;
        int16_t substructionForTonSliding;

        int8_t noteSkipCounter;
        int8_t additionToAmplitude;

        bool envelopeEnabled;
        bool soundEnabled;
        bool sampleFinished;
        bool breakSampleLoop;
        bool breakOrnamentLoop;
    };

public:
    bool Open(Module& module) override;
    bool Decode(Frame& frame) override;
    void Close(Module& module) override;

private:
    bool Init();
    bool Step();
    void PatternInterpreter(Channel& chan);
    void GetRegisters(Channel& chan, uint8_t& mixer);
    bool Play();

private:
    uint8_t* m_data;
    uint32_t m_loop;
    uint32_t m_tick;

    uint8_t m_delay;
    uint8_t m_delayCounter;
    uint8_t m_currentPosition;

    Channel m_chA;
    Channel m_chB;
    Channel m_chC;

    uint8_t m_regs[16];
};
