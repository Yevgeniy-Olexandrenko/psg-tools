#pragma once

#include <array>
#include <sstream>
#include "encoders/Encoder.h"
#include "utils/BitStream.h"
#include "utils/LZ78.h"

class EncodeAYM : public Encoder
{
    enum Technique
    {
        DELTA_CACHE = 1 << 0,
        CHUNK_CACHE = 1 << 1,
        ENCODE_LZ78 = 1 << 2
    };

    struct Delta
    {
        int16_t value;
        size_t  bits;

        Delta(uint16_t from, uint16_t to);
    };

    struct DeltaCache
    {
        int nextRecord{ 0 };
        std::array<int16_t, 64> cache;
        
        int FindRecord(const Delta& delta);
    };

    class  Chunk : public BitOutputStream
    {
        std::ostringstream stream;

    public:
        Chunk();
        using Data = std::string;
        const Data GetData();
    };

    struct ChunkCache
    {
        int nextRecord{ 0 };
        std::array<Chunk::Data, 64> cache;

        int FindRecord(const Chunk::Data& chunkData);
    };

public:
    enum class Profile : uint8_t
    {
        Low    = DELTA_CACHE,
        Medium = DELTA_CACHE + CHUNK_CACHE,
        High   = CHUNK_CACHE + ENCODE_LZ78
    };

    EncodeAYM();
    void Configure(Profile profile);

    bool Open(const Stream& stream) override;
    void Encode(const Frame& frame) override;
    void Close(const Stream& stream) override;

private:
    void WriteDelta(const Delta& delta, BitOutputStream& stream);
    void WriteRegisters(const Frame& frame, int chip, BitOutputStream& stream);
    void WriteSkipChunk();
    void WriteFrameChunk(const Frame& frame);
    void WriteChunk(Chunk& chunk, BitOutputStream& stream);
    void DebugPrint(const char* data, size_t size);

private:
    static uint8_t m_profile;
    std::ofstream m_output;
    BitOutputStream m_stream;
    DeltaCache m_deltaCache;
    ChunkCache m_chunkCache;
    bool  m_isTS{ false };
    int   m_skip{ 0 };
    Frame m_frame;

    LZ78Encoder<Chunk::Data, 4096> m_lz78Encoder;
};
