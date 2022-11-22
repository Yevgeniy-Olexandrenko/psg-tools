#pragma once

#include "decoders/Decoder.h"

class DecodePSC : public ModuleDecoder
{
#pragma pack(push, 1)
    struct Header
    {
        uint8_t  PSC_MusicName[69];
        uint16_t PSC_UnknownPointer;
        uint16_t PSC_PatternsPointer;
        uint8_t  PSC_Delay;
        uint16_t PSC_OrnamentsPointer;
        uint16_t PSC_SamplesPointers[32];
    };
#pragma pack(pop)

    struct Channel
    {
        uint8_t  num;
        uint16_t Address_In_Pattern;
        uint16_t OrnamentPointer;
        uint16_t SamplePointer;
        uint16_t Ton;
        int16_t  Current_Ton_Sliding;
        int16_t  Ton_Accumulator;
        int16_t  Addition_To_Ton;
        int8_t   Initial_Volume;
        int8_t   Note_Skip_Counter;
        uint8_t  Note;
        uint8_t  Volume;
        uint8_t  Amplitude;
        uint8_t  Volume_Counter;
        uint8_t  Volume_Counter1;
        uint8_t  Volume_Counter_Init;
        uint8_t  Noise_Accumulator;
        uint8_t  Position_In_Sample;
        uint8_t  Loop_Sample_Position;
        uint8_t  Position_In_Ornament;
        uint8_t  Loop_Ornament_Position;
        bool Enabled;
        bool Ornament_Enabled;
        bool Envelope_Enabled;
        bool Gliss;
        bool Ton_Slide_Enabled;
        bool Break_Sample_Loop;
        bool Break_Ornament_Loop;
        bool Volume_Inc;
    };

public:
    bool Open(Stream& stream) override;

protected:
    void Init() override;
    void Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition) override;
    bool Play() override;

private:
    void PatternInterpreter(Channel& chan);
    void GetRegisters(Channel& chan, uint8_t& mixer);

private:
    int m_version;
    uint8_t Delay;
    uint8_t DelayCounter;
    uint8_t Lines_Counter;
    uint8_t Noise_Base;
    uint16_t Positions_Pointer;

    Channel PSC_A;
    Channel PSC_B;
    Channel PSC_C;


    //uint8_t m_delay;
    //uint8_t m_delayCounter;
    //uint8_t m_currentPosition;

    //Channel m_chA;
    //Channel m_chB;
    //Channel m_chC;
};
