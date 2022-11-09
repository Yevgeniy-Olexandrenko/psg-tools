#pragma once

#include "decoders/Decoder.h"

class DecodePSC : public ModuleDecoder
{
#pragma pack(push, 1)
    struct Header
    {
        //
    };
#pragma pack(pop)

    struct Channel
    {
        //
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
    //uint8_t m_delay;
    //uint8_t m_delayCounter;
    //uint8_t m_currentPosition;

    //Channel m_chA;
    //Channel m_chB;
    //Channel m_chC;
};
