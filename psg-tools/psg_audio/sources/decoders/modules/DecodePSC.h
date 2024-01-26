#pragma once

#include "decoders/Decoder.h"

class DecodePSC : public ModuleDecoder
{
#pragma pack(push, 1)
    struct Header
    {
        uint8_t  musicName[69];
        uint16_t unknownPointer;
        uint16_t patternsPointer;
        uint8_t  delay;
        uint16_t ornamentsPointer;
        uint16_t samplesPointers[32];
    };
#pragma pack(pop)

    struct Channel
    {
        uint16_t patternPtr;
        uint16_t ornamentPtr;
        uint16_t samplePtr;

        uint8_t  note;
        uint8_t  volume;
        uint8_t  Volume_Counter;
        uint8_t  Volume_Counter1;
        uint8_t  Volume_Counter_Init;
        uint8_t  Noise_Accumulator;

        uint8_t  samplePos;
        uint8_t  sampleLoop;
        uint8_t  ornamentPos;
        uint8_t  ornamentLoop;

        int16_t  toneSliding;
        int16_t  toneAcc;
        int16_t  additionToTone;
        int8_t   Initial_Volume;
        int8_t   noteSkipCounter;

        bool enabled;
        bool ornamentEnabled;
        bool envelopeEnabled;
        bool glissade;
        bool toneSlideEnabled;
        bool breakSampleLoop;
        bool breakOrnamentLoop;
        bool volumeInc;
    };

public:
    bool Open(Stream& stream) override;

protected:
    void Init() override;
    void Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition) override;
    bool Play() override;

private:
    void ProcessPattern(int c, uint16_t& tperiod, uint8_t& efine, uint8_t& ecoarse, uint8_t& shape);
    void ProcessInstrument(int c, uint16_t& tperiod, uint8_t& noise, uint8_t& mixer, uint8_t& volume, uint16_t& eperiod);

private:
    uint8_t  m_delay;
    uint8_t  m_delayCounter;
    uint8_t  m_linesCounter;
    uint8_t  m_noiseBase;
    uint16_t m_positionsPtr;
    Channel  m_channels[3];
};
