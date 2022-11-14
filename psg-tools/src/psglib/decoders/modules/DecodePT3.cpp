#include "DecodePT3.h"
#include "stream/Stream.h"
#include "SharedTables.h"

const std::string PTSignature = "ProTracker 3.";
const std::string VTSignature = "Vortex Tracker II";

////////////////////////////////////////////////////////////////////////////////

bool DecodePT3::Open(Stream& stream)
{
    bool isDetected = false;
    if (CheckFileExt(stream, { "pt3", "ts" }))
    {
        std::ifstream fileStream;
        fileStream.open(stream.file, std::fstream::binary);

        if (fileStream)
        {
            uint8_t signature[30];
            fileStream.read((char*)signature, 30);

            bool isPT = !memcmp(signature, PTSignature.data(), PTSignature.size());
            bool isVT = !memcmp(signature, VTSignature.data(), VTSignature.size());

            if (isPT || isVT)
            {
                fileStream.seekg(0, fileStream.end);
                m_size = (int)fileStream.tellg();

                m_data = new uint8_t[m_size];
                fileStream.seekg(0, fileStream.beg);
                fileStream.read((char*)m_data, m_size);

                Init();
                isDetected = true;

                stream.info.title(ReadString(&m_data[0x1E], 32));
                stream.info.artist(ReadString(&m_data[0x42], 32));
                stream.info.type(ReadString(&m_data[0x00], isVT ? 21 : 14) + " module");

                if (m_module[0].m_hdr->tonTableId == 1)
                {
                    stream.schip.clock(Chip::Clock::F1773400);
                }
                else if (m_module[0].m_hdr->tonTableId == 2 && m_version > 3)
                {
                    stream.schip.clock(Chip::Clock::F1750000);
                }

                if (m_isTS)
                {
                    stream.schip.second.model(stream.schip.first.model());
                }
            }
            fileStream.close();
        }
    }
	return isDetected;
}

////////////////////////////////////////////////////////////////////////////////

void DecodePT3::Init()
{
    memset(&m_module, 0, sizeof(m_module));
    memset(&m_regs, 0, sizeof(m_regs));

    m_module[0].m_data = m_data;
    m_module[1].m_data = m_data;
    m_module[0].m_hdr  = (Header*)m_module[0].m_data;
    m_module[1].m_hdr  = (Header*)m_module[1].m_data;
    
    uint8_t ver = m_module[0].m_hdr->musicName[13];
    m_version = ('0' <= ver && ver <= '9') ? ver - '0' : 6;

    m_module[0].ts = m_module[1].ts = 0x20;
    int TS = m_module[0].m_hdr->musicName[98];
    m_isTS = (TS != 0x20);
    if (m_isTS) m_module[1].ts = TS;
    else if (m_size > 400 && !memcmp(m_data + m_size - 4, "02TS", 4)) 
    { 
        // try load Vortex II '02TS'
        uint16_t sz1 = m_data[m_size - 12] + 0x100 * m_data[m_size - 11];
        uint16_t sz2 = m_data[m_size - 6 ] + 0x100 * m_data[m_size - 5 ];
        if (uint32_t(sz1 + sz2) < uint32_t(m_size) && sz1 > 200 && sz2 > 200)
        {
            m_isTS = true;
            m_module[1].m_data = (m_data + sz1);
            m_module[1].m_hdr = (Header*)m_module[1].m_data;
        }
    }

    for (auto& mod : m_module) 
    {
        mod.m_delay = mod.m_hdr->delay;
        mod.m_delayCounter = 1;

        uint8_t pat = mod.m_hdr->positionList[0];
        if (mod.ts != 0x20) pat = (uint8_t)(3 * mod.ts - 3 - pat);

        for (int c = 0; c < 3; ++c) 
        {
            auto& cha = mod.m_channels[c];
            cha.patternPtr =
                mod.m_data[mod.m_hdr->patternsPointer + 2 * (pat + c) + 0] +
                mod.m_data[mod.m_hdr->patternsPointer + 2 * (pat + c) + 1] * 0x100;
            cha.ornamentPtr = mod.m_hdr->ornamentsPointers[0];
            cha.ornamentLoop = mod.m_data[cha.ornamentPtr++];
            cha.ornamentLen = mod.m_data[cha.ornamentPtr++];
            cha.samplePtr = mod.m_hdr->samplesPointers[1];
            cha.sampleLoop = mod.m_data[cha.samplePtr++];
            cha.sampleLen = mod.m_data[cha.samplePtr++];
            cha.volume = 0xF;
            cha.noteSkipCounter = 1;
        }
    }
}

void DecodePT3::Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition)
{
    currPosition = m_module[0].m_currentPosition;
    loopPosition = m_module[0].m_hdr->loopPosition;
    lastPosition = m_module[0].m_hdr->numberOfPositions - 1;
}

bool DecodePT3::Play()
{
    bool loop = PlayModule(0);
    if (m_isTS) PlayModule(1);
    return loop;
}

////////////////////////////////////////////////////////////////////////////////

bool DecodePT3::PlayModule(int m)
{
    auto& mod = m_module[m];
    bool loop = false;
    if (--mod.m_delayCounter == 0) 
    {
        for (int c = 0; c < 3; ++c) 
        {
            Channel& cha = mod.m_channels[c];
            if (!--cha.noteSkipCounter) 
            {
                if (!c && mod.m_data[cha.patternPtr] == 0) 
                {
                    if (++mod.m_currentPosition == mod.m_hdr->numberOfPositions)
                    {
                        mod.m_currentPosition = mod.m_hdr->loopPosition;
                        loop = true;
                    }

                    uint8_t pat = mod.m_hdr->positionList[mod.m_currentPosition];
                    if (mod.ts != 0x20) pat = (uint8_t)(3 * mod.ts - 3 - pat);

                    for (int c = 0; c < 3; c++)
                    {
                        mod.m_channels[c].patternPtr =
                            mod.m_data[mod.m_hdr->patternsPointer + 2 * (pat + c) + 0] +
                            mod.m_data[mod.m_hdr->patternsPointer + 2 * (pat + c) + 1] * 0x100;
                    }
                    mod.m_global.noiseBase = 0;
                }
                ProcessPattern(m, c, m_regs[m][E_Shape]);
            }
        }
        mod.m_delayCounter = mod.m_delay;
    }
    
    int8_t envAdd = 0;
    m_regs[m][Mixer] = 0;
    ProcessInstrument(m, 0, m_regs[m][A_Fine], m_regs[m][A_Coarse], m_regs[m][A_Volume], m_regs[m][Mixer], envAdd);
    ProcessInstrument(m, 1, m_regs[m][B_Fine], m_regs[m][B_Coarse], m_regs[m][B_Volume], m_regs[m][Mixer], envAdd);
    ProcessInstrument(m, 2, m_regs[m][C_Fine], m_regs[m][C_Coarse], m_regs[m][C_Volume], m_regs[m][Mixer], envAdd);

    uint8_t  noise = (mod.m_global.noiseBase + mod.m_global.noiseAdd) & 0x1F;
    uint16_t etone = (mod.m_global.envBaseLo | mod.m_global.envBaseHi << 8) + mod.m_global.curEnvSlide + envAdd;

    m_regs[m][N_Period] = noise;
    m_regs[m][E_Fine  ] = (etone & 0xFF);
    m_regs[m][E_Coarse] = (etone >> 8 & 0xFF);

    if (mod.m_global.curEnvDelay > 0) 
    {
        if (--mod.m_global.curEnvDelay == 0) 
        {
            mod.m_global.curEnvDelay = mod.m_global.envDelay;
            mod.m_global.curEnvSlide += mod.m_global.envSlideAdd;
        }
    }

    return loop;
}

void DecodePT3::ProcessPattern(int m, int c, uint8_t& shape)
{
    auto& mod = m_module[m];
    auto& cha = mod.m_channels[c];

    int PrNote = cha.note;
    int PrSliding = cha.toneSliding;

    uint8_t counter = 0;
    uint8_t f1 = 0, f2 = 0, f3 = 0, f4 = 0, f5 = 0, f8 = 0, f9 = 0;

    while (true) 
    {
        uint8_t byte = mod.m_data[cha.patternPtr];
        if (0xF0 <= byte && byte <= 0xFF)
        {
            int o = (byte - 0xF0);
            cha.ornamentPtr  = mod.m_hdr->ornamentsPointers[o];
            cha.ornamentLoop = mod.m_data[cha.ornamentPtr++];
            cha.ornamentLen  = mod.m_data[cha.ornamentPtr++];

            int s = mod.m_data[++cha.patternPtr] >> 1;
            cha.samplePtr  = mod.m_hdr->samplesPointers[s];
            cha.sampleLoop = mod.m_data[cha.samplePtr++];
            cha.sampleLen  = mod.m_data[cha.samplePtr++];

            cha.envelopeEnabled = false;
            cha.ornamentPos = 0;
        }
        else if (0xD1 <= byte && byte <= 0xEF) 
        {
            int s = (byte - 0xD0);
            cha.samplePtr  = mod.m_hdr->samplesPointers[s];
            cha.sampleLoop = mod.m_data[cha.samplePtr++];
            cha.sampleLen  = mod.m_data[cha.samplePtr++];
        }
        else if (byte == 0xD0) 
        {
            cha.patternPtr++;
            break;
        }
        else if (0xC1 <= byte && byte <= 0xCF) 
        {
            cha.volume = byte - 0xC0;
        }
        else if (byte == 0xC0) 
        {
            cha.samplePos = 0;
            cha.volumeSliding = 0;
            cha.noiseSliding = 0;
            cha.envelopeSliding = 0;
            cha.ornamentPos = 0;
            cha.tonSlideCount = 0;
            cha.toneSliding = 0;
            cha.toneAcc = 0;
            cha.currentOnOff = 0;
            cha.enabled = false;
            cha.patternPtr++;
            break;
        }
        else if (0xB2 <= byte && byte <= 0xBF) 
        {
            cha.envelopeEnabled = true;
            shape = (byte - 0xB1);
            mod.m_global.envBaseHi = mod.m_data[++cha.patternPtr];
            mod.m_global.envBaseLo = mod.m_data[++cha.patternPtr];
            mod.m_global.curEnvSlide = 0;
            mod.m_global.curEnvDelay = 0;
            cha.ornamentPos = 0;
        }
        else if (byte == 0xB1) 
        {
            cha.noteSkip = mod.m_data[++cha.patternPtr];
        }
        else if (byte == 0xB0) 
        {
            cha.envelopeEnabled = false;
            cha.ornamentPos = 0;
        }
        else if (0x50 <= byte && byte <= 0xAF) 
        {
            cha.note = (byte - 0x50);
            cha.samplePos = 0;
            cha.volumeSliding = 0;
            cha.noiseSliding = 0;
            cha.envelopeSliding = 0;
            cha.ornamentPos = 0;
            cha.tonSlideCount = 0;
            cha.toneSliding = 0;
            cha.toneAcc = 0;
            cha.currentOnOff = 0;
            cha.enabled = true;
            cha.patternPtr++;
            break;
        }
        else if (0x40 <= byte && byte <= 0x4F)
        {
            uint8_t o = (byte - 0x40);
            cha.ornamentPtr  = mod.m_hdr->ornamentsPointers[o];
            cha.ornamentLoop = mod.m_data[cha.ornamentPtr++];
            cha.ornamentLen  = mod.m_data[cha.ornamentPtr++];
            cha.ornamentPos  = 0;
        }
        else if (0x20 <= byte && byte <= 0x3F) 
        {
            mod.m_global.noiseBase = byte - 0x20;
        }
        else if (0x10 <= byte && byte <= 0x1F) 
        {
            cha.envelopeEnabled = (byte != 0x10);
            if (cha.envelopeEnabled) 
            {
                shape = (byte - 0x10);
                mod.m_global.envBaseHi = mod.m_data[++cha.patternPtr];
                mod.m_global.envBaseLo = mod.m_data[++cha.patternPtr];
                mod.m_global.curEnvSlide = 0;
                mod.m_global.curEnvDelay = 0;
            }

            int s = mod.m_data[++cha.patternPtr] >> 1;
            cha.samplePtr   = mod.m_hdr->samplesPointers[s];
            cha.sampleLoop  = mod.m_data[cha.samplePtr++];
            cha.sampleLen   = mod.m_data[cha.samplePtr++];
            cha.ornamentPos = 0;
        }
        else if (byte == 0x09) f9 = ++counter;
        else if (byte == 0x08) f8 = ++counter;
        else if (byte == 0x05) f5 = ++counter;
        else if (byte == 0x04) f4 = ++counter;
        else if (byte == 0x03) f3 = ++counter;
        else if (byte == 0x02) f2 = ++counter;
        else if (byte == 0x01) f1 = ++counter;

        cha.patternPtr++;
    }

    while (counter > 0) 
    {
        if (counter == f1) 
        {
            cha.simpleGliss = true;
            cha.tonSlideDelay = mod.m_data[cha.patternPtr++];
            cha.tonSlideCount = cha.tonSlideDelay;
            cha.tonSlideStep = *(int16_t*)(&mod.m_data[cha.patternPtr]);
            cha.patternPtr += 2;
            if (cha.tonSlideCount == 0 && m_version >= 7) cha.tonSlideCount++;
            cha.currentOnOff = 0;
        }
        else if (counter == f2) 
        {
            cha.simpleGliss = false;
            cha.tonSlideDelay = mod.m_data[cha.patternPtr];
            cha.tonSlideCount = cha.tonSlideDelay;
            cha.patternPtr += 3;
            int16_t step = *(int16_t*)(&mod.m_data[cha.patternPtr]);
            cha.tonSlideStep = (step < 0 ? -step : step);
            cha.patternPtr += 2;
            cha.toneDelta = (GetTonePeriod(m, cha.note) - GetTonePeriod(m, PrNote));
            cha.slideToNote = cha.note;
            cha.note = PrNote;
            if (m_version >= 6) cha.toneSliding = PrSliding;
            if (cha.toneDelta - cha.toneSliding < 0) cha.tonSlideStep = -cha.tonSlideStep;
            cha.currentOnOff = 0;
        }
        else if (counter == f3) 
        {
            cha.samplePos = mod.m_data[cha.patternPtr++];
        }
        else if (counter == f4) 
        {
            cha.ornamentPos = mod.m_data[cha.patternPtr++];
        }
        else if (counter == f5) 
        {
            cha.onOffDelay = mod.m_data[cha.patternPtr++];
            cha.offOnDelay = mod.m_data[cha.patternPtr++];
            cha.currentOnOff = cha.onOffDelay;
            cha.tonSlideCount = 0;
            cha.toneSliding = 0;
        }
        else if (counter == f8) 
        {
            mod.m_global.envDelay = mod.m_data[cha.patternPtr++];
            mod.m_global.curEnvDelay = mod.m_global.envDelay;
            mod.m_global.envSlideAdd = *(int16_t*)(&mod.m_data[cha.patternPtr]);
            cha.patternPtr += 2;
        }
        else if (counter == f9) 
        {
            uint8_t delay = mod.m_data[cha.patternPtr++];
            mod.m_delay = delay;
            if (m_isTS && m_module[1].ts != 0x20) 
            {
                m_module[0].m_delay = delay;
                m_module[0].m_delayCounter = delay;
                m_module[1].m_delay = delay;
            }
        }
        counter--;
    }
    cha.noteSkipCounter = cha.noteSkip;
}

void DecodePT3::ProcessInstrument(int m, int c, uint8_t& tfine, uint8_t& tcoarse, uint8_t& volume, uint8_t& mixer, int8_t& envAdd)
{
    auto& mod = m_module[m];
    auto& cha = mod.m_channels[c];

    if (cha.enabled) 
    {
        uint16_t sptr = (cha.samplePtr + 4 * cha.samplePos);
        if (++cha.samplePos >= cha.sampleLen)
            cha.samplePos = cha.sampleLoop;

        uint16_t optr = cha.ornamentPtr + cha.ornamentPos;
        if (++cha.ornamentPos >= cha.ornamentLen)
            cha.ornamentPos = cha.ornamentLoop;

        uint8_t sb0 = mod.m_data[sptr + 0];
        uint8_t sb1 = mod.m_data[sptr + 1];
        uint8_t sb2 = mod.m_data[sptr + 2];
        uint8_t sb3 = mod.m_data[sptr + 3];
        uint8_t ob0 = mod.m_data[optr + 0];

        uint16_t tone = (sb2 | sb3 << 8) + cha.toneAcc;
        if (sb1 & 0x40) cha.toneAcc = tone;

        int8_t note = (cha.note + ob0);
        if (note < 0 ) note = 0;
        if (note > 95) note = 95;

        tone += (cha.toneSliding + GetTonePeriod(m, note));
        tfine = (tone & 0xFF);
        tcoarse = (tone >> 8 & 0x0F);

        if (cha.tonSlideCount > 0)
        {
            if (!--cha.tonSlideCount) 
            {
                cha.toneSliding += cha.tonSlideStep;
                cha.tonSlideCount = cha.tonSlideDelay;
                if (!cha.simpleGliss) 
                {
                    if ((cha.tonSlideStep < 0 && cha.toneSliding <= cha.toneDelta) ||
                        (cha.tonSlideStep >= 0 && cha.toneSliding >= cha.toneDelta))
                    {
                        cha.note = cha.slideToNote;
                        cha.tonSlideCount = 0;
                        cha.toneSliding = 0;
                    }
                }
            }
        }

        if (sb0 & 0b10000000) 
        {
            if (sb0 & 0b01000000)
                { if (cha.volumeSliding < +15) ++cha.volumeSliding; }
            else
                { if (cha.volumeSliding > -15) --cha.volumeSliding; }
        }
        int vol = (sb1 & 0x0F) + cha.volumeSliding;
        if (vol < 0x0) vol = 0x0;
        if (vol > 0xF) vol = 0xF;

        volume = m_version <= 4
            ? VolumeTable_33_34[cha.volume][vol]
            : VolumeTable_35[cha.volume][vol];
        if (!(sb0 & 0b00000001) && cha.envelopeEnabled)
        {
            volume |= 0x10;
        }

        if (sb1 & 0b10000000) 
        {
            uint8_t envelopeSliding = (sb0 & 0b00100000)
                ? ((sb0 >> 1) | 0xF0) + cha.envelopeSliding
                : ((sb0 >> 1) & 0x0F) + cha.envelopeSliding;
            if (sb1 & 0b00100000)
            {
                cha.envelopeSliding = envelopeSliding;
            }
            envAdd += envelopeSliding;
        }
        else 
        {
            mod.m_global.noiseAdd = (sb0 >> 1) + cha.noiseSliding;
            if (sb1 & 0b00100000)
            {
                cha.noiseSliding = mod.m_global.noiseAdd;
            }
        }
        mixer |= (sb1 >> 1) & 0b01001000;
    }
    else volume = 0;
    mixer >>= 1;

    if (cha.currentOnOff > 0) 
    {
        if (!--cha.currentOnOff) 
        {
            cha.enabled ^= true;
            cha.currentOnOff = (cha.enabled ? cha.onOffDelay : cha.offOnDelay);
        }
    }
}

int DecodePT3::GetTonePeriod(int m, int note)
{
    switch (m_module[m].m_hdr->tonTableId)
    {
    case  0: return (m_version <= 3)
        ? NoteTable_PT_33_34r[note]
        : NoteTable_PT_34_35[note];
    case  1: return NoteTable_ST[note];
    case  2: return (m_version <= 3)
        ? NoteTable_ASM_34r[note]
        : NoteTable_ASM_34_35[note];
    default: return (m_version <= 3)
        ? NoteTable_REAL_34r[note]
        : NoteTable_REAL_34_35[note];
    }
}
