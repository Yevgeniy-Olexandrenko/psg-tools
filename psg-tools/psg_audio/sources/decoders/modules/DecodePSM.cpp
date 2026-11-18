#include "DecodePSM.h"

namespace
{
    const uint16_t NoteTable_PSM[96]
    {
        0x0D3D, 0x0C7F, 0x0BCB, 0x0B22, 0x0A82, 0x09EB, 0x095D, 0x08D6, 0x0857, 0x07DF, 0x076E, 0x0703,
        0x069F, 0x063F, 0x05E6, 0x0591, 0x0541, 0x04F6, 0x04AE, 0x046B, 0x042C, 0x03F0, 0x03B7, 0x0382,
        0x034F, 0x0320, 0x02F3, 0x02C8, 0x02A1, 0x027B, 0x0257, 0x0236, 0x0216, 0x01F8, 0x01DC, 0x01C1,
        0x01A8, 0x0190, 0x0179, 0x0164, 0x0150, 0x013D, 0x012C, 0x011B, 0x010B, 0x00FC, 0x00EE, 0x00E0,
        0x00D4, 0x00C8, 0x00BD, 0x00B2, 0x00A8, 0x009F, 0x0096, 0x008D, 0x0085, 0x007E, 0x0077, 0x0070,
        0x006A, 0x0064, 0x005E, 0x0059, 0x0054, 0x004F, 0x004B, 0x0047, 0x0043, 0x003F, 0x003B, 0x0038,
        0x0035, 0x0032, 0x002F, 0x002D, 0x002A, 0x0028, 0x0025, 0x0023, 0x0021, 0x001F, 0x001E, 0x001C,
        0x001A, 0x0019, 0x0018, 0x0016, 0x0015, 0x0014, 0x0013, 0x0012, 0x0011, 0x0010, 0x000F, 0x000E
    };

    const char PSMSignature[] = "psm1";
}

bool DecodePSM::Open(Stream& stream)
{
    bool isDetected = false;
    if (CheckFileExt(stream, { "psm" }))
    {
        std::ifstream fileStream;
        fileStream.open(stream.file, std::fstream::binary);

        if (fileStream)
        {
            fileStream.seekg(0, fileStream.end);
            uint32_t fileSize = (uint32_t)fileStream.tellg();

            if (fileSize > offsetof(Header, remark) && fileSize <= 0x10000)
            {
                // the player can read past the end of the file,
                // so the module gets the whole 64K memory of the Z80
                m_data = new uint8_t[0x10000]();
                fileStream.seekg(0, fileStream.beg);
                fileStream.read((char*)m_data, fileSize);

                auto& hdr = reinterpret_cast<const Header&>(*m_data);
                bool isHeaderOk = true;
                isHeaderOk &= (hdr.positionsPointer >= offsetof(Header, remark));
                isHeaderOk &= (hdr.positionsPointer < hdr.samplesPointer);
                isHeaderOk &= (hdr.samplesPointer <= hdr.ornamentsPointer);
                isHeaderOk &= (hdr.ornamentsPointer <= hdr.patternsPointer);
                isHeaderOk &= (hdr.patternsPointer < fileSize);

                if (isHeaderOk)
                {
                    // the position list holds pairs of pattern and transposition,
                    // it ends with 0xFF and the loop position (0xFF for no loop)
                    uint16_t ptr = hdr.positionsPointer;
                    int numberOfPositions = 0;
                    while (ptr + 1 < hdr.samplesPointer && m_data[ptr] != 0xFF)
                    {
                        ptr += 2;
                        numberOfPositions++;
                    }
                    isHeaderOk &= (numberOfPositions > 0 && numberOfPositions <= 0xFF);
                    isHeaderOk &= (ptr + 1 < hdr.samplesPointer);

                    if (isHeaderOk)
                    {
                        m_lastPosition = uint8_t(numberOfPositions - 1);
                        m_loopPosition = m_data[ptr + 1];
                        if (m_loopPosition > m_lastPosition) m_loopPosition = 0;
                    }
                }

                if (isHeaderOk)
                {
                    Init();
                    isDetected = true;

                    // the remark between the header and the position list
                    // starts with the signature of the tracker
                    auto remark = (uint8_t*)hdr.remark;
                    int remarkLen = std::min(hdr.positionsPointer - int(offsetof(Header, remark)), 0xFF);
                    if (remarkLen >= 5 && !memcmp(remark, PSMSignature, 5))
                    {
                        remark += 5;
                        remarkLen -= 5;
                    }
                    if (remarkLen > 0)
                    {
                        stream.info.title = ReadString(remark, uint8_t(remarkLen));
                    }
                    stream.info.type = "Pro Sound Maker module";
                }
                else
                {
                    delete[] m_data;
                    m_data = nullptr;
                }
            }
            fileStream.close();
        }
    }
    return isDetected;
}

////////////////////////////////////////////////////////////////////////////////

void DecodePSM::Init()
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);

    for (auto& cha : m_channels)
    {
        memset(&cha, 0, sizeof(cha));
        cha.noteSkipCounter = 1;
        cha.note = -128;
    }

    m_delayCounter = 1;
    m_currentPosition = 0;
    m_transposition = int8_t(m_data[hdr.positionsPointer + 1] + 48);
    InitPattern(m_data[hdr.positionsPointer]);
    memset(&m_regs, 0, sizeof(m_regs));
}

void DecodePSM::Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition)
{
    currPosition = m_currentPosition;
    loopPosition = m_loopPosition;
    lastPosition = m_lastPosition;
}

bool DecodePSM::Play()
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);
    bool loop = false;
    auto regs = m_regs[0];

    if (!--m_delayCounter)
    {
        // the channels are processed in the order C, B, A,
        // channel C holds the end of the pattern
        auto& chaC = m_channels[2];
        if (!--chaC.noteSkipCounter)
        {
            if (m_data[chaC.patternPtr] == 0xFF)
            {
                uint16_t ptr = (hdr.positionsPointer + ++m_currentPosition * 2);
                uint8_t pattern = m_data[ptr];
                if (pattern == 0xFF)
                {
                    // the end of the position list: go to the loop position or stop
                    uint8_t position = m_data[ptr + 1];
                    if (position == 0xFF) return true;

                    m_currentPosition = position;
                    pattern = m_data[hdr.positionsPointer + position * 2];
                    loop = true;
                }
                m_transposition = int8_t(m_data[hdr.positionsPointer + m_currentPosition * 2 + 1] + 48);
                InitPattern(pattern);

                for (auto& cha : m_channels)
                {
                    cha.returnCounter = 0;
                    cha.note |= 0x80;
                }
                m_channels[0].noteSkipCounter = 1;
                m_channels[1].noteSkipCounter = 1;
            }
            ProcessPattern(2, regs[Register::E_Fine], regs[Register::E_Coarse], regs[Register::E_Shape]);
        }
        if (!--m_channels[1].noteSkipCounter)
        {
            ProcessPattern(1, regs[Register::E_Fine], regs[Register::E_Coarse], regs[Register::E_Shape]);
        }
        if (!--m_channels[0].noteSkipCounter)
        {
            ProcessPattern(0, regs[Register::E_Fine], regs[Register::E_Coarse], regs[Register::E_Shape]);
        }
        m_delayCounter = m_delay;
    }

    regs[Register::Mixer] = 0;
    ProcessInstrument(0, (uint16_t&)regs[Register::A_Fine], regs[Register::N_Period], regs[Register::Mixer], regs[Register::A_Volume]);
    ProcessInstrument(1, (uint16_t&)regs[Register::B_Fine], regs[Register::N_Period], regs[Register::Mixer], regs[Register::B_Volume]);
    ProcessInstrument(2, (uint16_t&)regs[Register::C_Fine], regs[Register::N_Period], regs[Register::Mixer], regs[Register::C_Volume]);
    return loop;
}

////////////////////////////////////////////////////////////////////////////////

void DecodePSM::InitPattern(uint8_t pattern)
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);
    uint16_t ptr = (hdr.patternsPointer + pattern * 7);
    m_delay = m_data[ptr];
    m_channels[0].patternPtr = (uint16_t&)(m_data[ptr + 1]);
    m_channels[1].patternPtr = (uint16_t&)(m_data[ptr + 3]);
    m_channels[2].patternPtr = (uint16_t&)(m_data[ptr + 5]);
}

void DecodePSM::SetEnvelope(uint8_t type, uint8_t period, uint8_t& efine, uint8_t& ecoarse, uint8_t& shape)
{
    // types 0xB1-0xB7 select the shapes 8-14, periods
    // from 0xF1 set the coarse part, the others the fine part
    shape = (type - 0xB1 + 8);
    efine = (period >= 0xF1 ? 0 : period);
    ecoarse = (period >= 0xF1 ? period & 0x0F : 0);
}

void DecodePSM::ProcessPattern(int c, uint8_t& efine, uint8_t& ecoarse, uint8_t& shape)
{
    auto& cha = m_channels[c];
    uint16_t ptr = cha.patternPtr;

    // the end of a repeated block returns to the pattern
    if (cha.returnCounter && !--cha.returnCounter)
    {
        ptr = cha.returnPtr;
    }

    for (bool quit = false; !quit; ++ptr)
    {
        uint8_t byte = m_data[ptr];
        if (byte <= 0x5F)
        {
            // the first note of the pattern is relative to the transposition,
            // the other notes are relative to the previous note
            cha.note = (cha.note < 0 ? int8_t(m_transposition - byte) : int8_t(cha.note - byte));
            if (cha.note < 0) cha.note = int8_t(cha.note + 96);

            cha.volumeCounter = cha.volume;
            cha.samplePos = 0;
            cha.toneSliding = 0;
            cha.loopCounter = 1;
            cha.ornamentPos &= (int8_t(cha.ornamentPos) < 0 ? 0xE0 : 0xC0);

            if ((cha.ornamentPos & 0x40) && cha.ornament >= 33)
            {
                if (cha.envelopeType >= 0xB1)
                {
                    SetEnvelope(cha.envelopeType, cha.envelopePeriod, efine, ecoarse, shape);
                }
                else
                {
                    // types 0xA1-0xB0 select the shapes 8, 10, 12, 14 and
                    // a period that follows the note in one of 4 intervals
                    uint8_t type = (cha.envelopeType - 0xA1);
                    shape = (((type & 3) << 1) | 8);

                    uint8_t note = uint8_t((type & 12) * 3 + cha.note);
                    if (note >= 48)
                    {
                        note -= 48;
                        if (note >= 48) note -= 48;
                    }
                    uint16_t period = NoteTable_PSM[std::min(note + 48, 95)];
                    efine = uint8_t(period);
                    ecoarse = uint8_t(period >> 8);
                }
            }
            quit = true;
        }
        else if (byte == 0x60)
        {
            cha.samplePos |= 0x80;
            quit = true;
        }
        else if (byte <= 0x6F)
        {
            cha.sample = (byte - 0x61);
        }
        else if (byte <= 0x8F)
        {
            cha.ornament = (byte - 0x70);
            cha.ornamentPos = 0;
        }
        else if (byte == 0x90)
        {
            quit = true;
        }
        else if (byte <= 0x9F)
        {
            cha.volume = (byte - 0x90);
        }
        else if (byte == 0xA0)
        {
            // the ornament is off
            cha.ornamentPos = 0xA0;
        }
        else if (byte <= 0xB0)
        {
            cha.ornament = 33;
            cha.envelopeType = byte;
            cha.ornamentPos |= 0x40;
        }
        else if (byte <= 0xB7)
        {
            cha.envelopeType = byte;
            cha.envelopePeriod = m_data[++ptr];
            SetEnvelope(cha.envelopeType, cha.envelopePeriod, efine, ecoarse, shape);
            cha.ornamentPos |= 0x40;
        }
        else if (byte <= 0xF8)
        {
            cha.noteSkip = (byte - 0xB7);
        }
        else if (byte == 0xF9)
        {
            // repeat the block at the address for the given number of notes
            cha.returnPtr = (ptr + 4);
            cha.returnCounter = m_data[uint16_t(ptr + 3)];
            ptr = (m_data[uint16_t(ptr + 1)] | m_data[uint16_t(ptr + 2)] << 8) - 1;
        }
        else if (byte <= 0xFB)
        {
            cha.ornament = (byte - 0xFA + 32);
        }
        else
        {
            quit = true;
        }
    }

    cha.patternPtr = ptr;
    cha.noteSkipCounter = cha.noteSkip;
}

void DecodePSM::ProcessInstrument(int c, uint16_t& tperiod, uint8_t& noise, uint8_t& mixer, uint8_t& volume)
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);
    auto& cha = m_channels[c];

    const auto ReadWord = [&](uint16_t ptr) -> uint16_t
    {
        return (m_data[ptr] | m_data[uint16_t(ptr + 1)] << 8);
    };

    // ornament: 2 bytes of length and loop, then the note offsets
    uint16_t optr = ReadWord(hdr.ornamentsPointer + cha.ornament * 2);
    uint8_t note = (cha.note & 0x7F);
    if (!(cha.ornamentPos & 0x60)) note += m_data[uint16_t(optr + 2 + cha.ornamentPos)];
    if (int8_t(note) < 0) note = 0;
    else if (note > 95) note = 95;

    // sample: 2 bytes of length, loop and volume slide, then 3 bytes per tick
    uint16_t sptr = ReadWord(hdr.samplesPointer + cha.sample * 2);
    uint8_t  spos = uint8_t(cha.samplePos * 3);
    uint8_t  sb0 = m_data[uint16_t(sptr + 2 + spos)];
    uint8_t  sb1 = m_data[uint16_t(sptr + 3 + spos)];
    uint8_t  sb2 = m_data[uint16_t(sptr + 4 + spos)];

    // 11-bit signed tone delta, accumulated from the note start
    uint16_t delta = ((sb1 & 0x07) << 8 | sb2);
    if (sb1 & 0x04) delta |= 0xF800;
    cha.toneSliding += delta;
    cha.tone = (NoteTable_PSM[note] + cha.toneSliding);
    if (int16_t(cha.tone) < 0) cha.tone = 0;
    else if (cha.tone > 0x0FFF) cha.tone = 0x0FFF;
    tperiod = cha.tone;

    // the envelope flag and the volume add to the sample amplitude
    uint8_t amplitude = (sb0 & 0x0F);
    if (cha.ornamentPos & 0x40) amplitude |= 0x10;
    amplitude += (cha.volumeCounter - 15);
    if (int8_t(amplitude) < 0 || int8_t(cha.samplePos) < 0) amplitude = 0;
    volume = amplitude;

    mixer |= ((sb0 >> 1) & 0b01001000);
    if (int8_t(cha.samplePos) < 0) mixer |= 0b01000000;
    if (int8_t(sb0) >= 0 && amplitude) noise = (sb1 >> 3);

    // next sample position: stop at the end, or loop and slide the volume
    uint8_t pos = (cha.samplePos & 0x1F) + 1;
    uint8_t len = m_data[sptr + 0];
    uint8_t lop = m_data[uint16_t(sptr + 1)];
    if (pos > (len & 0x1F))
    {
        if (!(lop & 0xE0))
        {
            cha.samplePos |= 0x80;
        }
        else
        {
            pos = (lop & 0x1F);
            if (!--cha.loopCounter)
            {
                cha.loopCounter = (lop >> 5);
                if (!(len & 0x20))
                    cha.volumeCounter += (len >> 6);
                else
                    cha.volumeCounter -= (len >> 6) + 1;

                if (int8_t(cha.volumeCounter) < 0) cha.volumeCounter = 0;
                else if (cha.volumeCounter > 15) cha.volumeCounter = 15;
            }
        }
    }
    cha.samplePos = ((cha.samplePos & 0xE0) | (pos & 0x1F));

    // next ornament position: loop or stop at the end
    pos = (cha.ornamentPos & 0x1F) + 1;
    len = m_data[optr + 0];
    lop = m_data[uint16_t(optr + 1)];
    if (pos > len)
    {
        if (int8_t(lop) < 0)
            pos = lop;
        else
            cha.ornamentPos |= 0x20;
    }
    cha.ornamentPos = ((cha.ornamentPos & 0xE0) | (pos & 0x1F));

    mixer >>= 1;
}
