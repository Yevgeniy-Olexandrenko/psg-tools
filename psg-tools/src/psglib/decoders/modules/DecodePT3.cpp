#include "DecodePT3.h"
#include "stream/Stream.h"

const uint16_t DecodePT3::NoteTable_PT_33_34r[] = 
{
	0x0C21,0x0B73,0x0ACE,0x0A33,0x09A0,0x0916,0x0893,0x0818,0x07A4,0x0736,0x06CE,0x066D,
	0x0610,0x05B9,0x0567,0x0519,0x04D0,0x048B,0x0449,0x040C,0x03D2,0x039B,0x0367,0x0336,
	0x0308,0x02DC,0x02B3,0x028C,0x0268,0x0245,0x0224,0x0206,0x01E9,0x01CD,0x01B3,0x019B,
	0x0184,0x016E,0x0159,0x0146,0x0134,0x0122,0x0112,0x0103,0x00F4,0x00E6,0x00D9,0x00CD,
	0x00C2,0x00B7,0x00AC,0x00A3,0x009A,0x0091,0x0089,0x0081,0x007A,0x0073,0x006C,0x0066,
	0x0061,0x005B,0x0056,0x0051,0x004D,0x0048,0x0044,0x0040,0x003D,0x0039,0x0036,0x0033,
	0x0030,0x002D,0x002B,0x0028,0x0026,0x0024,0x0022,0x0020,0x001E,0x001C,0x001B,0x0019,
	0x0018,0x0016,0x0015,0x0014,0x0013,0x0012,0x0011,0x0010,0x000F,0x000E,0x000D,0x000C 
};

const uint16_t DecodePT3::NoteTable_PT_34_35[] =
{
    0x0C22,0x0B73,0x0ACF,0x0A33,0x09A1,0x0917,0x0894,0x0819,0x07A4,0x0737,0x06CF,0x066D,
    0x0611,0x05BA,0x0567,0x051A,0x04D0,0x048B,0x044A,0x040C,0x03D2,0x039B,0x0367,0x0337,
    0x0308,0x02DD,0x02B4,0x028D,0x0268,0x0246,0x0225,0x0206,0x01E9,0x01CE,0x01B4,0x019B,
    0x0184,0x016E,0x015A,0x0146,0x0134,0x0123,0x0112,0x0103,0x00F5,0x00E7,0x00DA,0x00CE,
    0x00C2,0x00B7,0x00AD,0x00A3,0x009A,0x0091,0x0089,0x0082,0x007A,0x0073,0x006D,0x0067,
    0x0061,0x005C,0x0056,0x0052,0x004D,0x0049,0x0045,0x0041,0x003D,0x003A,0x0036,0x0033,
    0x0031,0x002E,0x002B,0x0029,0x0027,0x0024,0x0022,0x0020,0x001F,0x001D,0x001B,0x001A,
    0x0018,0x0017,0x0016,0x0014,0x0013,0x0012,0x0011,0x0010,0x000F,0x000E,0x000D,0x000C 
};

const uint16_t DecodePT3::NoteTable_ST[] =
{
    0x0EF8,0x0E10,0x0D60,0x0C80,0x0BD8,0x0B28,0x0A88,0x09F0,0x0960,0x08E0,0x0858,0x07E0,
    0x077C,0x0708,0x06B0,0x0640,0x05EC,0x0594,0x0544,0x04F8,0x04B0,0x0470,0x042C,0x03FD,
    0x03BE,0x0384,0x0358,0x0320,0x02F6,0x02CA,0x02A2,0x027C,0x0258,0x0238,0x0216,0x01F8,
    0x01DF,0x01C2,0x01AC,0x0190,0x017B,0x0165,0x0151,0x013E,0x012C,0x011C,0x010A,0x00FC,
    0x00EF,0x00E1,0x00D6,0x00C8,0x00BD,0x00B2,0x00A8,0x009F,0x0096,0x008E,0x0085,0x007E,
    0x0077,0x0070,0x006B,0x0064,0x005E,0x0059,0x0054,0x004F,0x004B,0x0047,0x0042,0x003F,
    0x003B,0x0038,0x0035,0x0032,0x002F,0x002C,0x002A,0x0027,0x0025,0x0023,0x0021,0x001F,
    0x001D,0x001C,0x001A,0x0019,0x0017,0x0016,0x0015,0x0013,0x0012,0x0011,0x0010,0x000F 
};

const uint16_t DecodePT3::NoteTable_ASM_34r[] =
{
    0x0D3E,0x0C80,0x0BCC,0x0B22,0x0A82,0x09EC,0x095C,0x08D6,0x0858,0x07E0,0x076E,0x0704,
    0x069F,0x0640,0x05E6,0x0591,0x0541,0x04F6,0x04AE,0x046B,0x042C,0x03F0,0x03B7,0x0382,
    0x034F,0x0320,0x02F3,0x02C8,0x02A1,0x027B,0x0257,0x0236,0x0216,0x01F8,0x01DC,0x01C1,
    0x01A8,0x0190,0x0179,0x0164,0x0150,0x013D,0x012C,0x011B,0x010B,0x00FC,0x00EE,0x00E0,
    0x00D4,0x00C8,0x00BD,0x00B2,0x00A8,0x009F,0x0096,0x008D,0x0085,0x007E,0x0077,0x0070,
    0x006A,0x0064,0x005E,0x0059,0x0054,0x0050,0x004B,0x0047,0x0043,0x003F,0x003C,0x0038,
    0x0035,0x0032,0x002F,0x002D,0x002A,0x0028,0x0026,0x0024,0x0022,0x0020,0x001E,0x001D,
    0x001B,0x001A,0x0019,0x0018,0x0015,0x0014,0x0013,0x0012,0x0011,0x0010,0x000F,0x000E 
};

const uint16_t DecodePT3::NoteTable_ASM_34_35[] =
{
    0x0D10,0x0C55,0x0BA4,0x0AFC,0x0A5F,0x09CA,0x093D,0x08B8,0x083B,0x07C5,0x0755,0x06EC,
    0x0688,0x062A,0x05D2,0x057E,0x052F,0x04E5,0x049E,0x045C,0x041D,0x03E2,0x03AB,0x0376,
    0x0344,0x0315,0x02E9,0x02BF,0x0298,0x0272,0x024F,0x022E,0x020F,0x01F1,0x01D5,0x01BB,
    0x01A2,0x018B,0x0174,0x0160,0x014C,0x0139,0x0128,0x0117,0x0107,0x00F9,0x00EB,0x00DD,
    0x00D1,0x00C5,0x00BA,0x00B0,0x00A6,0x009D,0x0094,0x008C,0x0084,0x007C,0x0075,0x006F,
    0x0069,0x0063,0x005D,0x0058,0x0053,0x004E,0x004A,0x0046,0x0042,0x003E,0x003B,0x0037,
    0x0034,0x0031,0x002F,0x002C,0x0029,0x0027,0x0025,0x0023,0x0021,0x001F,0x001D,0x001C,
    0x001A,0x0019,0x0017,0x0016,0x0015,0x0014,0x0012,0x0011,0x0010,0x000F,0x000E,0x000D 
};

const uint16_t DecodePT3::NoteTable_REAL_34r[] =
{
    0x0CDA,0x0C22,0x0B73,0x0ACF,0x0A33,0x09A1,0x0917,0x0894,0x0819,0x07A4,0x0737,0x06CF,
    0x066D,0x0611,0x05BA,0x0567,0x051A,0x04D0,0x048B,0x044A,0x040C,0x03D2,0x039B,0x0367,
    0x0337,0x0308,0x02DD,0x02B4,0x028D,0x0268,0x0246,0x0225,0x0206,0x01E9,0x01CE,0x01B4,
    0x019B,0x0184,0x016E,0x015A,0x0146,0x0134,0x0123,0x0113,0x0103,0x00F5,0x00E7,0x00DA,
    0x00CE,0x00C2,0x00B7,0x00AD,0x00A3,0x009A,0x0091,0x0089,0x0082,0x007A,0x0073,0x006D,
    0x0067,0x0061,0x005C,0x0056,0x0052,0x004D,0x0049,0x0045,0x0041,0x003D,0x003A,0x0036,
    0x0033,0x0031,0x002E,0x002B,0x0029,0x0027,0x0024,0x0022,0x0020,0x001F,0x001D,0x001B,
    0x001A,0x0018,0x0017,0x0016,0x0014,0x0013,0x0012,0x0011,0x0010,0x000F,0x000E,0x000D 
};

const uint16_t DecodePT3::NoteTable_REAL_34_35[] =
{
    0x0CDA,0x0C22,0x0B73,0x0ACF,0x0A33,0x09A1,0x0917,0x0894,0x0819,0x07A4,0x0737,0x06CF,
    0x066D,0x0611,0x05BA,0x0567,0x051A,0x04D0,0x048B,0x044A,0x040C,0x03D2,0x039B,0x0367,
    0x0337,0x0308,0x02DD,0x02B4,0x028D,0x0268,0x0246,0x0225,0x0206,0x01E9,0x01CE,0x01B4,
    0x019B,0x0184,0x016E,0x015A,0x0146,0x0134,0x0123,0x0112,0x0103,0x00F5,0x00E7,0x00DA,
    0x00CE,0x00C2,0x00B7,0x00AD,0x00A3,0x009A,0x0091,0x0089,0x0082,0x007A,0x0073,0x006D,
    0x0067,0x0061,0x005C,0x0056,0x0052,0x004D,0x0049,0x0045,0x0041,0x003D,0x003A,0x0036,
    0x0033,0x0031,0x002E,0x002B,0x0029,0x0027,0x0024,0x0022,0x0020,0x001F,0x001D,0x001B,
    0x001A,0x0018,0x0017,0x0016,0x0014,0x0013,0x0012,0x0011,0x0010,0x000F,0x000E,0x000D 
};

const uint8_t DecodePT3::VolumeTable_33_34[16][16] =
{
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x02,0x02,0x02,0x02,0x02},
    {0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x03},
    {0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x02,0x02,0x02,0x03,0x03,0x03,0x04,0x04,0x04},
    {0x00,0x00,0x00,0x01,0x01,0x01,0x02,0x02,0x03,0x03,0x03,0x04,0x04,0x04,0x05,0x05},
    {0x00,0x00,0x00,0x01,0x01,0x02,0x02,0x03,0x03,0x03,0x04,0x04,0x05,0x05,0x06,0x06},
    {0x00,0x00,0x01,0x01,0x02,0x02,0x03,0x03,0x04,0x04,0x05,0x05,0x06,0x06,0x07,0x07},
    {0x00,0x00,0x01,0x01,0x02,0x02,0x03,0x03,0x04,0x05,0x05,0x06,0x06,0x07,0x07,0x08},
    {0x00,0x00,0x01,0x01,0x02,0x03,0x03,0x04,0x05,0x05,0x06,0x06,0x07,0x08,0x08,0x09},
    {0x00,0x00,0x01,0x02,0x02,0x03,0x04,0x04,0x05,0x06,0x06,0x07,0x08,0x08,0x09,0x0A},
    {0x00,0x00,0x01,0x02,0x03,0x03,0x04,0x05,0x06,0x06,0x07,0x08,0x09,0x09,0x0A,0x0B},
    {0x00,0x00,0x01,0x02,0x03,0x04,0x04,0x05,0x06,0x07,0x08,0x08,0x09,0x0A,0x0B,0x0C},
    {0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D},
    {0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E},
    {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F}
};

const uint8_t DecodePT3::VolumeTable_35[16][16] =
{
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01},
    {0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x02,0x02,0x02,0x02},
    {0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x02,0x02,0x02,0x02,0x02,0x03,0x03,0x03},
    {0x00,0x00,0x01,0x01,0x01,0x01,0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x03,0x04,0x04},
    {0x00,0x00,0x01,0x01,0x01,0x02,0x02,0x02,0x03,0x03,0x03,0x04,0x04,0x04,0x05,0x05},
    {0x00,0x00,0x01,0x01,0x02,0x02,0x02,0x03,0x03,0x04,0x04,0x04,0x05,0x05,0x06,0x06},
    {0x00,0x00,0x01,0x01,0x02,0x02,0x03,0x03,0x04,0x04,0x05,0x05,0x06,0x06,0x07,0x07},
    {0x00,0x01,0x01,0x02,0x02,0x03,0x03,0x04,0x04,0x05,0x05,0x06,0x06,0x07,0x07,0x08},
    {0x00,0x01,0x01,0x02,0x02,0x03,0x04,0x04,0x05,0x05,0x06,0x07,0x07,0x08,0x08,0x09},
    {0x00,0x01,0x01,0x02,0x03,0x03,0x04,0x05,0x05,0x06,0x07,0x07,0x08,0x09,0x09,0x0A},
    {0x00,0x01,0x01,0x02,0x03,0x04,0x04,0x05,0x06,0x07,0x07,0x08,0x09,0x0A,0x0A,0x0B},
    {0x00,0x01,0x02,0x02,0x03,0x04,0x05,0x06,0x06,0x07,0x08,0x09,0x0A,0x0A,0x0B,0x0C},
    {0x00,0x01,0x02,0x03,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0A,0x0B,0x0C,0x0D},
    {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E},
    {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F} 
};

const std::string PTSignature = "ProTracker 3.";
const std::string VTSignature = "Vortex Tracker II";


////////////////////////////////////////////////////////////////////////////////

bool DecodePT3::Open(Stream& stream)
{
    bool isDetected = false;
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
            m_size = (uint32_t)fileStream.tellg();

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
            else if (m_module[0].m_hdr->tonTableId == 2 && m_ver > 3)
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
    m_ver = ('0' <= ver && ver <= '9') ? ver - '0' : 6;

    m_module[0].ts = m_module[1].ts = 0x20;
    int TS = m_module[0].m_hdr->musicName[98];
    m_isTS = (TS != 0x20);
    if (m_isTS) m_module[1].ts = TS;
    else if (m_size > 400 && !memcmp(m_data + m_size - 4, "02TS", 4)) 
    { 
        // try load Vortex II '02TS'
        uint16_t sz1 = m_data[m_size - 12] + 0x100 * m_data[m_size - 11];
        uint16_t sz2 = m_data[m_size - 6 ] + 0x100 * m_data[m_size - 5 ];
        if (uint32_t(sz1 + sz2) < m_size && sz1 > 200 && sz2 > 200) 
        {
            m_isTS = true;
            m_module[1].m_data = (m_data + sz1);
            m_module[1].m_hdr = (Header*)m_module[1].m_data;
        }
    }

    for (auto& module : m_module) 
    {
        module.m_delay = module.m_hdr->delay;
        module.m_delayCounter = 1;

        int b = module.ts;
        uint8_t i = module.m_hdr->positionList[0];
        if (b != 0x20) i = (uint8_t)(3 * b - 3 - i);

        for (int c = 0; c < 3; ++c) 
        {
            Channel& chan = module.m_channels[c];
            chan.patternPtr =
                module.m_data[module.m_hdr->patternsPointer + 2 * (i + c) + 0] +
                module.m_data[module.m_hdr->patternsPointer + 2 * (i + c) + 1] * 0x100;

            chan.samplePtr = module.m_hdr->samplesPointers[2] + module.m_hdr->samplesPointers[3] * 0x100;
            chan.ornamentPtr = module.m_hdr->ornamentsPointers[0] + module.m_hdr->ornamentsPointers[1] * 0x100;
            chan.ornamentLoop = module.m_data[chan.ornamentPtr++];
            chan.ornamentLen = module.m_data[chan.ornamentPtr++];
            chan.sampleLoop = module.m_data[chan.samplePtr++];
            chan.sampleLen = module.m_data[chan.samplePtr++];
            chan.volume = 15;
            chan.noteSkipCounter = 1;
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
    bool isNewLoop = Play(0);
    if (m_isTS) Play(1);
    return isNewLoop;
}

////////////////////////////////////////////////////////////////////////////////

bool DecodePT3::Play(int m)
{
    Module& module = m_module[m];

    bool loop = false;
    if (--module.m_delayCounter == 0) 
    {
        for (int c = 0; c < 3; ++c) 
        {
            Channel& chan = module.m_channels[c];
            if (!--chan.noteSkipCounter) 
            {
                if (!c && module.m_data[chan.patternPtr] == 0) 
                {
                    if (++module.m_currentPosition == module.m_hdr->numberOfPositions)
                    {
                        module.m_currentPosition = module.m_hdr->loopPosition;
                        loop = true;
                    }

                    uint8_t i = module.m_hdr->positionList[module.m_currentPosition];
                    if (module.ts != 0x20) i = (uint8_t)(3 * module.ts - 3 - i);

                    for (int c = 0; c < 3; c++)
                    {
                        module.m_channels[c].patternPtr =
                            module.m_data[module.m_hdr->patternsPointer + 2 * (i + c) + 0] +
                            module.m_data[module.m_hdr->patternsPointer + 2 * (i + c) + 1] * 0x100;
                    }
                    module.glob.noiseBase = 0;
                }
                ProcessPattern(m, chan);
            }
        }
        module.m_delayCounter = module.m_delay;
    }
    
    uint8_t mixer = 0;
    int envAdd = 0;
    ProcessInstrument(m, 0, mixer, envAdd);
    ProcessInstrument(m, 1, mixer, envAdd);
    ProcessInstrument(m, 2, mixer, envAdd);

    m_regs[m][Mixer] = mixer;
    m_regs[m][N_Period] = (module.glob.noiseBase + module.glob.noiseAdd) & 0x1F;

    for (int ch = 0; ch < 3; ch++) 
    {
        auto& _chan = module.m_channels[ch];
        m_regs[m][A_Fine   + 2 * ch] = (_chan.tone & 0xFF);
        m_regs[m][A_Coarse + 2 * ch] = (_chan.tone >> 8 & 0x0F);
        m_regs[m][A_Volume + ch] = _chan.amplitude;
    }

    uint16_t env = module.glob.envBaseHi * 0x100 + module.glob.envBaseLo + envAdd + module.glob.curEnvSlide;
    m_regs[m][E_Fine] = (env & 0xFF);
    m_regs[m][E_Coarse] = (env >> 8 & 0xFF);

    if (module.glob.curEnvDelay > 0) 
    {
        if (!--module.glob.curEnvDelay) 
        {
            module.glob.curEnvDelay = module.glob.envDelay;
            module.glob.curEnvSlide += module.glob.envSlideAdd;
        }
    }

    return loop;
}

void DecodePT3::ProcessPattern(int chip, Channel& chan)
{
    int PrNote = chan.note;
    int PrSliding = chan.toneSliding;

    uint8_t counter = 0;
    uint8_t f1 = 0, f2 = 0, f3 = 0, f4 = 0, f5 = 0, f8 = 0, f9 = 0;

    auto& _chip = m_module[chip];
    for (;;) 
    {
        uint8_t cc = _chip.m_data[chan.patternPtr];
        if (0xF0 <= cc && cc <= 0xFF)
        {
            uint8_t c1 = cc - 0xF0;
            chan.ornamentPtr = _chip.m_hdr->ornamentsPointers[2 * c1] + 0x100 * _chip.m_hdr->ornamentsPointers[2 * c1 + 1];
            chan.ornamentLoop = _chip.m_data[chan.ornamentPtr++];
            chan.ornamentLen = _chip.m_data[chan.ornamentPtr++];
            chan.patternPtr++;

            uint8_t c2 = _chip.m_data[chan.patternPtr] / 2;
            chan.samplePtr = _chip.m_hdr->samplesPointers[2 * c2] + 0x100 * _chip.m_hdr->samplesPointers[2 * c2 + 1];
            chan.sampleLoop = _chip.m_data[chan.samplePtr++];
            chan.sampleLen = _chip.m_data[chan.samplePtr++];
            chan.envelopeEnabled = false;
            chan.ornamentPos = 0;
        }
        else if (0xD1 <= cc && cc <= 0xEF) 
        {
            uint8_t c2 = cc - 0xD0;
            chan.samplePtr = _chip.m_hdr->samplesPointers[2 * c2] + 0x100 * _chip.m_hdr->samplesPointers[2 * c2 + 1];
            chan.sampleLoop = _chip.m_data[chan.samplePtr++];
            chan.sampleLen = _chip.m_data[chan.samplePtr++];
        }
        else if (cc == 0xD0) 
        {
            chan.patternPtr++;
            break;
        }
        else if (0xC1 <= cc && cc <= 0xCF) 
        {
            chan.volume = cc - 0xC0;
        }
        else if (cc == 0xC0) 
        {
            chan.samplePos = 0;
            chan.volumeSliding = 0;
            chan.noiseSliding = 0;
            chan.envelopeSliding = 0;
            chan.ornamentPos = 0;
            chan.tonSlideCount = 0;
            chan.toneSliding = 0;
            chan.toneAcc = 0;
            chan.currentOnOff = 0;
            chan.enabled = false;
            chan.patternPtr++;
            break;
        }
        else if (0xB2 <= cc && cc <= 0xBF) 
        {
            chan.envelopeEnabled = true;
            m_regs[chip][E_Shape] = (cc - 0xB1);
            _chip.glob.envBaseHi = _chip.m_data[++chan.patternPtr];
            _chip.glob.envBaseLo = _chip.m_data[++chan.patternPtr];
            chan.ornamentPos = 0;
            _chip.glob.curEnvSlide = 0;
            _chip.glob.curEnvDelay = 0;
        }
        else if (cc == 0xB1) 
        {
            chan.numberOfNotesToSkip = _chip.m_data[++chan.patternPtr];
        }
        else if (cc == 0xB0) 
        {
            chan.envelopeEnabled = false;
            chan.ornamentPos = 0;
        }
        else if (0x50 <= cc && cc <= 0xAF) 
        {
            chan.note = cc - 0x50;
            chan.samplePos = 0;
            chan.volumeSliding = 0;
            chan.noiseSliding = 0;
            chan.envelopeSliding = 0;
            chan.ornamentPos = 0;
            chan.tonSlideCount = 0;
            chan.toneSliding = 0;
            chan.toneAcc = 0;
            chan.currentOnOff = 0;
            chan.enabled = true;
            chan.patternPtr++;
            break;
        }
        else if (0x40 <= cc && cc <= 0x4F)
        {
            uint8_t c1 = cc - 0x40;
            chan.ornamentPtr = _chip.m_hdr->ornamentsPointers[2 * c1] + 0x100 * _chip.m_hdr->ornamentsPointers[2 * c1 + 1];
            chan.ornamentLoop = _chip.m_data[chan.ornamentPtr++];
            chan.ornamentLen = _chip.m_data[chan.ornamentPtr++];
            chan.ornamentPos = 0;
        }
        else if (0x20 <= cc && cc <= 0x3F) 
        {
            _chip.glob.noiseBase = cc - 0x20;
        }
        else if (0x10 <= cc && cc <= 0x1F) 
        {
            chan.envelopeEnabled = (cc != 0x10);
            if (chan.envelopeEnabled) 
            {
                m_regs[chip][E_Shape] = (cc - 0x10);
                _chip.glob.envBaseHi = _chip.m_data[++chan.patternPtr];
                _chip.glob.envBaseLo = _chip.m_data[++chan.patternPtr];
                _chip.glob.curEnvSlide = 0;
                _chip.glob.curEnvDelay = 0;
            }

            uint8_t c2 = _chip.m_data[++chan.patternPtr] / 2;
            chan.samplePtr = _chip.m_hdr->samplesPointers[2 * c2] + 0x100 * _chip.m_hdr->samplesPointers[2 * c2 + 1];
            chan.sampleLoop = _chip.m_data[chan.samplePtr++];
            chan.sampleLen = _chip.m_data[chan.samplePtr++];
            chan.ornamentPos = 0;
        }
        else if (cc == 0x09) f9 = ++counter;
        else if (cc == 0x08) f8 = ++counter;
        else if (cc == 0x05) f5 = ++counter;
        else if (cc == 0x04) f4 = ++counter;
        else if (cc == 0x03) f3 = ++counter;
        else if (cc == 0x02) f2 = ++counter;
        else if (cc == 0x01) f1 = ++counter;

        chan.patternPtr++;
    }

    while (counter > 0) 
    {
        if (counter == f1) 
        {
            chan.tonSlideDelay = _chip.m_data[chan.patternPtr++];
            chan.tonSlideCount = chan.tonSlideDelay;
            chan.tonSlideStep = (int16_t)(_chip.m_data[chan.patternPtr] + 0x100 * _chip.m_data[chan.patternPtr + 1]);
            chan.patternPtr += 2;
            chan.simpleGliss = true;
            chan.currentOnOff = 0;
            if (chan.tonSlideCount == 0 && m_ver >= 7)
                chan.tonSlideCount++;
        }
        else if (counter == f2) 
        {
            chan.simpleGliss = false;
            chan.currentOnOff = 0;
            chan.tonSlideDelay = _chip.m_data[chan.patternPtr];
            chan.tonSlideCount = chan.tonSlideDelay;
            chan.patternPtr += 3;
            uint16_t step = _chip.m_data[chan.patternPtr] + 0x100 * _chip.m_data[chan.patternPtr + 1];
            chan.patternPtr += 2;
            int16_t signed_step = step;
            chan.tonSlideStep = (signed_step < 0) ? -signed_step : signed_step;
            chan.toneDelta = GetToneFromNote(chip, chan.note) - GetToneFromNote(chip, PrNote);
            chan.slideToNote = chan.note;
            chan.note = PrNote;
            if (m_ver >= 6) chan.toneSliding = PrSliding;
            if (chan.toneDelta - chan.toneSliding < 0)
                chan.tonSlideStep = -chan.tonSlideStep;
        }
        else if (counter == f3) 
        {
            chan.samplePos = _chip.m_data[chan.patternPtr++];
        }
        else if (counter == f4) 
        {
            chan.ornamentPos = _chip.m_data[chan.patternPtr++];
        }
        else if (counter == f5) 
        {
            chan.onOffDelay = _chip.m_data[chan.patternPtr++];
            chan.offOnDelay = _chip.m_data[chan.patternPtr++];
            chan.currentOnOff = chan.onOffDelay;
            chan.tonSlideCount = 0;
            chan.toneSliding = 0;
        }
        else if (counter == f8) 
        {
            _chip.glob.envDelay = _chip.m_data[chan.patternPtr++];
            _chip.glob.curEnvDelay = _chip.glob.envDelay;
            _chip.glob.envSlideAdd = _chip.m_data[chan.patternPtr] + 0x100 * _chip.m_data[chan.patternPtr + 1];
            chan.patternPtr += 2;
        }
        else if (counter == f9) 
        {
            uint8_t b = _chip.m_data[chan.patternPtr++];
            _chip.m_delay = b;
            if (m_isTS && m_module[1].ts != 0x20) 
            {
                m_module[0].m_delay = b;
                m_module[1].m_delay = b;
                m_module[0].m_delayCounter = b;
            }
        }
        counter--;
    }
    chan.noteSkipCounter = chan.numberOfNotesToSkip;
}

void DecodePT3::ProcessInstrument(int m, int c, uint8_t& mixer, int& envAdd)
{
    Module& module = m_module[m];
    Channel& chan = module.m_channels[c];

    if (chan.enabled) 
    {
        uint16_t sptr = (chan.samplePtr + 4 * chan.samplePos);
        if (++chan.samplePos >= chan.sampleLen)
            chan.samplePos = chan.sampleLoop;

        uint16_t optr = chan.ornamentPtr + chan.ornamentPos;
        if (++chan.ornamentPos >= chan.ornamentLen)
            chan.ornamentPos = chan.ornamentLoop;

        uint8_t sb0 = module.m_data[sptr + 0];
        uint8_t sb1 = module.m_data[sptr + 1];
        uint8_t sb2 = module.m_data[sptr + 2];
        uint8_t sb3 = module.m_data[sptr + 3];
        uint8_t ob0 = module.m_data[optr + 0];

        chan.tone = sb2 | sb3 << 8;
        chan.tone += chan.toneAcc;
        if (sb1 & 0x40) chan.toneAcc = chan.tone;

        int8_t note = (chan.note + ob0);
        if (note < 0 ) note = 0;
        if (note > 95) note = 95;

        int tone = GetToneFromNote(m, note);
        chan.tone += (chan.toneSliding + tone);
        chan.tone &= 0x0FFF;

        if (chan.tonSlideCount > 0)
        {
            if (!--chan.tonSlideCount) 
            {
                chan.toneSliding += chan.tonSlideStep;
                chan.tonSlideCount = chan.tonSlideDelay;
                if (!chan.simpleGliss) 
                {
                    if ((chan.tonSlideStep < 0 && chan.toneSliding <= chan.toneDelta) ||
                        (chan.tonSlideStep >= 0 && chan.toneSliding >= chan.toneDelta))
                    {
                        chan.note = chan.slideToNote;
                        chan.tonSlideCount = 0;
                        chan.toneSliding = 0;
                    }
                }
            }
        }

        int volume = (sb1 & 0x0F);
        if (sb0 & 0b10000000) 
        {
            if (sb0 & 0b01000000)
            {
                if (chan.volumeSliding < +15) ++chan.volumeSliding;
            }
            else
            {
                if (chan.volumeSliding > -15) --chan.volumeSliding;
            }
        }
        volume += chan.volumeSliding;
        if (volume < 0x0) volume = 0x0;
        if (volume > 0xF) volume = 0xF;

        if (m_ver <= 4) 
            chan.amplitude = VolumeTable_33_34[chan.volume][volume];
        else 
            chan.amplitude = VolumeTable_35[chan.volume][volume];

        if (!(sb0 & 0b00000001) && chan.envelopeEnabled)
        {
            chan.amplitude |= 0x10;
        }

        if (sb1 & 0b10000000) 
        {
            uint8_t envelopeSliding = (sb0 & 0b00100000)
                ? ((sb0 >> 1) | 0xF0) + chan.envelopeSliding
                : ((sb0 >> 1) & 0x0F) + chan.envelopeSliding;
            if (sb1 & 0b00100000)
            {
                chan.envelopeSliding = envelopeSliding;
            }
            envAdd += envelopeSliding;
        }
        else 
        {
            module.glob.noiseAdd = (sb0 >> 1) + chan.noiseSliding;
            if (sb1 & 0b00100000)
            {
                chan.noiseSliding = module.glob.noiseAdd;
            }
        }
        mixer |= (sb1 >> 1) & 0b01001000;
    }
    else 
    {
        chan.amplitude = 0;
    }
    mixer >>= 1;

    if (chan.currentOnOff > 0) 
    {
        if (!--chan.currentOnOff) 
        {
            chan.enabled ^= true;
            chan.currentOnOff = (chan.enabled ? chan.onOffDelay : chan.offOnDelay);
        }
    }
}

int DecodePT3::GetToneFromNote(int chip, int note)
{
    switch (m_module[chip].m_hdr->tonTableId)
    {
    case  0: return (m_ver <= 3) ? NoteTable_PT_33_34r[note] : NoteTable_PT_34_35[note];
    case  1: return NoteTable_ST[note];
    case  2: return (m_ver <= 3) ? NoteTable_ASM_34r[note]  : NoteTable_ASM_34_35[note];
    default: return (m_ver <= 3) ? NoteTable_REAL_34r[note] : NoteTable_REAL_34_35[note];
    }
}
