#include "DecodePT2.h"
#include "SharedTables.h"

bool DecodePT2::Open(Stream& stream)
{
    bool isDetected = false;
    if (CheckFileExt(stream, { "pt2" }))
    {
        std::ifstream fileStream;
        fileStream.open(stream.file, std::fstream::binary);

        if (fileStream)
        {
            fileStream.seekg(0, fileStream.end);
            uint32_t fileSize = (uint32_t)fileStream.tellg();

            if (fileSize >= 132)
            {
                Header header;
                fileStream.seekg(0, fileStream.beg);
                fileStream.read((char*)(&header), std::min((uint32_t)sizeof(Header), fileSize));

                bool isHeaderOk = true;
                isHeaderOk &= (header.delay >= 3);
                isHeaderOk &= (header.numberOfPositions > 0);
                isHeaderOk &= (header.samplesPointers[0] == 0);
                isHeaderOk &= (header.patternsPointer < fileSize);
                isHeaderOk &= (header.ornamentsPointers[0] - header.samplesPointers[0] - 2 <= int(fileSize));
                isHeaderOk &= (header.ornamentsPointers[0] - header.samplesPointers[0] >= 0);

                if (isHeaderOk)
                {
                    m_data = new uint8_t[fileSize];
                    fileStream.seekg(0, fileStream.beg);
                    fileStream.read((char*)m_data, fileSize);

                    Init();
                    isDetected = true;

                    stream.info.title = ReadString(header.musicName, 30);
                    stream.info.type = "ProTracker 2.x module";
                }
            }
            fileStream.close();
        }
    }
	return isDetected;
}

////////////////////////////////////////////////////////////////////////////////

void DecodePT2::Init()
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);

    m_delay = hdr.delay;
    m_delayCounter = 1;
    m_currentPosition = 0;

    for (auto& cha : m_channels)
    {
        memset(&cha, 0, sizeof(cha));
        cha.ornamentPtr  = hdr.ornamentsPointers[0];
        cha.ornamentLen  = m_data[cha.ornamentPtr++];
        cha.ornamentLoop = m_data[cha.ornamentPtr++];
        cha.volume = 0x0F;
    }

    InitPattern();
    memset(&m_regs, 0, sizeof(m_regs));
}

void DecodePT2::Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition)
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);
    currPosition = m_currentPosition;
    loopPosition = hdr.loopPosition;
    lastPosition = hdr.numberOfPositions - 1;
}

bool DecodePT2::Play()
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
                if (!c && !m_data[cha.patternPtr])
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

void DecodePT2::InitPattern()
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);
    uint16_t patternPointer = (hdr.patternsPointer + hdr.positionList[m_currentPosition] * 6);
    m_channels[0].patternPtr = (uint16_t&)(m_data[patternPointer + 0]);
    m_channels[1].patternPtr = (uint16_t&)(m_data[patternPointer + 2]);
    m_channels[2].patternPtr = (uint16_t&)(m_data[patternPointer + 4]);
}

void DecodePT2::ProcessPattern(int c, uint8_t& efine, uint8_t& ecoarse, uint8_t& shape)
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);
    auto& cha = m_channels[c];

    bool quit  = false;
    bool gliss = false;

    do
    {
        uint8_t byte = m_data[cha.patternPtr];
        if (byte >= 0xE1)
        {
            cha.samplePtr  = hdr.samplesPointers[byte - 0xE0];
            cha.sampleLen  = m_data[cha.samplePtr++];
            cha.sampleLoop = m_data[cha.samplePtr++];
        }
        else if (byte == 0xE0)
        {
            cha.samplePos = 0;
            cha.ornamentPos = 0;
            cha.toneSliding = 0;
            cha.glissType = 0;
            cha.enabled = false;
            quit = true;
        }
        else if (byte >= 0x80 && byte <= 0xDF)
        {
            cha.samplePos = 0;
            cha.ornamentPos = 0;
            cha.toneSliding = 0;
            if (gliss)
            {
                cha.slideToNote = (byte - 0x80);
                if (cha.glissType == 1) cha.note = cha.slideToNote;
            }
            else
            {
                cha.note = (byte - 0x80);
                cha.glissType = 0;
            }
            cha.enabled = true;
            quit = true;
        }
        else if (byte == 0x7F)
        {
            cha.envelopeEnabled = false;
        }
        else if (byte >= 0x71 && byte <= 0x7E)
        {
            cha.envelopeEnabled = true;
            shape   = (byte - 0x70);
            efine   = m_data[++cha.patternPtr];
            ecoarse = m_data[++cha.patternPtr];
        }
        else if (byte == 0x70)
        {
            quit = true;
        }
        else if (byte >= 0x60 && byte <= 0x6F)
        {
            cha.ornamentPtr  = hdr.ornamentsPointers[byte - 0x60];
            cha.ornamentLen  = m_data[cha.ornamentPtr++];
            cha.ornamentLoop = m_data[cha.ornamentPtr++];
            cha.ornamentPos  = 0;
        }
        else if (byte >= 0x20 && byte <= 0x5F)
        {
            cha.noteSkip = (byte - 0x20);
        }
        else if (byte >= 0x10 && byte <= 0x1F)
        {
            cha.volume = (byte - 0x10);
        }
        else if (byte == 0x0F)
        {
            m_delay = m_data[++cha.patternPtr];
        }
        else if (byte == 0x0E)
        {
            cha.glissade = m_data[++cha.patternPtr];
            cha.glissType = 1;
            gliss = true;
        }
        else if (byte == 0x0D)
        {
            cha.glissade = std::abs((int8_t)(m_data[++cha.patternPtr]));
            // Do not use precalculated Ton_Delta to
            // avoide error with first note of pattern
            cha.patternPtr += 2; 
            cha.glissType = 2;
            gliss = true;
        }
        else if (byte == 0x0C)
        {
            cha.glissType = 0;
        }
        else
        {
            cha.additionToNoise = m_data[++cha.patternPtr];
        }
        cha.patternPtr++;
    } while (!quit);

    if (gliss && cha.glissType == 2)
    {
        cha.toneDelta = std::abs(NoteTable_ST[cha.slideToNote] - NoteTable_ST[cha.note]);
        if (cha.slideToNote > cha.note) cha.glissade = -cha.glissade;
    }
    cha.noteSkipCounter = cha.noteSkip;
}

void DecodePT2::ProcessInstrument(int c, uint16_t& tperiod, uint8_t& noise, uint8_t& mixer, uint8_t& volume)
{
    auto& cha = m_channels[c];
    if (cha.enabled)
    {
        uint16_t sptr = (cha.samplePtr + 3 * cha.samplePos);
        if (++cha.samplePos == cha.sampleLen)
            cha.samplePos = cha.sampleLoop;

        uint16_t optr = (cha.ornamentPtr + cha.ornamentPos);
        if (++cha.ornamentPos == cha.ornamentLen)
            cha.ornamentPos = cha.ornamentLoop;

        uint8_t sb0 = m_data[sptr + 0];
        uint8_t sb1 = m_data[sptr + 1];
        uint8_t sb2 = m_data[sptr + 2];
        uint8_t ob0 = m_data[optr + 0];

        uint16_t tone = sb2 | ((sb1 & 0x0F) << 8);
        if (!(sb0 & 0b00000100)) tone = -tone;

        int8_t note = (cha.note + ob0);
        if (note < 0 ) note = 0;
        if (note > 95) note = 95;

        tone += (cha.toneSliding + NoteTable_ST[note]);
        tperiod = (tone & 0x0FFF);

        if (cha.glissType == 2)
        {
            cha.toneDelta -= std::abs(cha.glissade);
            if (cha.toneDelta < 0)
            {
                cha.note = cha.slideToNote;
                cha.glissType = 0;
                cha.toneSliding = 0;
            }
        }
        if (cha.glissType != 0)
        {
            cha.toneSliding += cha.glissade;
        }

        volume = ((cha.volume * 17 + uint8_t(cha.volume > 7)) * (sb1 >> 4) + 127) / 256;
        if (cha.envelopeEnabled) volume |= 0x10;

        if (sb0 & 0b00000010) mixer |= 0b00001000;
        if (sb0 & 0b00000001) mixer |= 0b01000000;
        else
        {
            noise = ((sb0 >> 3) + cha.additionToNoise) & 0x1F;
        }
    }
    else volume = 0;
    mixer >>= 1;
}
