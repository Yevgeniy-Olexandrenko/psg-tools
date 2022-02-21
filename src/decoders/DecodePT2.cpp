#include <fstream>
#include "DecodePT2.h"
#include "module/Module.h"

bool DecodePT2::Open(Module& module)
{
    bool isDetected = false;
    std::ifstream fileStream;
    fileStream.open(module.file.dirNameExt(), std::fstream::binary);

    if (fileStream)
    {
        fileStream.seekg(0, fileStream.end);
        uint32_t fileSize = (uint32_t)fileStream.tellg();

        if (fileSize > 131)
        {
            uint8_t data[131 + 1];
            fileStream.seekg(0, fileStream.beg);
            fileStream.read((char*)data, sizeof(data));
            Header* header = (Header*)data;
            data[131] = 0; // end of music name

            if (header->delay >= 3 
                && header->numberOfPositions > 0 
                && (header->samplesPointers[0] | header->samplesPointers[1] << 8) == 0
                && (header->patternsPointerL | header->patternsPointerH << 8) < fileSize)
            {
                m_data = new uint8_t[fileSize];
                fileStream.seekg(0, fileStream.beg);
                fileStream.read((char*)m_data, fileSize);

                if (fileStream && Init())
                {
                    std::string musicName(header->musicName);
                    module.info.title(musicName);
                    module.info.type("ProTracker 2.x module");
                    module.playback.frameRate(50);

                    m_loop = m_tick = 0;
                    isDetected = true;
                }
            }
        }
        fileStream.close();
    }
	return isDetected;
}

bool DecodePT2::Decode(Frame& frame)
{
    // stop decoding on new loop
    if (Step()) return false;

    for (uint8_t r = 0; r < 16; ++r)
    {
        uint8_t data = m_regs[r];
        if (r == Env_Shape)
        {
            if (data != 0xFF)
                frame[r].first.override(data);
        }
        else
        {
            frame[r].first.update(data);
        }
    }
    return true;
}

void DecodePT2::Close(Module& module)
{
    if (m_loop > 0)
        module.loop.frameId(m_loop);

    delete[] m_data;
}

////////////////////////////////////////////////////////////////////////////////

namespace
{
    const uint16_t PT2NoteTable[96] =
    {
        0x0ef8, 0x0e10, 0x0d60, 0x0c80, 0x0bd8, 0x0b28, 0x0a88, 0x09f0,
        0x0960, 0x08e0, 0x0858, 0x07e0, 0x077c, 0x0708, 0x06b0, 0x0640,
        0x05ec, 0x0594, 0x0544, 0x04f8, 0x04b0, 0x0470, 0x042c, 0x03fd,
        0x03be, 0x0384, 0x0358, 0x0320, 0x02f6, 0x02ca, 0x02a2, 0x027c,
        0x0258, 0x0238, 0x0216, 0x01f8, 0x01df, 0x01c2, 0x01ac, 0x0190,
        0x017b, 0x0165, 0x0151, 0x013e, 0x012c, 0x011c, 0x010a, 0x00fc,
        0x00ef, 0x00e1, 0x00d6, 0x00c8, 0x00bd, 0x00b2, 0x00a8, 0x009f,
        0x0096, 0x008e, 0x0085, 0x007e, 0x0077, 0x0070, 0x006b, 0x0064,
        0x005e, 0x0059, 0x0054, 0x004f, 0x004b, 0x0047, 0x0042, 0x003f,
        0x003b, 0x0038, 0x0035, 0x0032, 0x002f, 0x002c, 0x002a, 0x0027,
        0x0025, 0x0023, 0x0021, 0x001f, 0x001d, 0x001c, 0x001a, 0x0019,
        0x0017, 0x0016, 0x0015, 0x0013, 0x0012, 0x0011, 0x0010, 0x000f,
    };

    uint16_t ReadWord(uint8_t* ptr)
    {
        return *ptr | *(ptr + 1) << 8;
    }
}

bool DecodePT2::Init()
{
    Header* header = (Header*)m_data;

    memset(&m_chA, 0, sizeof(Channel));
    memset(&m_chB, 0, sizeof(Channel));
    memset(&m_chC, 0, sizeof(Channel));

    m_delay = header->delay;
    m_delayCounter = 1;
    m_currentPosition = 0;

    uint16_t patternsPointer = header->patternsPointerL | header->patternsPointerH << 8;
    patternsPointer += header->positionList[0] * 6;
    m_chA.addressInPattern = ReadWord(&m_data[patternsPointer + 0]);
    m_chB.addressInPattern = ReadWord(&m_data[patternsPointer + 2]);
    m_chC.addressInPattern = ReadWord(&m_data[patternsPointer + 4]);

    for (Channel* chan : { &m_chA, &m_chB, &m_chC })
    {
        chan->ornamentPointer = ReadWord(&header->ornamentsPointers[0 * 2]);
        chan->ornamentLength = m_data[chan->ornamentPointer++];
        chan->loopOrnamentPosition = m_data[chan->ornamentPointer++];
        chan->volume = 15;
    }

    memset(&m_regs, 0, sizeof(m_regs));
	return true;
}

bool DecodePT2::Step()
{
    bool isNewLoop = Play();
    Header* header = (Header*)m_data;

    if (m_loop == 0)
    {
        uint8_t currPosition = m_currentPosition;
        uint8_t loopPosition = header->loopPosition;
        uint8_t lastPosition = header->numberOfPositions - 1;

        // detect true loop frame (ommit loop to first or last position)
        if (loopPosition > 0 && loopPosition < lastPosition && currPosition == loopPosition)
        {
            m_loop = m_tick;
        }
    }

    m_tick++;
    return isNewLoop;
}

void DecodePT2::PatternInterpreter(Channel& chan)
{
    Header* header = (Header*)m_data;
    bool quit = false;
    bool gliss = false;

    do
    {
        uint8_t val = m_data[chan.addressInPattern];
        if (val >= 0xe1)
        {
            chan.samplePointer = ReadWord(&header->samplesPointers[(val - 0xe0) * 2]);
            chan.sampleLength = m_data[chan.samplePointer++];
            chan.loopSamplePosition = m_data[chan.samplePointer++];
        }
        else if (val == 0xe0)
        {
            chan.positionInSample = 0;
            chan.positionInOrnament = 0;
            chan.currentTonSliding = 0;
            chan.glisType = 0;
            chan.enabled = false;
            quit = true;
        }
        else if (val >= 0x80 && val <= 0xdf)
        {
            chan.positionInSample = 0;
            chan.positionInOrnament = 0;
            chan.currentTonSliding = 0;
            if (gliss)
            {
                chan.slideToNote = val - 0x80;
                if (chan.glisType == 1)
                    chan.note = chan.slideToNote;
            }
            else
            {
                chan.note = val - 0x80;
                chan.glisType = 0;
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
            m_regs[Env_Shape] = val - 0x70;
            m_regs[Env_PeriodL] = m_data[++chan.addressInPattern];
            m_regs[Env_PeriodH] = m_data[++chan.addressInPattern];
        }
        else if (val == 0x70)
        {
            quit = true;
        }
        else if (val >= 0x60 && val <= 0x6f)
        {
            chan.ornamentPointer = ReadWord(&header->ornamentsPointers[(val - 0x60) * 2]);
            chan.ornamentLength = m_data[chan.ornamentPointer++];
            chan.loopOrnamentPosition = m_data[chan.ornamentPointer++];
            chan.positionInOrnament = 0;
        }
        else if (val >= 0x20 && val <= 0x5f)
        {
            chan.numberOfNotesToSkip = val - 0x20;
        }
        else if (val >= 0x10 && val <= 0x1f)
        {
            chan.volume = val - 0x10;
        }
        else if (val == 0xf)
        {
            m_delay = m_data[++chan.addressInPattern];
        }
        else if (val == 0xe)
        {
            chan.glissade = m_data[++chan.addressInPattern];
            chan.glisType = 1;
            gliss = true;
        }
        else if (val == 0xd)
        {
            chan.glissade = std::abs((int8_t)(m_data[++chan.addressInPattern]));

            // Do not use precalculated Ton_Delta to
            // avoide error with first note of pattern
            chan.addressInPattern += 2; 
            
            chan.glisType = 2;
            gliss = true;
        }
        else if (val == 0xc)
        {
            chan.glisType = 0;
        }
        else
        {
            chan.additionToNoise = m_data[++chan.addressInPattern];
        }
        chan.addressInPattern++;
    } while (!quit);

    if (gliss && (chan.glisType == 2))
    {
        chan.tonDelta = std::abs(PT2NoteTable[chan.slideToNote] - PT2NoteTable[chan.note]);
        if (chan.slideToNote > chan.note)
            chan.glissade = -chan.glissade;
    }
    chan.noteSkipCounter = chan.numberOfNotesToSkip;
}

void DecodePT2::GetRegisters(Channel& chan, uint8_t& mixer)
{
    uint8_t note, b0, b1;
    Header* header = (Header*)m_data;

    if (chan.enabled)
    {
        uint16_t samplePointer = chan.samplePointer + chan.positionInSample * 3;
        b0 = m_data[samplePointer + 0];
        b1 = m_data[samplePointer + 1];
        chan.ton = m_data[samplePointer + 2] + (uint16_t)((b1 & 15) << 8);
        if ((b0 & 4) == 0)
            chan.ton = -chan.ton;

        note = chan.note + m_data[chan.ornamentPointer + chan.positionInOrnament];
        if (note > 95) note = 95;
        chan.ton = (chan.ton + chan.currentTonSliding + PT2NoteTable[note]) & 0xfff;

        if (chan.glisType == 2)
        {
            chan.tonDelta = chan.tonDelta - std::abs(chan.glissade);
            if (chan.tonDelta < 0)
            {
                chan.note = chan.slideToNote;
                chan.glisType = 0;
                chan.currentTonSliding = 0;
            }
        }
        if (chan.glisType != 0)
            chan.currentTonSliding += chan.glissade;

        chan.amplitude = (chan.volume * 17 + (uint8_t)(chan.volume > 7)) * (b1 >> 4) / 256;
        if (chan.envelopeEnabled) chan.amplitude |= 16;

        if ((b0 & 1) != 0)
            mixer |= 64;
        else
            m_regs[Noise_Period] = ((b0 >> 3) + chan.additionToNoise) & 0x1f;

        if ((b0 & 2) != 0)
            mixer |= 8;

        if (++chan.positionInSample == chan.sampleLength)
            chan.positionInSample = chan.loopSamplePosition;

        if (++chan.positionInOrnament == chan.ornamentLength)
            chan.positionInOrnament = chan.loopOrnamentPosition;
    }
    else
    {
        chan.amplitude = 0;
    }
    mixer >>= 1;
}

bool DecodePT2::Play()
{
    bool isNewLoop = false;
    m_regs[Env_Shape] = 0xFF;

    uint8_t mixer = 0;
    Header* header = (Header*)m_data;

    if (--m_delayCounter == 0)
    {
        if (--m_chA.noteSkipCounter < 0)
        {
            if (m_data[m_chA.addressInPattern] == 0)
            {
                if (++m_currentPosition == header->numberOfPositions)
                {
                    m_currentPosition = header->loopPosition;
                    isNewLoop = true;
                }

                uint16_t patternsPointer = header->patternsPointerL | header->patternsPointerH << 8;
                patternsPointer += header->positionList[m_currentPosition] * 6;
                m_chA.addressInPattern = ReadWord(&m_data[patternsPointer + 0]);
                m_chB.addressInPattern = ReadWord(&m_data[patternsPointer + 2]);
                m_chC.addressInPattern = ReadWord(&m_data[patternsPointer + 4]);
            }
            PatternInterpreter(m_chA);
        }

        if (--m_chB.noteSkipCounter < 0) PatternInterpreter(m_chB);
        if (--m_chC.noteSkipCounter < 0) PatternInterpreter(m_chC);
        m_delayCounter = m_delay;
    }

    GetRegisters(m_chA, mixer);
    GetRegisters(m_chB, mixer);
    GetRegisters(m_chC, mixer);

    m_regs[Mixer_Flags] = mixer;
    m_regs[TonA_PeriodL] = m_chA.ton & 0xff;
    m_regs[TonA_PeriodH] = (m_chA.ton >> 8) & 0xf;
    m_regs[TonB_PeriodL] = m_chB.ton & 0xff;
    m_regs[TonB_PeriodH] = (m_chB.ton >> 8) & 0xf;
    m_regs[TonC_PeriodL] = m_chC.ton & 0xff;
    m_regs[TonC_PeriodH] = (m_chC.ton >> 8) & 0xf;
    m_regs[VolA_EnvFlg] = m_chA.amplitude;
    m_regs[VolB_EnvFlg] = m_chB.amplitude;
    m_regs[VolC_EnvFlg] = m_chC.amplitude;
    return isNewLoop;
}
