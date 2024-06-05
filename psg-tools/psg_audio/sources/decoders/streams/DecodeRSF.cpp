#include "DecodeRSF.h"

namespace
{
	const uint32_t RSFSignature = 0x03465352;
}

bool DecodeRSF::Open(Stream& stream)
{
    m_input.open(stream.file, std::fstream::binary);
    if (m_input)
    {
        Header header;
        m_input.read((char*)(&header), sizeof(header));

        if (header.m_sigAndVer == RSFSignature)
        {
            std::string title; std::getline(m_input, title, char(0));
            std::string author; std::getline(m_input, author, char(0));
            std::string comment; std::getline(m_input, comment, char(0));

            if (m_input.tellg() == header.m_dataOffset)
            {
                stream.info.type = "RSF stream";
                stream.info.title = title;
                stream.info.artist = author;
                stream.info.comment = comment;

                stream.play.frameRate = header.m_frameRate;
                stream.schip.clockValue = header.m_chipFreq;
                m_loop = header.m_loopFrame;

                m_skip = 0;
                return true;
            }
        }
    }
    return false;
}

bool DecodeRSF::Decode(Frame& frame)
{
    if (m_skip) m_skip--;
    else
    {
        if (m_input.peek() == EOF) return false;

        uint8_t val;
        m_input.get((char&)val);

        if (val == 0xFF) return true;
        else 
        if (val == 0xFE)
        {
            m_input.get((char&)val);
            m_skip = (val - 1);
        }
        else
        {
            union { uint16_t w; uint8_t b[2]; } mask;
            m_input.get((char&)mask.b[0]);
            mask.b[1] = val;

            for (Register reg = 0; mask.w; mask.w >>= 1, ++reg)
            {
                if (mask.w & 0x01)
                {
                    m_input.get((char&)val);
                    frame[0].Update(reg, val);
                }
            }
        }
    }
    return true;
}

void DecodeRSF::Close(Stream& stream)
{
    stream.Finalize(m_loop);
    m_input.close();
}
