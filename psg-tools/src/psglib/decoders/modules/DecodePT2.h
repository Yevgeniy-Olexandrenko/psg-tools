#pragma once

#include "decoders/Decoder.h"

class DecodePT2 : public ModuleDecoder
{
    #pragma pack(push, 1)
    struct Header
    {
        uint8_t  delay;
        uint8_t  numberOfPositions;
        uint8_t  loopPosition;
        uint16_t samplesPointers[32];
        uint16_t ornamentsPointers[16];
        uint16_t patternsPointer;
        uint8_t  musicName[30];
        uint8_t  positionList[256];
    };
    #pragma pack(pop)

    struct Channel
    {
        uint16_t patternPtr;
        uint16_t ornamentPtr;
        uint16_t samplePtr;

        uint8_t ornamentLoop;
        uint8_t ornamentLen;
        uint8_t ornamentPos;
        uint8_t sampleLoop;
        uint8_t sampleLen;
        uint8_t samplePos;
        uint8_t volume;
        uint8_t noteSkip;
        uint8_t note;
        uint8_t slideToNote;

        int8_t toneSliding;
        int8_t toneDelta;
        int8_t glissType;
        int8_t glissade;
        int8_t additionToNoise;
        int8_t noteSkipCounter;

        bool envelopeEnabled;
        bool enabled;
    };

    static const uint16_t NoteTable[];

public:
    bool Open(Stream& stream) override;

protected:
    void Init() override;
    void Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition) override;
    bool Play() override;

private:
    void InitPattern();
    void ProcessPattern(int ch, uint8_t& efine, uint8_t& ecoarse, uint8_t& shape);
    void ProcessInstrument(int ch, uint8_t& tfine, uint8_t& tcoarse, uint8_t& volume, uint8_t& noise, uint8_t& mixer);

private:
    uint8_t m_delay;
    uint8_t m_delayCounter;
    uint8_t m_currentPosition;
    Channel m_channels[3];
};
