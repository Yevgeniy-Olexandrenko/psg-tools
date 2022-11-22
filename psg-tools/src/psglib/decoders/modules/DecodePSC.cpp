#include "DecodePSC.h"
#include "SharedTables.h"

bool DecodePSC::Open(Stream& stream)
{
    bool isDetected = false;
    if (CheckFileExt(stream, { "psc" }))
    {
        std::ifstream fileStream;
        fileStream.open(stream.file, std::fstream::binary);

        if (fileStream)
        {
            fileStream.seekg(0, fileStream.end);
            auto fileSize = int(fileStream.tellg());

            if (fileSize >= 0x4C + 0x02)
            {
                Header header;
                fileStream.seekg(0, fileStream.beg);
                fileStream.read((char*)(&header), std::min((int)sizeof(Header), fileSize));

                bool isHeaderOk = true;
                isHeaderOk &= (header.ornamentsPointer < fileSize);
            //  isHeaderOk &= (header.PSC_OrnamentsPointer >= 0x4C + 0x02);
            //  isHeaderOk &= (header.PSC_OrnamentsPointer <= 0x4C + 0x40);
                isHeaderOk &= (header.ornamentsPointer % 2 == 0);
            //  isHeaderOk &= (header.PSC_SamplesPointers[0] + 0x4C + 0x05 <= fileSize);
                isHeaderOk &= (header.patternsPointer + 11 < fileSize);

                if (isHeaderOk)
                {
                    m_data = new uint8_t[fileSize];
                    fileStream.seekg(0, fileStream.beg);
                    fileStream.read((char*)m_data, fileSize);

                    Init();
                    isDetected = true;

                    std::string ver("1.00"); ver[3] += m_version;
                    stream.info.title(ReadString(&m_data[0x19], 20));
                    stream.info.artist(ReadString(&m_data[0x31], 20));
                    stream.info.type("Pro Sound Creator v" + ver + " module");
                }
            }
            fileStream.close();
        }
    }
    return isDetected;
}

void DecodePSC::Init()
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);

    uint8_t ver = hdr.musicName[0x08];
    m_version = ('0' <= ver && ver <= '9') ? ver - '0' : 7;

    m_delay = hdr.delay;
    m_delayCounter = 1;
    m_positionsPtr = hdr.patternsPointer;
    m_linesCounter = 1;
    m_noiseBase = 0;

    for (int c = 0; c < 3; ++c)
    {
        auto& cha = m_channels[c];

        cha.samplePtr = hdr.samplesPointers[0];
        if (m_version > 3) cha.samplePtr += 0x4C;
        cha.ornamentPtr = (uint16_t&)(m_data[hdr.ornamentsPointer]);
        if (m_version > 3) cha.ornamentPtr += hdr.ornamentsPointer;

        cha.breakOrnamentLoop = false;
        cha.ornamentEnabled = false;
        cha.enabled = false;
        cha.breakSampleLoop = false;
        cha.toneSlideEnabled = false;
        cha.noteSkipCounter = 1;
    }
    memset(&m_regs, 0, sizeof(m_regs));
}

void DecodePSC::Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition)
{
}

bool DecodePSC::Play()
{
    uint8_t* regs = m_regs[0];

    bool loop = false;
    if (!--m_delayCounter)
    {
        if (!--m_linesCounter)
        {
            if (m_data[m_positionsPtr + 1] == 0xFF)
            {
                m_positionsPtr = (uint16_t&)(m_data[m_positionsPtr + 2]);
                loop = true;
            }
            m_linesCounter = m_data[m_positionsPtr + 1];

            m_channels[0].patternPtr = (uint16_t&)(m_data[m_positionsPtr + 2]);
            m_channels[1].patternPtr = (uint16_t&)(m_data[m_positionsPtr + 4]);
            m_channels[2].patternPtr = (uint16_t&)(m_data[m_positionsPtr + 6]);
            m_positionsPtr += 8;

            m_channels[0].noteSkipCounter = 1;
            m_channels[1].noteSkipCounter = 1;
            m_channels[2].noteSkipCounter = 1;
        }

        if (!--m_channels[0].noteSkipCounter) ProcessPattern(0, (uint16_t&)regs[A_Fine], regs[E_Fine], regs[E_Coarse], regs[E_Shape]);
        if (!--m_channels[1].noteSkipCounter) ProcessPattern(1, (uint16_t&)regs[B_Fine], regs[E_Fine], regs[E_Coarse], regs[E_Shape]);
        if (!--m_channels[2].noteSkipCounter) ProcessPattern(2, (uint16_t&)regs[C_Fine], regs[E_Fine], regs[E_Coarse], regs[E_Shape]);

        m_channels[0].Noise_Accumulator += m_noiseBase;
        m_channels[1].Noise_Accumulator += m_noiseBase;
        m_channels[2].Noise_Accumulator += m_noiseBase;

        m_delayCounter = m_delay;
    }

    regs[Mixer] = 0;
    ProcessInstrument(0, (uint16_t&)regs[A_Fine], regs[N_Period], regs[Mixer], regs[A_Volume], (uint16_t&)regs[E_Fine]);
    ProcessInstrument(1, (uint16_t&)regs[B_Fine], regs[N_Period], regs[Mixer], regs[B_Volume], (uint16_t&)regs[E_Fine]);
    ProcessInstrument(2, (uint16_t&)regs[C_Fine], regs[N_Period], regs[Mixer], regs[C_Volume], (uint16_t&)regs[E_Fine]);
    return loop;
}

void DecodePSC::ProcessPattern(int c, uint16_t& tperiod, uint8_t& efine, uint8_t& ecoarse, uint8_t& shape)
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);
    auto& cha = m_channels[c];

    bool quit;
    bool b1b, b2b, b3b, b4b, b5b, b6b, b7b;
    quit = b1b = b2b = b3b = b4b = b5b = b6b = b7b = false;
    do
    {
        uint8_t val = m_data[cha.patternPtr];
        if (val >= 0xc0)
        {
            cha.noteSkipCounter = (val - 0xbf);
            quit = true;
        }
        else if (val >= 0xa0 && val <= 0xbf)
        {
            int o = (val - 0xa0);
            cha.ornamentPtr = (uint16_t&)(m_data[hdr.ornamentsPointer + o * 2]);
            if (m_version > 3) cha.ornamentPtr += hdr.ornamentsPointer;
        }
        else if (val >= 0x7e && val <= 0x9f)
        {
            if (val >= 0x80)
            {
                int s = (val - 0x80);
                cha.samplePtr = hdr.samplesPointers[s];
                if (m_version > 3) cha.samplePtr += 0x4C;
            }
        }
        else if (val == 0x6b)
        {
            cha.additionToTone = m_data[++cha.patternPtr];
            b5b = true;
        }
        else if (val == 0x6c)
        {
            cha.additionToTone = -(int8_t)(m_data[++cha.patternPtr]);
            b5b = true;
        }
        else if (val == 0x6d)
        {
            b4b = true;
            cha.additionToTone = m_data[++cha.patternPtr];
        }
        else if (val == 0x6e)
        {
            m_delay = m_data[++cha.patternPtr];
        }
        else if (val == 0x6f)
        {
            b1b = true;
            cha.patternPtr++;
        }
        else if (val == 0x70)
        {
            b3b = true;
            cha.Volume_Counter1 = m_data[++cha.patternPtr];
        }
        else if (val == 0x71)
        {
            cha.breakOrnamentLoop = true;
            cha.patternPtr++;
        }
        else if (val == 0x7a)
        {
            cha.patternPtr++;
            if (c == 1)
            {
                shape   = (m_data[cha.patternPtr] & 0x0F);
                efine   = m_data[++cha.patternPtr];
                ecoarse = m_data[++cha.patternPtr];
            }
        }
        else if (val == 0x7b)
        {
            cha.patternPtr++;
            if (c == 1)
                m_noiseBase = m_data[cha.patternPtr];
        }
        else if (val == 0x7c)
        {
            b1b = b3b = b4b = b5b = b6b = b7b = false;
            b2b = true;
        }
        else if (val == 0x7d)
        {
            cha.breakSampleLoop = true;
        }
        else if (val >= 0x58 && val <= 0x66)
        {
            cha.Initial_Volume = (val - 0x57);
            cha.envelopeEnabled = false;
            b6b = true;
        }
        else if (val == 0x57)
        {
            cha.Initial_Volume = 0xf;
            cha.envelopeEnabled = true;
            b6b = true;
        }
        else if (val <= 0x56)
        {
            cha.note = val;
            b6b = true;
            b7b = true;
        }
        else
        {
            cha.patternPtr++;
        }
        cha.patternPtr++;
    } while (!quit);

    if (b7b)
    {
        cha.breakOrnamentLoop = false;
        cha.ornamentEnabled = true;
        cha.enabled = true;
        cha.breakSampleLoop = false;
        cha.toneSlideEnabled = false;
        cha.toneAcc = 0;
        cha.toneSliding = 0;
        cha.Noise_Accumulator = 0;
        cha.Volume_Counter = 0;
        cha.samplePos = 0;
        cha.ornamentPos = 0;
    }
    if (b6b)
    {
        cha.volume = cha.Initial_Volume;
    }
    if (b5b)
    {
        cha.glissade = false;
        cha.toneSlideEnabled = true;
    }
    if (b4b)
    {
        cha.toneSliding = (tperiod - NoteTable_ASM[cha.note]);
        cha.glissade = true;
        if (cha.toneSliding >= 0)
            cha.additionToTone = -cha.additionToTone;
        cha.toneSlideEnabled = true;
    }
    if (b3b)
    {
        cha.Volume_Counter = cha.Volume_Counter1;
        cha.volumeInc = true;
        if ((cha.Volume_Counter & 0x40) != 0)
        {
            cha.Volume_Counter = -(int8_t)(cha.Volume_Counter | 128);
            cha.volumeInc = false;
        }
        cha.Volume_Counter_Init = cha.Volume_Counter;
    }
    if (b2b)
    {
        cha.breakOrnamentLoop = false;
        cha.ornamentEnabled = false;
        cha.enabled = false;
        cha.breakSampleLoop = false;
        cha.toneSlideEnabled = false;
    }
    if (b1b)
        cha.ornamentEnabled = false;
}

void DecodePSC::ProcessInstrument(int c, uint16_t& tperiod, uint8_t& noise, uint8_t& mixer, uint8_t& volume, uint16_t& eperiod)
{
    uint8_t j, b;

    auto& cha = m_channels[c];
    if (cha.enabled)
    {
        j = cha.note;
        if (cha.ornamentEnabled)
        {
            b = m_data[cha.ornamentPtr + cha.ornamentPos * 2];
            cha.Noise_Accumulator += b;
            j += m_data[cha.ornamentPtr + cha.ornamentPos * 2 + 1];
            if ((int8_t)j < 0)
                j += 0x56;
            if (j > 0x55)
                j -= 0x56;
            if (j > 0x55)
                j = 0x55;
            if ((b & 128) == 0)
                cha.ornamentLoop = cha.ornamentPos;
            if ((b & 64) == 0)
            {
                if (!cha.breakOrnamentLoop)
                    cha.ornamentPos = cha.ornamentLoop;
                else
                {
                    cha.breakOrnamentLoop = false;
                    if ((b & 32) == 0)
                        cha.ornamentEnabled = false;
                    cha.ornamentPos++;
                }
            }
            else
            {
                if ((b & 32) == 0)
                    cha.ornamentEnabled = false;
                cha.ornamentPos++;
            }
        }
        cha.note = j;
        tperiod = *(uint16_t*)(&m_data[cha.samplePtr + cha.samplePos * 6]);
        cha.toneAcc += tperiod;
        tperiod = NoteTable_ASM[j] + cha.toneAcc;
        if (cha.toneSlideEnabled)
        {
            cha.toneSliding += cha.additionToTone;
            if (cha.glissade && (((cha.toneSliding < 0) && (cha.additionToTone <= 0)) || ((cha.toneSliding >= 0) && (cha.additionToTone >= 0))))
                cha.toneSlideEnabled = false;
            tperiod += cha.toneSliding;
        }
        tperiod &= 0x0FFF;

        b = m_data[cha.samplePtr + cha.samplePos * 6 + 4];
        mixer |= ((b & 9) << 3);
        j = 0;
        if ((b & 2) != 0) j++;
        if ((b & 4) != 0) j--;
        if (cha.Volume_Counter > 0)
        {
            cha.Volume_Counter--;
            if (cha.Volume_Counter == 0)
            {
                if (cha.volumeInc) j++; else j--;
                cha.Volume_Counter = cha.Volume_Counter_Init;
            }
        }
        cha.volume += j;
        if ((int8_t)cha.volume < 0)
            cha.volume = 0;
        else if (cha.volume > 15)
            cha.volume = 15;
        volume = ((cha.volume + 1) * (m_data[cha.samplePtr + cha.samplePos * 6 + 3] & 15)) >> 4;
        if (cha.envelopeEnabled && ((b & 16) == 0)) volume |= 16;

        if (((volume & 16) != 0) && ((b & 8) != 0))
        {
            eperiod += (int8_t)(m_data[cha.samplePtr + cha.samplePos * 6 + 2]);
        }
        else
        {
            cha.Noise_Accumulator += m_data[cha.samplePtr + cha.samplePos * 6 + 2];
            if ((b & 8) == 0)
                noise = (cha.Noise_Accumulator & 31);
        }
        if ((b & 128) == 0)
            cha.sampleLoop = cha.samplePos;
        if ((b & 64) == 0)
        {
            if (!cha.breakSampleLoop)
                cha.samplePos = cha.sampleLoop;
            else
            {
                cha.breakSampleLoop = false;
                if ((b & 32) == 0)
                    cha.enabled = false;
                cha.samplePos++;
            }
        }
        else
        {
            if ((b & 32) == 0)
                cha.enabled = false;
            cha.samplePos++;
        }
    }
    else volume = 0;
    mixer >>= 1;
}
