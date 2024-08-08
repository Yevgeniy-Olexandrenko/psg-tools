#pragma once

#include <array>
#include <sstream>
#include "encoders/Encoder.h"
#include "utils/BitStream.h"
#include "utils/LZ78.h"

class EncodeAYM : public Encoder
{
    enum
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

    class Chunk : public BitOutputStream
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

#pragma pack(push, 1)
    struct Header
    {
        uint32_t formatSig;  // format signature        : AYYM
        uint8_t  formatVer;  // format version          : 1 for now
        uint8_t  encProfile; // encoding profile flags  : Low|Medium|High
        uint8_t  chipCount;  // number of chips         : 1|2 supported
        uint32_t chipFreq;   // chip(s) clock frequency : 1000000-2000000 Hz
        uint8_t  chipOutput; // chip(s) output config   : 0-6 MONO|ABC|ACB|BAC|BCA|CAB|CBA
        uint16_t frameRate;  // frame rate for playback : 50|60 usually
        uint32_t frameCount; // number of frames        : 0-99999 usually
        uint32_t frameLoop;  // loop frame for playback : 0-99999 usually
    };
#pragma pack(pop)

public:
    enum class Profile
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
    void WriteChunk(Chunk& chunk);
    void WriteLZ78EncodedData();
    void FinalizeWriting();

private:
    static uint8_t s_profile;

    std::ofstream m_output;
    BitOutputStream m_stream;
    DeltaCache m_deltaCache;
    ChunkCache m_chunkCache;
    Frame m_frame;
    int m_chips;
    int m_skip;

    LZ78Encoder<Chunk::Data, 4096> m_lz78Encoder;
};
