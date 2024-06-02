#include "EncodeAYM.h"
#include <cassert>

////////////////////////////////////////////////////////////////////////////////

// debug output
#if DBG_ENCODE_AYM
std::ofstream debug_out;
#define DebugOpen() \
    debug_out.open("dbg_encode_aym.txt");
#define DebugPrintByteValue(dd) \
    debug_out << std::hex << std::setw(2) << std::setfill('0') << int(dd); \
    debug_out << ' ';
#define DebugPrintMessage(msg) \
    debug_out << msg; \
    debug_out << ' ';
#define DebugPrintNewLine() \
    debug_out << std::endl;
#define DebugClose() \
    debug_out.close();
#else
#define DebugOpen()
#define DebugPrintWrite(aa, bb)
#define DebugPrintNewLine()
#define DebugClose()
#endif

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
#if AYM_OPT_DELTA_CACHE
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
#endif
    return -1;
}

EncodeAYM::Chunk::Chunk()
    : BitOutputStream(stream)
{
}

const std::string EncodeAYM::Chunk::GetData()
{
    Flush();
    return stream.str();
}

int EncodeAYM::ChunkCache::FindRecord(Chunk& chunk)
{
    // TODO

    return -1;
}

////////////////////////////////////////////////////////////////////////////////

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

            DebugOpen();
            return true;
        }
    }
    return false;
}

void EncodeAYM::Encode(const Frame& frame)
{
    if (frame.HasChanges())
    {
        WriteStepChunk();
        WriteFrameChunk(frame);
    }
    else
    {
        m_newStep++;
    }
    m_frame = frame;
}

void EncodeAYM::Close(const Stream& stream)
{
    WriteStepChunk();
    m_output.close();
    DebugClose();
}

////////////////////////////////////////////////////////////////////////////////

void EncodeAYM::WriteDelta(const Delta& delta, BitOutputStream& stream)
{
    auto record = m_deltaCache.FindRecord(delta);
    if (record < 0)
    {
#if AYM_OPT_DELTA_SHORTS
        // write delta shorts
        if (delta.value == +1) stream.Write<3>(0b100);
        else
        if (delta.value == -1) stream.Write<3>(0b101);
        else
#endif
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

void EncodeAYM::WriteRegsData(const Frame& frame, int chip, BitOutputStream& stream)
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

void EncodeAYM::WriteStepChunk()
{
    if (m_newStep != m_oldStep)
    {
        Chunk chunk;
        chunk.Write<8>(0x00);
        WriteDelta({ m_oldStep, m_newStep }, chunk);
        WriteChunk(chunk);

        m_oldStep = m_newStep;
        m_newStep = 1;
    }
}

void EncodeAYM::WriteFrameChunk(const Frame& frame)
{
    Chunk chunk;
    WriteRegsData(frame, 0, chunk);
    if (m_isTS) WriteRegsData(frame, 1, chunk);
    WriteChunk(chunk);
}

void EncodeAYM::WriteChunk(Chunk& chunk)
{
    const std::string& chunkData = chunk.GetData();
    auto data = chunkData.c_str();
    auto size = chunkData.size();
    m_output.write(data, size);

#if DBG_ENCODE_AYM
    for (size_t i = 0; i < size; ++i)
    {
        uint8_t dd = data[i];
        if (i == 0)
        {
            if (dd) { DebugPrintMessage("regs:"); }
            else    { DebugPrintMessage("skip:"); }
        }
        DebugPrintByteValue(dd);
    }
    DebugPrintNewLine();
#endif
}
