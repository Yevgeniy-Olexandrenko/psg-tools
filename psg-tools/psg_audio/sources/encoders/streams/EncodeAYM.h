#pragma once

#include <array>
#include <sstream>
#include "encoders/Encoder.h"
#include "utils/BitStream.h"

#define DBG_ENCODE_AYM 1

#define AYM_OPT_DELTA_CACHE  1
#define AYM_OPT_DELTA_SHORTS 1

class EncodeAYM : public Encoder
{
    struct Delta
    {
        int16_t value;
        size_t  bits;

        Delta(uint16_t from, uint16_t to);
    };

    struct DeltaCache
    {
        std::array<int16_t, 64> cache;
        int nextRecord{ 0 };

        int8_t FindRecord(const Delta& delta);
    };

    class Chunk : public BitOutputStream
    {
    public:
        Chunk();
        void Finish();
        
        const uint8_t* GetData() const;
        const size_t GetSize() const;

    protected:
        std::ostringstream m_stream;
        std::string m_data;
    };

public:
    bool Open(const Stream& stream) override;
    void Encode(const Frame& frame) override;
    void Close(const Stream& stream) override;

private:
    void WriteDelta(const Delta& delta, BitOutputStream& stream);
    void WriteRegsData(const Frame& frame, int chip, bool isLast, BitOutputStream& stream);
    void WriteStepChunk();
    void WriteFrameChunk(const Frame& frame);
    void WriteChunk(const Chunk& chunk);

private:
    std::ofstream m_output;
    DeltaCache m_deltaCache;
    uint16_t m_oldStep = 1;
    uint16_t m_newStep = 1;
    Frame m_frame;
    bool m_isTS;
};

