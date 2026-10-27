#include "DecodeYM.h"
#include "lha/lzh.h"

namespace
{
    uint16_t GetU16(const uint8_t* p) { return (p[0] << 8 | p[1]); }
    uint32_t GetU32(const uint8_t* p) { return (p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]); }

    // finds the end of a null-terminated string inside the data
    bool SkipString(const std::vector<uint8_t>& data, size_t& offset, std::string& str)
    {
        for (size_t end = offset; end < data.size(); ++end)
        {
            if (!data[end])
            {
                str.assign((const char*)&data[offset], end - offset);
                offset = end + 1;
                return true;
            }
        }
        return false;
    }
}

bool DecodeYM::Open(Stream& stream)
{
    std::ifstream fileStream;
    fileStream.open(stream.file, std::fstream::binary);
    if (!fileStream) return false;

    fileStream.seekg(0, fileStream.end);
    size_t fileSize = (size_t)fileStream.tellg();
    if (fileSize < 22) return false;

    // YM file is usually an LZH archive, but it can be unpacked
    uint8_t header[22];
    fileStream.seekg(0, fileStream.beg);
    fileStream.read((char*)header, sizeof(header));

    bool isPacked = (!memcmp(&header[2], "-lh", 3) && header[6] == '-');
    bool isRaw = (!memcmp(header, "YM", 2));
    if (!isPacked && !isRaw) return false;

    std::vector<uint8_t> file(fileSize);
    fileStream.seekg(0, fileStream.beg);
    if (!fileStream.read((char*)file.data(), fileSize)) return false;
    fileStream.close();

    if (isPacked)
    {
        m_data.clear();
        if (lzh::Unpack(file.data(), file.size(), m_data) != lzh::Result::Ok) return false;
    }
    else
    {
        m_data.swap(file);
    }

    if (m_data.size() < 4) return false;
    std::string id((char*)m_data.data(), 4);

    if (id == "YM5!" || id == "YM6!")
    {
        if (m_data.size() < sizeof(HeaderYM5)) return false;
        const HeaderYM5* ym5 = (const HeaderYM5*)m_data.data();

        size_t offset = sizeof(HeaderYM5);
        offset += GetU16(ym5->addSize);

        // skip digidrum samples
        for (int i = 0, c = GetU16(ym5->numOfDig); i < c; i++)
        {
            if (offset + 4 > m_data.size()) return false;
            uint32_t count = GetU32(&m_data[offset]);
            offset += (4 + size_t(count));
        }

        std::string title, author, comment;
        if (offset > m_data.size()) return false;
        if (!SkipString(m_data, offset, title)) return false;
        if (!SkipString(m_data, offset, author)) return false;
        if (!SkipString(m_data, offset, comment)) return false;

        m_version = id[2];
        m_offset = offset;
        m_frames = GetU32(ym5->numOfFrames);
        m_loop   = GetU32(ym5->loopFrame);
        m_frame  = 0;
        m_interleaved = ((GetU32(ym5->songAttr) & 1) != 0);
        if (!m_frames || m_frames > (m_data.size() - m_offset) / 16) return false;

        stream.schip.clockValue = GetU32(ym5->chipFreq);
        stream.play.frameRate = GetU16(ym5->playFreq);
        stream.info.title = title;
        stream.info.artist = author;
        stream.info.comment = comment;
        stream.info.type = id.substr(0, 3) + " stream";
        return true;
    }

    if (id == "YM2!" || id == "YM3!" || id == "YM3b")
    {
        m_version = id[2];
        m_offset = id.size();
        m_frames = (m_data.size() - id.size()) / 14;
        m_loop   = 0;
        m_frame  = 0;
        m_interleaved = true;

        if (id[3] == 'b')
        {
            // last four bytes (DWORD data) are frame number for looping
            if (m_data.size() < 8) return false;
            m_loop = *(uint32_t*)(&m_data[m_data.size() - 4]);
        }
        if (!m_frames) return false;

        stream.play.frameRate = 50; // already set to 50 in stream by default
        stream.info.type = id.substr(0, 3) + " stream";
        return true;
    }
    return false;
}

bool DecodeYM::Decode(Frame& frame)
{
    if (m_frame >= m_frames) return false;
    size_t frameSize = m_interleaved ? 1 : 16;
    size_t valueSize = m_interleaved ? m_frames : 1;
    const uint8_t* dataPtr = (m_data.data() + m_offset) + (m_frame * frameSize);
    m_frame++;

    uint8_t regs[14];
    for (uint8_t& value : regs)
    {
        value = *dataPtr;
        dataPtr += valueSize;
    }

    // digidrums are not supported: on the frame that starts a digidrum
    // the volume register holds the sample number, so mute the voice
    const auto MuteDigidrum = [&](uint8_t effect)
    {
        if (int voice = (effect >> 4 & 0x03))
        {
            regs[Register::A_Volume + voice - 1] = 0;
        }
    };
    if (m_version == '5')
    {
        MuteDigidrum(regs[Register::B_Coarse]);
    }
    else if (m_version == '6')
    {
        // effect type 1 is digidrum
        if ((regs[Register::A_Coarse] >> 6) == 1) MuteDigidrum(regs[Register::A_Coarse]);
        if ((regs[Register::B_Coarse] >> 6) == 1) MuteDigidrum(regs[Register::B_Coarse]);
    }

    for (Register reg = Register::BankA_Fst; reg <= Register::BankA_Lst; ++reg)
    {
        frame[0].Update(reg, regs[reg]);
    }

    return true;
}

void DecodeYM::Close(Stream& stream)
{
    stream.Finalize(m_loop);
    m_data.clear();
}
