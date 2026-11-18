#include "DecodePT1.h"
#include "SharedTables.h"

bool DecodePT1::Open(Stream& stream)
{
    bool isDetected = false;
    if (CheckFileExt(stream, { "pt1" }))
    {
        std::ifstream fileStream;
        fileStream.open(stream.file, std::fstream::binary);

        if (fileStream)
        {
            fileStream.seekg(0, fileStream.end);
            uint32_t fileSize = (uint32_t)fileStream.tellg();

            if (fileSize > offsetof(Header, positionList) && fileSize <= 0x10000)
            {
                // the player can read past the end of the file,
                // so the module gets the whole 64K memory of the Z80
                m_data = new uint8_t[0x10000]();
                fileStream.seekg(0, fileStream.beg);
                fileStream.read((char*)m_data, fileSize);

                auto& hdr = reinterpret_cast<const Header&>(*m_data);
                bool isHeaderOk = true;
                isHeaderOk &= (hdr.delay > 0);
                isHeaderOk &= (hdr.numberOfPositions > 0);
                isHeaderOk &= (hdr.loopPosition < hdr.numberOfPositions);
                isHeaderOk &= (hdr.patternsPointer < fileSize);

                // the position list ends with 0xFF right in front of the patterns
                isHeaderOk &= (hdr.patternsPointer == offsetof(Header, positionList) + hdr.numberOfPositions + 1);
                isHeaderOk &= (m_data[hdr.patternsPointer - 1] == 0xFF);

                if (isHeaderOk)
                {
                    Init();
                    isDetected = true;

                    stream.info.title = ReadString((uint8_t*)hdr.musicName, 30);
                    stream.info.type = "ProTracker 1.x module";
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

void DecodePT1::Init()
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);

    m_delay = hdr.delay;
    m_delayCounter = 1;
    m_currentPosition = 0;

    for (auto& cha : m_channels)
    {
        memset(&cha, 0, sizeof(cha));
        cha.ornamentPtr = hdr.ornamentsPointers[0];
        cha.volume = 0x0F;
    }

    InitPattern();
    memset(&m_regs, 0, sizeof(m_regs));
}

void DecodePT1::Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition)
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);
    currPosition = m_currentPosition;
    loopPosition = hdr.loopPosition;
    lastPosition = hdr.numberOfPositions - 1;
}

bool DecodePT1::Play()
{
    bool loop = false;
    auto regs = m_regs[0];

    if (!--m_delayCounter)
    {
        for (int c = 0; c < 3; ++c)
        {
            auto& cha = m_channels[c];
            if (--cha.noteSkipCounter < 0)
            {
                if (!c && m_data[cha.patternPtr] == 0xFF)
                {
                    auto& hdr = reinterpret_cast<const Header&>(*m_data);
                    if (++m_currentPosition == hdr.numberOfPositions)
                    {
                        m_currentPosition = hdr.loopPosition;
                        loop = true;
                    }
                    InitPattern();
                }
                ProcessPattern(c, regs[Register::E_Fine], regs[Register::E_Coarse], regs[Register::E_Shape]);
            }
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

void DecodePT1::InitPattern()
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);
    uint16_t patternPointer = (hdr.patternsPointer + hdr.positionList[m_currentPosition] * 6);
    m_channels[0].patternPtr = (uint16_t&)(m_data[patternPointer + 0]);
    m_channels[1].patternPtr = (uint16_t&)(m_data[patternPointer + 2]);
    m_channels[2].patternPtr = (uint16_t&)(m_data[patternPointer + 4]);
}

void DecodePT1::ProcessPattern(int c, uint8_t& efine, uint8_t& ecoarse, uint8_t& shape)
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);
    auto& cha = m_channels[c];

    bool quit = false;
    do
    {
        uint8_t byte = m_data[cha.patternPtr];
        if (byte <= 0x5F)
        {
            cha.note = byte;
            cha.samplePos = 0;
            cha.enabled = true;
            quit = true;
        }
        else if (byte <= 0x6F)
        {
            cha.samplePtr  = hdr.samplesPointers[byte - 0x60];
            cha.sampleLen  = m_data[cha.samplePtr++];
            cha.sampleLoop = m_data[cha.samplePtr++];
        }
        else if (byte <= 0x7F)
        {
            cha.ornamentPtr = hdr.ornamentsPointers[byte - 0x70];
        }
        else if (byte == 0x80)
        {
            cha.enabled = false;
            quit = true;
        }
        else if (byte == 0x81)
        {
            cha.envelopeEnabled = false;
        }
        else if (byte <= 0x8F)
        {
            cha.envelopeEnabled = true;
            shape   = (byte - 0x81);
            efine   = m_data[++cha.patternPtr];
            ecoarse = m_data[++cha.patternPtr];
        }
        else if (byte == 0x90)
        {
            quit = true;
        }
        else if (byte <= 0xA0)
        {
            m_delay = (byte - 0x91);
        }
        else if (byte <= 0xB0)
        {
            cha.volume = (byte - 0xA1);
        }
        else
        {
            cha.noteSkip = (byte - 0xB1);
        }
        cha.patternPtr++;
    } while (!quit);

    cha.noteSkipCounter = cha.noteSkip;
}

void DecodePT1::ProcessInstrument(int c, uint16_t& tperiod, uint8_t& noise, uint8_t& mixer, uint8_t& volume)
{
    auto& cha = m_channels[c];
    if (cha.enabled)
    {
        uint16_t sptr = (cha.samplePtr + 3 * cha.samplePos);
        uint8_t sb0 = m_data[uint16_t(sptr + 0)];
        uint8_t sb1 = m_data[uint16_t(sptr + 1)];
        uint8_t sb2 = m_data[uint16_t(sptr + 2)];

        // ornaments have no header and follow the sample position
        uint8_t note = (cha.note + m_data[uint16_t(cha.ornamentPtr + cha.samplePos)]);
        if (note > 95) note = 95;

        uint16_t tone = ((sb0 << 4) & 0x0F00) | sb2;
        if (!(sb1 & 0b00100000)) tone = -tone;

        // the original player has 0x10B instead of 0x10A for note 46
        tone += (NoteTable_ST[note] + (note == 46));
        tperiod = (tone & 0x0FFF);

        volume = ((cha.volume * 17 + uint8_t(cha.volume > 7)) * (sb0 & 0x0F) + 128) / 256;
        if (cha.envelopeEnabled) volume |= 0x10;

        if (sb1 & 0b10000000) mixer |= 0b01000000;
        else noise = (sb1 & 0x1F);
        if (sb1 & 0b01000000) mixer |= 0b00001000;

        if (++cha.samplePos == cha.sampleLen)
            cha.samplePos = cha.sampleLoop;
    }
    else volume = 0;
    mixer >>= 1;
}
