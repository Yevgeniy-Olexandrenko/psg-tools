#pragma once

#include "decoders/Decoder.h"

class DecodePT3 : public ModuleDecoder
{
    #pragma pack(push, 1)
    struct Header
    {
        char     musicName[0x63];
        uint8_t  tonTableId;
        uint8_t  delay;
        uint8_t  numberOfPositions;
        uint8_t  loopPosition;
        uint16_t patternsPointer;
        uint8_t  samplesPointers[32 * 2];
        uint8_t  ornamentsPointers[16 * 2];
        uint8_t  positionList[1];
    };
    #pragma pack(pop)

    struct Channel
    {
        uint16_t patternPtr;
        uint16_t ornamentPtr;
        uint16_t samplePtr;
        
        int volumeSliding;
        int noiseSliding;
        int envelopeSliding;
        int tonSlideCount;
        int currentOnOff;
        int onOffDelay;
        int offOnDelay;
        int tonSlideDelay;
        int toneSliding;
        int toneAcc;
        int tonSlideStep;
        int toneDelta;

        int8_t noteSkipCounter;

        uint8_t ornamentLoop;
        uint8_t ornamentLen;
        uint8_t ornamentPos;

        uint8_t sampleLoop;
        uint8_t sampleLen;
        uint8_t samplePos;

        uint8_t volume;
        uint8_t numberOfNotesToSkip;
        uint8_t note;
        uint8_t slideToNote;
        
        bool envelopeEnabled;
        bool enabled;
        bool simpleGliss;

        uint16_t tone;
        uint8_t amplitude;
    };

    struct Global
    {
        int curEnvSlide;
        int envSlideAdd;
        uint8_t envBaseLo;
        uint8_t envBaseHi;

        uint8_t noiseBase;
        uint8_t noiseAdd;

        int8_t curEnvDelay;
        int8_t envDelay;
    };

    struct Module
    {
        uint8_t* m_data;
        Header*  m_hdr;
        
        uint8_t m_delay;
        uint8_t m_delayCounter;
        uint8_t m_currentPosition;
        Channel m_channels[3];

        Global glob;
        int ts;
        
    };

    static const uint16_t NoteTable_PT_33_34r[];
    static const uint16_t NoteTable_PT_34_35[];
    static const uint16_t NoteTable_ST[];
    static const uint16_t NoteTable_ASM_34r[];
    static const uint16_t NoteTable_ASM_34_35[];
    static const uint16_t NoteTable_REAL_34r[];
    static const uint16_t NoteTable_REAL_34_35[];

    static const uint8_t VolumeTable_33_34[16][16];
    static const uint8_t VolumeTable_35[16][16];

public:
	bool Open(Stream& stream) override;

protected:
    void Init() override;
    void Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition) override;
    bool Play() override;

private:
    bool Play(int chip);
    void ProcessPattern(int m, Channel& chan);
    void ProcessInstrument(int m, int c, uint8_t& mixer, int& envAdd);
    int  GetToneFromNote(int m, int note);
    
private:
    uint32_t m_size;
    uint8_t  m_ver;

    Module m_module[2];
};