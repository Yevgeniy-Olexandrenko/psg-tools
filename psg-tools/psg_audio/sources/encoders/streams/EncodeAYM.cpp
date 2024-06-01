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

EncodeAYM::Delta::Delta(uint16_t from, uint16_t to)
    : value(to - from)
    , bits(16)
{
    if (value <= 7i8 && value >= (-7i8 - 1)) bits = 4;
    else if (value <= 127i8 && value >= (-127i8 - 1)) bits = 8;
    else if (value <= 2047i16 && value >= (-2047i16 - 1)) bits = 12;
}

////////////////////////////////////////////////////////////////////////////////

EncodeAYM::DeltaList::DeltaList()
    : m_list{}
    , m_index(0)
{
}

int8_t EncodeAYM::DeltaList::GetIndex(const Delta& delta)
{
    int8_t index = -1;
    if (delta.bits > 4)
    {
        int size = sizeof(m_list) / sizeof(m_list[0]);
        for (int i = 0; i < size; ++i)
        {
            if (m_list[i] == delta.value)
            {
                return i;
            }
        }

        index = m_index;
        m_list[m_index] = delta.value;
        if (++m_index == size) m_index = 0;
    }
    return index;
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

void EncodeAYM::WriteFrameChunk(const Frame& frame)
{
    Chunk chunk;
    if (m_isTS)
    {
        WriteChipData(frame, 0, false, chunk);
        WriteChipData(frame, 1, true, chunk);
    }
    else
    {
        WriteChipData(frame, 0, true, chunk);
    }
    chunk.Finish();
    WriteChunk(chunk);
}

////////////////////////////////////////////////////////////////////////////////

void EncodeAYM::WriteDelta(const Delta& delta, BitOutputStream& stream)
{
    auto index = m_deltaList.GetIndex(delta);
    if (index < 0)
    {
        switch (delta.value)
        {
        default:
            stream.Write<3>(delta.bits / 4 - 1);
            stream.Write(delta.value, delta.bits);
            break;

        case  0:
            stream.Write<3>(0b100); break;
        case +1: stream.Write<3>(0b101); break;
        case -1: stream.Write<3>(0b110); break;
        }
    }
    else
    {
        stream.Write<8>(0b11100000 | index);
    }
}

void EncodeAYM::WriteChipData(const Frame& frame, int chip, bool isLast, BitOutputStream& stream)
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

    if (!isLast) hiMask |= (1 << 3);
    if (hiMask ) loMask |= (1 << 7);

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

        chunk.Finish();
        WriteChunk(chunk);

        m_oldStep = m_newStep;
        m_newStep = 1;
    }
}

void EncodeAYM::WriteChunk(const Chunk& chunk)
{
    auto data = chunk.GetData();
    auto size = chunk.GetSize();
    m_output.write(reinterpret_cast<const char*>(data), size);

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

////////////////////////////////////////////////////////////////////////////////

EncodeAYM::Chunk::Chunk()
    : BitOutputStream(m_stream)
{
}

void EncodeAYM::Chunk::Finish()
{
    Flush();
    m_data = m_stream.str();
}

const uint8_t* EncodeAYM::Chunk::GetData() const
{
    return reinterpret_cast<const uint8_t*>(m_data.data());
}

const size_t EncodeAYM::Chunk::GetSize() const
{
    return m_data.size();
}
