#pragma once

#include "decoders/Decoder.h"

class DecodePSM : public ModuleDecoder
{
    #pragma pack(push, 1)
    struct Header
    {
        uint16_t positionsPointer;
        uint16_t samplesPointer;
        uint16_t ornamentsPointer;
        uint16_t patternsPointer;
        uint8_t  remark[1];
    };
    #pragma pack(pop)

    struct Channel
    {
        uint16_t patternPtr;
        uint16_t returnPtr;
        uint16_t toneSliding;
        uint16_t tone;

        uint8_t noteSkip;
        uint8_t noteSkipCounter;
        uint8_t returnCounter;
        uint8_t volume;
        uint8_t volumeCounter;
        uint8_t loopCounter;
        uint8_t ornament;
        uint8_t sample;
        uint8_t envelopeType;
        uint8_t envelopePeriod;

        // bits 0-4: position, bit 7: the sample is stopped
        uint8_t samplePos;
        // bits 0-4: position, bit 5: the ornament is stopped,
        // bit 6: envelope, bit 7: the ornament is off
        uint8_t ornamentPos;
        // bit 7: the next note is relative to the transposition
        int8_t note;
    };

public:
    bool Open(Stream& stream) override;

protected:
    void Init() override;
    void Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition) override;
    bool Play() override;

private:
    void InitPattern(uint8_t pattern);
    void SetEnvelope(uint8_t type, uint8_t period, uint8_t& efine, uint8_t& ecoarse, uint8_t& shape);
    void ProcessPattern(int c, uint8_t& efine, uint8_t& ecoarse, uint8_t& shape);
    void ProcessInstrument(int c, uint16_t& tperiod, uint8_t& noise, uint8_t& mixer, uint8_t& volume);

private:
    uint8_t m_delay;
    uint8_t m_delayCounter;
    uint8_t m_currentPosition;
    uint8_t m_lastPosition;
    uint8_t m_loopPosition;
    int8_t  m_transposition;
    Channel m_channels[3];
};
