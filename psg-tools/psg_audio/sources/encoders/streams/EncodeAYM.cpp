#include "EncodeAYM.h"
#include "debug/DebugOutput.h"

static DebugOutput<DBG_ENCODE_AYM> dbg;
static void dbg_print_chunk(const char* data, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        uint8_t dd = data[i];
        if (i == 0)
        {
            if (dd) dbg.print_message("reg%02X: ", uint8_t(size));
            else    dbg.print_message("cmd%02X: ", data[i + 1] & 0b11000000);
        }
        dbg.print_message("%02X ", dd);
    }
    dbg.print_endline();
}

uint8_t EncodeAYM::s_profile{ uint8_t(Profile::High) };
enum { TAG_CMD = 0x00, CMD_SKIP = 0, CMD_CREF = 1, CMD_RESERVED_2 =  2, CMD_RESERVED_3 = 3 };

////////////////////////////////////////////////////////////////////////////////

EncodeAYM::Delta::Delta(uint16_t from, uint16_t to)
    : value(to - from)
    , bits(16)
{
    if (value <= 7i8 && value >= (-7i8 - 1)) bits = 4;
    else if (value <= 127i8 && value >= (-127i8 - 1)) bits = 8;
    else if (value <= 2047i16 && value >= (-2047i16 - 1)) bits = 12;
}

int EncodeAYM::DeltaCache::FindRecord(const Delta& delta)
{
    if (s_profile & DELTA_CACHE)
    {
        if (delta.bits > 4)
        {
            auto cacheSize = int(cache.size());
            for (int record = 0; record < cacheSize; ++record)
            {
                if (cache[record] == delta.value) return record;
            }

            cache[nextRecord] = delta.value;
            if (++nextRecord >= cacheSize) nextRecord = 0;
        }
    }
    return -1;
}

EncodeAYM::Chunk::Chunk()
    : BitOutputStream(stream)
{
}

const EncodeAYM::Chunk::Data EncodeAYM::Chunk::GetData()
{
    Flush();
    return stream.str();
}

int EncodeAYM::ChunkCache::FindRecord(const Chunk::Data& chunkData)
{
    if (s_profile & CHUNK_CACHE)
    {
        if (chunkData.size() > 2)
        {
            auto cacheSize = int(cache.size());
            for (int record = 0; record < cacheSize; ++record)
            {
                if (cache[record] == chunkData) return record;
            }

            cache[nextRecord] = chunkData;
            if (++nextRecord >= cacheSize) nextRecord = 0;
        }
    }
    return -1;
}

////////////////////////////////////////////////////////////////////////////////

EncodeAYM::EncodeAYM()
    : m_stream(m_output)
    , m_chips(1)
    , m_skip(0)
{
}

void EncodeAYM::Configure(Profile profile)
{
    s_profile = uint8_t(profile);
}

bool EncodeAYM::Open(const Stream& stream)
{
    if (CheckFileExt(stream, "aym"))
    {
        m_output.open(stream.file, std::fstream::binary);
        if (m_output)
        {
            if (stream.IsSecondChipUsed()) m_chips = 2;
            uint8_t stereo = uint8_t(Chip::Stereo(stream.dchip.stereo));
            uint8_t output = (stream.dchip.output == Chip::Output::Mono ? 0 : stereo);

            // write basic info header
            Header header;
            header.formatSig  = 0x4D595941;
            header.formatVer  = 1;
            header.encProfile = s_profile;
            header.chipCount  = m_chips;
            header.chipFreq   = uint32_t(stream.dchip.clockValue);
            header.chipOutput = output;
            header.frameRate  = uint16_t(stream.play.frameRate);
            header.frameCount = uint32_t(stream.framesCount);
            header.frameLoop  = uint32_t(stream.loopFrameId);
            m_output.write((char*)&header, sizeof(header));

            // write chip(s) model info
            m_output << uint8_t(Chip::Model(stream.dchip.first.model));
            if (m_chips > 1) m_output << uint8_t(Chip::Model(stream.dchip.second.model));

            // write media info
            auto title = std::string(stream.info.title);
            auto artist = std::string(stream.info.artist);
            auto comment = std::string(stream.info.comment);
            m_output << uint8_t(title.size()) << title;
            m_output << uint8_t(artist.size()) << artist;
            m_output << uint8_t(comment.size()) << comment;

            dbg.open("encode_aym_" + stream.ToString(Stream::Property::Tag));
            return true;
        }
    }
    return false;
}

void EncodeAYM::Encode(const Frame& frame)
{
    if (frame.HasChanges())
    {
        WriteSkipChunk();
        WriteFrameChunk(frame);
    }
    else 
    {
        m_skip++;
    }
    m_frame = frame;
}

void EncodeAYM::Close(const Stream& stream)
{
    FinalizeWriting();
    m_output.close();
    dbg.close();
}

////////////////////////////////////////////////////////////////////////////////

void EncodeAYM::WriteDelta(const Delta& delta, BitOutputStream& stream)
{
    auto record = m_deltaCache.FindRecord(delta);
    if (record < 0)
    {
        // write delta shorts
        if (delta.value == +1) stream.Write<3>(0b100);
        else
        if (delta.value == -1) stream.Write<3>(0b101);
        else
        {   // write width and value of delta
            stream.Write<3>(delta.bits / 4 - 1);
            stream.Write(delta.value, delta.bits);
        }
    }
    else
    {   // write record number in cache
        // instead of the delta itself
        stream.Write<8>(0b11000000 | record);
    }
}

void EncodeAYM::WriteRegisters(const Frame& frame, int chip, BitOutputStream& stream)
{
    const auto CheckRChanged = [&](Register r, uint8_t& flags, uint8_t bit)
    {
        if (frame[chip].IsChanged(r)) flags |= (1 << bit);
    };

    const auto CheckPChanged = [&](Register::Period p, uint8_t& flags, uint8_t bit)
    {
        if (frame[chip].IsChanged(p)) flags |= (1 << bit);
    };

    const auto WriteRDelta = [&](const uint8_t& flags, uint8_t bit, Register r)
    {
        if (flags & (1 << bit)) 
            WriteDelta({ m_frame[chip].Read(r), frame[chip].Read(r) }, stream);
    };

    const auto WritePDelta = [&](const uint8_t& flags, uint8_t bit, Register::Period p)
    {
        if (flags & (1 << bit))
            WriteDelta({ m_frame[chip].Read(p), frame[chip].Read(p) }, stream);
    };

    uint8_t flags0 = 0; // 8 bit
    uint8_t flags1 = 0; // 4 bit
    uint8_t flags2 = 0; // 4 bit
    uint8_t flags3 = 0; // 6 bit

    CheckRChanged(Register::Mixer,     flags0, 0);
    CheckPChanged(Register::Period::A, flags0, 1);
    CheckRChanged(Register::A_Volume,  flags0, 2);
    CheckPChanged(Register::Period::B, flags0, 3);
    CheckRChanged(Register::B_Volume,  flags0, 4);
    CheckPChanged(Register::Period::C, flags0, 5);
    CheckRChanged(Register::C_Volume,  flags0, 6);
    CheckPChanged(Register::Period::N, flags1, 0);
    CheckPChanged(Register::Period::E, flags1, 1);
    CheckRChanged(Register::E_Shape,   flags0, 2);
    
    if (frame[chip].IsExpMode())
    {
        CheckRChanged(Register::A_Duty,     flags2, 0);
        CheckRChanged(Register::B_Duty,     flags2, 1);
        CheckRChanged(Register::C_Duty,     flags2, 2);
        CheckPChanged(Register::Period::EB, flags3, 0);
        CheckRChanged(Register::EB_Shape,   flags3, 1);
        CheckPChanged(Register::Period::EC, flags3, 2);
        CheckRChanged(Register::EC_Shape,   flags3, 3);
        CheckRChanged(Register::N_AndMask,  flags3, 4);
        CheckRChanged(Register::N_OrMask,   flags3, 5);
    }

    if (flags3) flags2 |= (1 << 3);
    if (flags2) flags1 |= (1 << 3);
    if (flags1) flags0 |= (1 << 7);

    stream.Write<8>(flags0);
    if (flags0 & (1 << 7)) stream.Write<4>(flags1);
    if (flags1 & (1 << 3)) stream.Write<4>(flags2);
    if (flags2 & (1 << 3)) stream.Write<6>(flags3);

    WriteRDelta(flags0, 0, Register::Mixer);
    WritePDelta(flags0, 1, Register::Period::A);
    WriteRDelta(flags0, 2, Register::A_Volume);
    WritePDelta(flags0, 3, Register::Period::B);
    WriteRDelta(flags0, 4, Register::B_Volume);
    WritePDelta(flags0, 5, Register::Period::C);
    WriteRDelta(flags0, 6, Register::C_Volume);
    WritePDelta(flags1, 0, Register::Period::N);
    WritePDelta(flags1, 1, Register::Period::E);
    WriteRDelta(flags1, 2, Register::E_Shape);
    WriteRDelta(flags2, 0, Register::A_Duty);
    WriteRDelta(flags2, 1, Register::B_Duty);
    WriteRDelta(flags2, 2, Register::C_Duty);
    WritePDelta(flags3, 0, Register::Period::EB);
    WriteRDelta(flags3, 1, Register::EB_Shape);
    WritePDelta(flags3, 2, Register::Period::EC);
    WriteRDelta(flags3, 3, Register::EC_Shape);
    WriteRDelta(flags3, 4, Register::N_AndMask);
    WriteRDelta(flags3, 5, Register::N_OrMask);
}

void EncodeAYM::WriteSkipChunk()
{
    if (m_skip > 0)
    {
        for (; m_skip >= 64; m_skip -= 64)
        {
            Chunk chunk;
            chunk.Write<8>(TAG_CMD).Write<2>(CMD_SKIP).Write<6>(64 - 1);
            WriteChunk(chunk);
        }

        Chunk chunk;
        chunk.Write<8>(TAG_CMD).Write<2>(CMD_SKIP).Write<6>(m_skip - 1);
        WriteChunk(chunk);
        m_skip = 0;
    }
}

void EncodeAYM::WriteFrameChunk(const Frame& frame)
{
    Chunk chunk;
    for (int chip = 0; chip < m_chips; ++chip)
    {
        WriteRegisters(frame, chip, chunk);
    }
    WriteChunk(chunk);
}

void EncodeAYM::WriteChunk(Chunk& chunk)
{
    Chunk::Data chunkData = chunk.GetData();

    auto record = m_chunkCache.FindRecord(chunkData);
    if (record >= 0)
    {
        Chunk chunk;
        chunk.Write<8>(TAG_CMD).Write<2>(CMD_CREF).Write<6>(record);
        chunkData = chunk.GetData();
    }

    if (s_profile & ENCODE_LZ78)
    {
        if (m_lz78Encoder.Encode(chunkData))
        {
            WriteLZ78EncodedData();
        }
    }
    else
    {
        m_stream.Write(chunkData.c_str(), chunkData.size());
        dbg_print_chunk(chunkData.c_str(), chunkData.size());
    }
}

void EncodeAYM::WriteLZ78EncodedData()
{
    auto encoded = m_lz78Encoder.GetEncodedData();
    auto index = encoded.first;

    if (index == 0) m_stream.Write<1>(0b0);
    else if (index < 4096) m_stream.Write<1>(0b1).Write<12>(index);
    else dbg.print_message("(!) ");
    m_stream.Write(encoded.second.c_str(), encoded.second.size());

    dbg.print_message("LZ78-%03X: ", encoded.first);
    dbg_print_chunk(encoded.second.c_str(), encoded.second.size());
}

void EncodeAYM::FinalizeWriting()
{
    WriteSkipChunk();
    if (s_profile & ENCODE_LZ78)
    {
        if (m_lz78Encoder.HasUnfinishedEncoding())
        {
            WriteLZ78EncodedData();
        }
    }
    m_stream.Flush();
}
