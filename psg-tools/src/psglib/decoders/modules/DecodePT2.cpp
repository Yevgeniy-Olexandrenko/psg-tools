#include "DecodePT2.h"

const uint16_t DecodePT2::NoteTable[] = // PT3NoteTable_ST
{
    0x0EF8, 0x0E10, 0x0D60, 0x0C80, 0x0BD8, 0x0B28, 0x0A88, 0x09F0,
    0x0960, 0x08E0, 0x0858, 0x07E0, 0x077C, 0x0708, 0x06B0, 0x0640,
    0x05EC, 0x0594, 0x0544, 0x04F8, 0x04B0, 0x0470, 0x042C, 0x03FD,
    0x03BE, 0x0384, 0x0358, 0x0320, 0x02F6, 0x02CA, 0x02A2, 0x027C, 
    0x0258, 0x0238, 0x0216, 0x01F8, 0x01DF, 0x01C2, 0x01AC, 0x0190,
    0x017B, 0x0165, 0x0151, 0x013E, 0x012C, 0x011C, 0x010A, 0x00FC,
    0x00EF, 0x00E1, 0x00D6, 0x00C8, 0x00BD, 0x00B2, 0x00A8, 0x009F,
    0x0096, 0x008E, 0x0085, 0x007E, 0x0077, 0x0070, 0x006B, 0x0064,
    0x005E, 0x0059, 0x0054, 0x004F, 0x004B, 0x0047, 0x0042, 0x003F,
    0x003B, 0x0038, 0x0035, 0x0032, 0x002F, 0x002C, 0x002A, 0x0027,
    0x0025, 0x0023, 0x0021, 0x001F, 0x001D, 0x001C, 0x001A, 0x0019,
    0x0017, 0x0016, 0x0015, 0x0013, 0x0012, 0x0011, 0x0010, 0x000F
};

bool DecodePT2::Open(Stream& stream)
{
    bool isDetected = false;
    if (CheckFileExt(stream, "pt2"))
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

                    stream.info.title(ReadString(header.musicName, 30));
                    stream.info.type("ProTracker 2.x module");
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
    Header* header = (Header*)m_data;

    m_delay = header->delay;
    m_delayCounter = 1;
    m_currentPosition = 0;

    for (Channel& chan : m_ch)
    {
        memset(&chan, 0, sizeof(Channel));
        chan.ornamentPtr = header->ornamentsPointers[0];
        chan.ornamentLen  = m_data[chan.ornamentPtr++];
        chan.ornamentLoop = m_data[chan.ornamentPtr++];
        chan.volume = 0x0F;
    }

    uint16_t patternPointer  = (header->patternsPointer + header->positionList[0] * 6);
    m_ch[0].patternPtr = *(uint16_t*)(&m_data[patternPointer + 0]);
    m_ch[1].patternPtr = *(uint16_t*)(&m_data[patternPointer + 2]);
    m_ch[2].patternPtr = *(uint16_t*)(&m_data[patternPointer + 4]);

    memset(&m_regs, 0, sizeof(m_regs));
}

void DecodePT2::Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition)
{
    Header* header = (Header*)m_data;
    currPosition = m_currentPosition;
    loopPosition = header->loopPosition;
    lastPosition = header->numberOfPositions - 1;
}

bool DecodePT2::Play()
{
    bool loop = false;
    if (--m_delayCounter == 0)
    {
        for (int ch = 0; ch < 3; ++ch)
        {
            auto& chan = m_ch[ch];
            if (--chan.noteSkipCounter < 0)
            {
                if (!ch && m_data[chan.patternPtr] == 0)
                {
                    auto& hdr = reinterpret_cast<const Header&>(*m_data);
                    if (++m_currentPosition == hdr.numberOfPositions)
                    {
                        m_currentPosition = hdr.loopPosition;
                        loop = true;
                    }

                    uint16_t patternPointer  = (hdr.patternsPointer + hdr.positionList[m_currentPosition] * 6);
                    m_ch[0].patternPtr = *(uint16_t*)(&m_data[patternPointer + 0]);
                    m_ch[1].patternPtr = *(uint16_t*)(&m_data[patternPointer + 2]);
                    m_ch[2].patternPtr = *(uint16_t*)(&m_data[patternPointer + 4]);
                }
                ParsePattern(ch);
            }
        }
        m_delayCounter = m_delay;
    }

    uint8_t mixer = 0x00;
    ParseSample(0, mixer);
    ParseSample(1, mixer);
    ParseSample(2, mixer);

    m_regs[0][Mixer   ] = mixer;
    m_regs[0][A_Fine  ] = (m_ch[0].tone & 0xFF);
    m_regs[0][A_Coarse] = (m_ch[0].tone >> 8 & 0x0F);
    m_regs[0][B_Fine  ] = (m_ch[1].tone & 0xFF);
    m_regs[0][B_Coarse] = (m_ch[1].tone >> 8 & 0x0F);
    m_regs[0][C_Fine  ] = (m_ch[2].tone & 0xFF);
    m_regs[0][C_Coarse] = (m_ch[2].tone >> 8 & 0x0F);
    m_regs[0][A_Volume] = m_ch[0].amplitude;
    m_regs[0][B_Volume] = m_ch[1].amplitude;
    m_regs[0][C_Volume] = m_ch[2].amplitude;
    return loop;
}

////////////////////////////////////////////////////////////////////////////////

void DecodePT2::ParsePattern(int ch)
{
    auto& hdr  = reinterpret_cast<const Header&>(*m_data);
    auto& chan = m_ch[ch];

    bool quit  = false;
    bool gliss = false;

    do
    {
        uint8_t val = m_data[chan.patternPtr];
        if (val >= 0xe1)
        {
            chan.samplePtr = hdr.samplesPointers[val - 0xe0];
            chan.sampleLen = m_data[chan.samplePtr++];
            chan.sampleLoop = m_data[chan.samplePtr++];
        }
        else if (val == 0xe0)
        {
            chan.samplePos = 0;
            chan.ornamentPos = 0;
            chan.toneSliding = 0;
            chan.glissType = 0;
            chan.enabled = false;
            quit = true;
        }
        else if (val >= 0x80 && val <= 0xdf)
        {
            chan.samplePos = 0;
            chan.ornamentPos = 0;
            chan.toneSliding = 0;
            if (gliss)
            {
                chan.slideToNote = val - 0x80;
                if (chan.glissType == 1)
                    chan.note = chan.slideToNote;
            }
            else
            {
                chan.note = val - 0x80;
                chan.glissType = 0;
            }
            chan.enabled = true;
            quit = true;
        }
        else if (val == 0x7f)
        {
            chan.envelopeEnabled = false;
        }
        else if (val >= 0x71 && val <= 0x7e)
        {
            chan.envelopeEnabled = true;
            m_regs[0][E_Shape] = val - 0x70;
            m_regs[0][E_Fine] = m_data[++chan.patternPtr];
            m_regs[0][E_Coarse] = m_data[++chan.patternPtr];
        }
        else if (val == 0x70)
        {
            quit = true;
        }
        else if (val >= 0x60 && val <= 0x6f)
        {
            chan.ornamentPtr = hdr.ornamentsPointers[val - 0x60];
            chan.ornamentLen = m_data[chan.ornamentPtr++];
            chan.ornamentLoop = m_data[chan.ornamentPtr++];
            chan.ornamentPos = 0;
        }
        else if (val >= 0x20 && val <= 0x5f)
        {
            chan.noteSkip = val - 0x20;
        }
        else if (val >= 0x10 && val <= 0x1f)
        {
            chan.volume = val - 0x10;
        }
        else if (val == 0xf)
        {
            m_delay = m_data[++chan.patternPtr];
        }
        else if (val == 0xe)
        {
            chan.glissade = m_data[++chan.patternPtr];
            chan.glissType = 1;
            gliss = true;
        }
        else if (val == 0xd)
        {
            chan.glissade = std::abs((int8_t)(m_data[++chan.patternPtr]));

            // Do not use precalculated Ton_Delta to
            // avoide error with first note of pattern
            chan.patternPtr += 2; 
            
            chan.glissType = 2;
            gliss = true;
        }
        else if (val == 0xc)
        {
            chan.glissType = 0;
        }
        else
        {
            chan.additionToNoise = m_data[++chan.patternPtr];
        }
        chan.patternPtr++;
    } while (!quit);

    if (gliss && (chan.glissType == 2))
    {
        chan.toneDelta = std::abs(NoteTable[chan.slideToNote] - NoteTable[chan.note]);
        if (chan.slideToNote > chan.note)
            chan.glissade = -chan.glissade;
    }
    chan.noteSkipCounter = chan.noteSkip;
}

void DecodePT2::ParseSample(int ch, uint8_t& mixer)
{
    Channel& chan = m_ch[ch];

    if (chan.enabled)
    {
        uint16_t sptr = (chan.samplePtr + 3 * chan.samplePos);
        if (++chan.samplePos == chan.sampleLen)
            chan.samplePos = chan.sampleLoop;

        uint16_t optr = (chan.ornamentPtr + chan.ornamentPos);
        if (++chan.ornamentPos == chan.ornamentLen)
            chan.ornamentPos = chan.ornamentLoop;

        uint8_t s0 = m_data[sptr + 0];
        uint8_t s1 = m_data[sptr + 1];
        uint8_t s2 = m_data[sptr + 2];
        uint8_t o1 = m_data[optr + 0];

        chan.tone = s2 | ((s1 & 0x0F) << 8);
        if (!(s0 & 0b00000100)) chan.tone = -chan.tone;

        int8_t note = (chan.note + o1);
        if (note < 0 ) note = 0;
        if (note > 95) note = 95;

        chan.tone += (chan.toneSliding + NoteTable[note]);
        chan.tone &= 0x0FFF;

        if (chan.glissType == 2)
        {
            chan.toneDelta -= std::abs(chan.glissade);
            if (chan.toneDelta < 0)
            {
                chan.note = chan.slideToNote;
                chan.glissType = 0;
                chan.toneSliding = 0;
            }
        }
        if (chan.glissType != 0)
        {
            chan.toneSliding += chan.glissade;
        }

        chan.amplitude = ((chan.volume * 17 + uint8_t(chan.volume > 7)) * (s1 >> 4) + 127) / 256;
        if (chan.envelopeEnabled) chan.amplitude |= 0x10;

        if (s0 & 0b00000010) mixer |= 0b00001000;
        if (s0 & 0b00000001) mixer |= 0b01000000;
        else
        {
            uint8_t noise = ((s0 >> 3) + chan.additionToNoise);
            m_regs[0][N_Period] = (noise & 0x1F);
        }
    }
    else chan.amplitude = 0;
    mixer >>= 1;
}
