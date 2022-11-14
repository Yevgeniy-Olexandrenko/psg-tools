#include "DecodeSTC.h"

namespace
{
    const uint16_t STCNoteTable[] =
    {
        0x0ef8, 0x0e10, 0x0d60, 0x0c80, 0x0bd8, 0x0b28, 0x0a88, 0x09f0,
        0x0960, 0x08e0, 0x0858, 0x07e0, 0x077c, 0x0708, 0x06b0, 0x0640,
        0x05ec, 0x0594, 0x0544, 0x04f8, 0x04b0, 0x0470, 0x042c, 0x03f0,
        0x03be, 0x0384, 0x0358, 0x0320, 0x02f6, 0x02ca, 0x02a2, 0x027c,
        0x0258, 0x0238, 0x0216, 0x01f8, 0x01df, 0x01c2, 0x01ac, 0x0190,
        0x017b, 0x0165, 0x0151, 0x013e, 0x012c, 0x011c, 0x010b, 0x00fc,
        0x00ef, 0x00e1, 0x00d6, 0x00c8, 0x00bd, 0x00b2, 0x00a8, 0x009f,
        0x0096, 0x008e, 0x0085, 0x007e, 0x0077, 0x0070, 0x006b, 0x0064,
        0x005e, 0x0059, 0x0054, 0x004f, 0x004b, 0x0047, 0x0042, 0x003f,
        0x003b, 0x0038, 0x0035, 0x0032, 0x002f, 0x002c, 0x002a, 0x0027,
        0x0025, 0x0023, 0x0021, 0x001f, 0x001d, 0x001c, 0x001a, 0x0019,
        0x0017, 0x0016, 0x0015, 0x0013, 0x0012, 0x0011, 0x0010, 0x000f
    };
}

////////////////////////////////////////////////////////////////////////////////

bool DecodeSTC::Open(Stream& stream)
{
    bool isDetected = false;
    if (CheckFileExt(stream, { "stc" }))
    {
        std::ifstream fileStream;
        fileStream.open(stream.file, std::fstream::binary);

        if (fileStream)
        {
            fileStream.seekg(0, fileStream.end);
            uint32_t fileSize = (uint32_t)fileStream.tellg();

            if (fileSize >= sizeof(Header))
            {
                Header header;
                fileStream.seekg(0, fileStream.beg);
                fileStream.read((char*)(&header), sizeof(header));

                bool isHeaderOK = true;
                isHeaderOK &= (header.positionsPointer < fileSize);
                isHeaderOK &= (int(header.patternsPointer - header.ornamentsPointer) > 0);
                isHeaderOK &= (int(header.positionsPointer - header.ornamentsPointer) < 0);
                isHeaderOK &= (((header.patternsPointer - header.ornamentsPointer) % 0x21) == 0);

                if (isHeaderOK)
                {
                    m_data = new uint8_t[fileSize];
                    fileStream.seekg(0, fileStream.beg);
                    fileStream.read((char*)m_data, fileSize);

                    Init();
                    isDetected = true;

                    auto identifier = (uint8_t*)(&header.identifier);
                    if (memcmp(identifier, "SONG BY ST COMPILE", 18) &&
                        memcmp(identifier, "SONG BY MB COMPILE", 18) &&
                        memcmp(identifier, "SONG BY ST-COMPILE", 18) &&
                        memcmp(identifier, "SOUND TRACKER v1.1", 18) &&
                        memcmp(identifier, "S.T.FULL EDITION ", 17) &&
                        memcmp(identifier, "SOUND TRACKER v1.3", 18))
                    {
                        int length = 18;
                        if (header.size != fileSize)
                        {
                            if (identifier[18] >= 32 && identifier[18] <= 127)
                            {
                                length++;
                                if (identifier[19] >= 32 && identifier[19] <= 127)
                                    length++;
                            }
                        }
                        stream.info.comment(ReadString(header.identifier, length));
                    }
                    stream.info.type("Sound Tracker module");
                }
            }
            fileStream.close();
        }
    }
    return isDetected;
}

////////////////////////////////////////////////////////////////////////////////

void DecodeSTC::Init()
{
    Header* header = (Header*)m_data;

    memset(&m_chA, 0, sizeof(Channel));
    memset(&m_chB, 0, sizeof(Channel));
    memset(&m_chC, 0, sizeof(Channel));

    m_delayCounter = 1;
    m_transposition = m_data[header->positionsPointer + 2];
    m_currentPosition = 0;
    
    uint16_t patternPointer = header->patternsPointer;
    while (m_data[patternPointer] != m_data[header->positionsPointer + 1]) patternPointer += 7;
    m_chA.addressInPattern = *(uint16_t*)(&m_data[patternPointer + 1]);
    m_chB.addressInPattern = *(uint16_t*)(&m_data[patternPointer + 3]);
    m_chC.addressInPattern = *(uint16_t*)(&m_data[patternPointer + 5]);

    for (Channel* chan : { &m_chA, &m_chB, &m_chC })
    {
        chan->sampleTikCounter = -1;
        chan->ornamentPointer = header->ornamentsPointer + 1;
    }

    memset(&m_regs, 0, sizeof(m_regs));
}

bool DecodeSTC::Play()
{
    bool isNewLoop = false;
    Header* header = (Header*)m_data;
    uint8_t mixer  = 0;

    if (--m_delayCounter == 0)
    {
        if (--m_chA.noteSkipCounter < 0)
        {
            if (m_data[m_chA.addressInPattern] == 255)
            {
                if (m_currentPosition == m_data[header->positionsPointer])
                {
                    m_currentPosition = 0;
                    isNewLoop = true;
                }
                else
                    m_currentPosition++;

                m_transposition = m_data[header->positionsPointer + 2 + m_currentPosition * 2];

                uint16_t patternPointer = header->patternsPointer;
                while (m_data[patternPointer] != m_data[header->positionsPointer + 1 + m_currentPosition * 2]) patternPointer += 7;
                m_chA.addressInPattern = *(uint16_t*)(&m_data[patternPointer + 1]);
                m_chB.addressInPattern = *(uint16_t*)(&m_data[patternPointer + 3]);
                m_chC.addressInPattern = *(uint16_t*)(&m_data[patternPointer + 5]);
            }
            PatternInterpreter(m_chA);
        }

        if (--m_chB.noteSkipCounter < 0) PatternInterpreter(m_chB);
        if (--m_chC.noteSkipCounter < 0) PatternInterpreter(m_chC);

        m_delayCounter = header->delay;
    }

    GetRegisters(m_chA, mixer);
    GetRegisters(m_chB, mixer);
    GetRegisters(m_chC, mixer);

    m_regs[0][Mixer] = mixer;
    m_regs[0][A_Fine] = m_chA.ton & 0xff;
    m_regs[0][A_Coarse] = (m_chA.ton >> 8) & 0xf;
    m_regs[0][B_Fine] = m_chB.ton & 0xff;
    m_regs[0][B_Coarse] = (m_chB.ton >> 8) & 0xf;
    m_regs[0][C_Fine] = m_chC.ton & 0xff;
    m_regs[0][C_Coarse] = (m_chC.ton >> 8) & 0xf;
    m_regs[0][A_Volume] = m_chA.amplitude;
    m_regs[0][B_Volume] = m_chB.amplitude;
    m_regs[0][C_Volume] = m_chC.amplitude;
    return isNewLoop;
}

////////////////////////////////////////////////////////////////////////////////

void DecodeSTC::PatternInterpreter(Channel& chan)
{
    Header* header = (Header*)m_data;

    uint16_t k;
    while (true)
    {
        uint8_t val = m_data[chan.addressInPattern];
        if (val <= 0x5f)
        {
            chan.note = val;
            chan.sampleTikCounter = 32;
            chan.positionInSample = 0;
            chan.addressInPattern++;
            break;
        }
        else if (val >= 0x60 && val <= 0x6f)
        {
            k = 0;
            while (m_data[0x1b + 0x63 * k] != (val - 0x60))
                k++;
            chan.samplePointer = 0x1c + 0x63 * k;
        }
        else if (val >= 0x70 && val <= 0x7f)
        {
            k = 0;
            while (m_data[header->ornamentsPointer + 0x21 * k] != (val - 0x70)) k++;
            chan.ornamentPointer = header->ornamentsPointer + 0x21 * k + 1;

            chan.envelopeEnabled = false;
        }
        else if (val == 0x80)
        {
            chan.sampleTikCounter = -1;
            chan.addressInPattern++;
            break;
        }
        else if (val == 0x81)
        {
            chan.addressInPattern++;
            break;
        }
        else if (val == 0x82)
        {
            k = 0;
            while (m_data[header->ornamentsPointer + 0x21 * k] != 0) k++;
            chan.ornamentPointer = header->ornamentsPointer + 0x21 * k + 1;

            chan.envelopeEnabled = false;
        }
        else if (val >= 0x83 && val <= 0x8e)
        {
            m_regs[0][E_Shape] = val - 0x80;
            m_regs[0][E_Fine] = m_data[++chan.addressInPattern];
            chan.envelopeEnabled = true;

            k = 0;
            while (m_data[header->ornamentsPointer + 0x21 * k] != 0) k++;
            chan.ornamentPointer = header->ornamentsPointer + 0x21 * k + 1;
        }
        else
        {
            chan.numberOfNotesToSkip = val - 0xa1;
        }
        chan.addressInPattern++;
    }
    chan.noteSkipCounter = chan.numberOfNotesToSkip;
}

void DecodeSTC::GetRegisters(Channel& chan, uint8_t& mixer)
{
    Header* header = (Header*)m_data;

    if (chan.sampleTikCounter >= 0)
    {
        chan.sampleTikCounter--;
        chan.positionInSample = (chan.positionInSample + 1) & 0x1f;
        if (chan.sampleTikCounter == 0)
        {
            if (m_data[chan.samplePointer + 0x60] != 0)
            {
                chan.positionInSample = m_data[chan.samplePointer + 0x60] & 0x1f;
                chan.sampleTikCounter = m_data[chan.samplePointer + 0x61] + 1;
            }
            else
                chan.sampleTikCounter = -1;
        }
    }
    if (chan.sampleTikCounter >= 0)
    {
        uint16_t samplePointer = ((chan.positionInSample - 1) & 0x1f) * 3 + chan.samplePointer;
        if ((m_data[samplePointer + 1] & 0x80) != 0)
            mixer |= 64;
        else
            m_regs[0][N_Period] = m_data[samplePointer + 1] & 0x1f;

        if ((m_data[samplePointer + 1] & 0x40) != 0)
            mixer |= 8;
        chan.amplitude = m_data[samplePointer] & 15;

        uint8_t note = chan.note + m_data[chan.ornamentPointer + ((chan.positionInSample - 1) & 0x1f)] + m_transposition;
        if (note > 95) note = 95;

        if ((m_data[samplePointer + 1] & 0x20) != 0)
            chan.ton = (STCNoteTable[note] + m_data[samplePointer + 2] + (((uint16_t)(m_data[samplePointer] & 0xf0)) << 4)) & 0xFFF;
        else
            chan.ton = (STCNoteTable[note] - m_data[samplePointer + 2] - (((uint16_t)(m_data[samplePointer] & 0xf0)) << 4)) & 0xFFF;
        if (chan.envelopeEnabled)
            chan.amplitude = chan.amplitude | 16;
    }
    else
        chan.amplitude = 0;

    mixer >>= 1;
}
