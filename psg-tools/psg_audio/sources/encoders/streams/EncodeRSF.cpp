#include "EncodeRSF.h"

namespace
{
    const uint32_t RSFSignature = 0x03465352;
}

bool EncodeRSF::Open(const Stream& stream)
{
    if (CheckFileExt(stream, "rsf"))
    {
        m_output.open(stream.file, std::fstream::binary);
        if (m_output)
        {
            size_t dataOffset = 20
                + ((std::string)stream.info.title).length() + 1
                + ((std::string)stream.info.artist).length() + 1
                + ((std::string)stream.info.comment).length() + 1;

            Header header;
            header.m_sigAndVer = RSFSignature;
            header.m_frameRate = stream.play.frameRate;
            header.m_dataOffset = uint16_t(dataOffset);
            header.m_frameCount = uint32_t(stream.framesCount);
            header.m_loopFrame = stream.loopFrameId;
            header.m_chipFreq = stream.dchip.clockValue;
            m_output.write((char*)&header, sizeof(header));

            m_output << (std::string)stream.info.title << char(0);
            m_output << (std::string)stream.info.artist << char(0);
            m_output << (std::string)stream.info.comment << char(0);

            m_skip = 0;
            return true;
        }
    }
    return false;
}

void EncodeRSF::Encode(const Frame& frame)
{
    if (frame.HasChanges())
    {
        WriteSkip();

        // write mask of changed registers
        uint16_t mask = 0;
        for (int reg = Register::BankA_Lst; reg >= Register::BankA_Fst; --reg)
        {
            mask <<= 1;
            mask |= uint16_t(frame[0].IsChanged(reg));
        }
        m_output << uint8_t(mask >> 8) << uint8_t(mask);

        // write values of changed registers
        for (int reg = 0; mask; mask >>= 1, ++reg)
        {
            if (mask & 0x01)
                m_output << frame[0].Read(reg);
        }
    }
    else ++m_skip;
}

void EncodeRSF::Close(const Stream& stream)
{
    WriteSkip();
    m_output.close();
}

void EncodeRSF::WriteSkip()
{
    if (m_skip)
    {
        if (m_skip == 1) m_output << uint8_t(0xFF);
        else m_output << uint8_t(0xFE) << uint8_t(m_skip);
        m_skip = 0;
    }
}
