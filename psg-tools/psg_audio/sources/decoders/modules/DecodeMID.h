#pragma once

#include "decoders/Decoder.h"

class DecodeMID : public Decoder
{
#pragma pack(push, 1)
    struct Header
    {
        //
    };
#pragma pack(pop)

public:
    bool Open(Stream& stream) override;

private:
    //
};
