#pragma once

#include "decoders/Decoder.h"

class DecodePT3 : public ModuleDecoder
{
    #pragma pack(push, 1)
    struct Header
    {
        uint8_t  musicName[99];
        uint8_t  tonTableId;
        uint8_t  delay;
        uint8_t  numberOfPositions;
        uint8_t  loopPosition;
        uint16_t patternsPointer;
        uint16_t samplesPointers[32];
        uint16_t ornamentsPointers[16];
        uint8_t  positionList[1];
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

        int8_t noteSkipCounter;
        
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

        bool simpleGliss;
        bool envelopeEnabled;
        bool enabled;
    };

    struct Global
    {
        uint8_t envBaseLo;
        uint8_t envBaseHi;

        int curEnvSlide;
        int envSlideAdd;

        int8_t curEnvDelay;
        int8_t envDelay;

        uint8_t noiseBase;
        uint8_t noiseAdd;
    };

    struct Module
    {
        uint8_t* m_data;
        Header*  m_hdr;
        
        uint8_t m_delay;
        uint8_t m_delayCounter;
        uint8_t m_currentPosition;
        Channel m_channels[3];
        Global  m_global;

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
    bool PlayModule(int m);
    void ProcessPattern(int m, int c, uint8_t& shape);
    void ProcessInstrument(int m, int c, uint8_t& tfine, uint8_t& tcoarse, uint8_t& volume, uint8_t& mixer, int& envAdd);
    int  GetToneFromNote(int m, int note);
    
private:
    int    m_size;
    int    m_version;
    Module m_module[2];
};