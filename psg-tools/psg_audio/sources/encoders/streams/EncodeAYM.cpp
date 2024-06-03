#include "EncodeAYM.h"
#include "debug/DebugOutput.h"

static DebugOutput<DBG_ENCODE_AYM> dbg;
static void DebugPrint(const char* data, size_t size)
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

uint8_t EncodeAYM::m_profile{ uint8_t(Profile::High) };
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
    if (m_profile & DELTA_CACHE)
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
    if (m_profile & CHUNK_CACHE)
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
{
}

void EncodeAYM::Configure(Profile profile)
{
    m_profile = uint8_t(profile);
}

bool EncodeAYM::Open(const Stream& stream)
{
    if (CheckFileExt(stream, "aym"))
    {
        m_output.open(stream.file, std::fstream::binary);
        if (m_output)
        {
            m_isTS = stream.IsSecondChipUsed();
            m_output << "AYYM";

            // TODO

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
    const auto WriteRDelta = [&](Register r)
        {
            WriteDelta({ m_frame[chip].Read(r), frame[chip].Read(r) }, stream);
        };

    const auto WritePDelta = [&](PRegister p)
        {
            WriteDelta({ m_frame[chip].Read(p), frame[chip].Read(p) }, stream);
        };

    uint8_t loMask = 0;
    uint8_t hiMask = 0;

    if (frame[chip].IsChanged(Register::Mixer))       loMask |= (1 << 0);
    if (frame[chip].IsChanged__(PRegister::A_Period)) loMask |= (1 << 1);
    if (frame[chip].IsChanged(Register::A_Volume))    loMask |= (1 << 2);
    if (frame[chip].IsChanged__(PRegister::B_Period)) loMask |= (1 << 3);
    if (frame[chip].IsChanged(Register::B_Volume))    loMask |= (1 << 4);
    if (frame[chip].IsChanged__(PRegister::C_Period)) loMask |= (1 << 5);
    if (frame[chip].IsChanged(Register::C_Volume))    loMask |= (1 << 6);
    if (frame[chip].IsChanged__(PRegister::N_Period)) hiMask |= (1 << 0);
    if (frame[chip].IsChanged__(PRegister::E_Period)) hiMask |= (1 << 1);
    if (frame[chip].IsChanged(Register::E_Shape))     hiMask |= (1 << 2);

 // if (!isLast) hiMask |= (1 << 3);
    if ( hiMask) loMask |= (1 << 7);

    stream.Write<8>(loMask);
    if (loMask & (1 << 7)) stream.Write<4>(hiMask);

    if (loMask & (1 << 0)) WriteRDelta(Register::Mixer);
    if (loMask & (1 << 1)) WritePDelta(PRegister::A_Period);
    if (loMask & (1 << 2)) WriteRDelta(Register::A_Volume);
    if (loMask & (1 << 3)) WritePDelta(PRegister::B_Period);
    if (loMask & (1 << 4)) WriteRDelta(Register::B_Volume);
    if (loMask & (1 << 5)) WritePDelta(PRegister::C_Period);
    if (loMask & (1 << 6)) WriteRDelta(Register::C_Volume);
    if (hiMask & (1 << 0)) WritePDelta(PRegister::N_Period);
    if (hiMask & (1 << 1)) WritePDelta(PRegister::E_Period);
    if (hiMask & (1 << 2)) WriteRDelta(Register::E_Shape);
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
    WriteRegisters(frame, 0, chunk);
    if (m_isTS) WriteRegisters(frame, 1, chunk);
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

    if (m_profile & ENCODE_LZ78)
    {
        if (m_lz78Encoder.Encode(chunkData))
        {
            WriteLZ78EncodedData();
        }
    }
    else
    {
        m_stream.Write(chunkData.c_str(), chunkData.size());
        DebugPrint(chunkData.c_str(), chunkData.size());
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
    DebugPrint(encoded.second.c_str(), encoded.second.size());
}

void EncodeAYM::FinalizeWriting()
{
    WriteSkipChunk();
    if (m_profile & ENCODE_LZ78)
    {
        if (m_lz78Encoder.HasUnfinishedEncoding())
        {
            WriteLZ78EncodedData();
        }
    }
    m_stream.Flush();
}
