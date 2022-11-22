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
                isHeaderOk &= (header.PSC_OrnamentsPointer < fileSize);
            //  isHeaderOk &= (header.PSC_OrnamentsPointer >= 0x4C + 0x02);
            //  isHeaderOk &= (header.PSC_OrnamentsPointer <= 0x4C + 0x40);
                isHeaderOk &= (header.PSC_OrnamentsPointer % 2 == 0);
            //  isHeaderOk &= (header.PSC_SamplesPointers[0] + 0x4C + 0x05 <= fileSize);
                isHeaderOk &= (header.PSC_PatternsPointer + 11 < fileSize);

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

    m_version = 7;
    if (hdr.PSC_MusicName[0x08] >= '0' && hdr.PSC_MusicName[0x08] <= '9')
        m_version = (hdr.PSC_MusicName[0x08] - '0');

    DelayCounter = 1;
    Delay = hdr.PSC_Delay;
    Positions_Pointer = hdr.PSC_PatternsPointer;
    Lines_Counter = 1;
    Noise_Base = 0;

    PSC_A.num = 0;
    PSC_B.num = 1;
    PSC_C.num = 2;

    PSC_A.SamplePointer = hdr.PSC_SamplesPointers[0];
    if (m_version > 3) PSC_A.SamplePointer += 0x4C;
    PSC_B.SamplePointer = PSC_A.SamplePointer;
    PSC_C.SamplePointer = PSC_A.SamplePointer;
    PSC_A.OrnamentPointer = *(uint16_t*)(&m_data[hdr.PSC_OrnamentsPointer]);
    if (m_version > 3) PSC_A.OrnamentPointer += hdr.PSC_OrnamentsPointer;
    PSC_B.OrnamentPointer = PSC_A.OrnamentPointer;
    PSC_C.OrnamentPointer = PSC_A.OrnamentPointer;

    PSC_A.Break_Ornament_Loop = false;
    PSC_A.Ornament_Enabled = false;
    PSC_A.Enabled = false;
    PSC_A.Break_Sample_Loop = false;
    PSC_A.Ton_Slide_Enabled = false;
    PSC_A.Note_Skip_Counter = 1;
    PSC_A.Ton = 0;

    PSC_B.Break_Ornament_Loop = false;
    PSC_B.Ornament_Enabled = false;
    PSC_B.Enabled = false;
    PSC_B.Break_Sample_Loop = false;
    PSC_B.Ton_Slide_Enabled = false;
    PSC_B.Note_Skip_Counter = 1;
    PSC_B.Ton = 0;

    PSC_C.Break_Ornament_Loop = false;
    PSC_C.Ornament_Enabled = false;
    PSC_C.Enabled = false;
    PSC_C.Break_Sample_Loop = false;
    PSC_C.Ton_Slide_Enabled = false;
    PSC_C.Note_Skip_Counter = 1;
    PSC_C.Ton = 0;
    memset(&m_regs, 0, sizeof(m_regs));
}

void DecodePSC::Loop(uint8_t& currPosition, uint8_t& lastPosition, uint8_t& loopPosition)
{
}

bool DecodePSC::Play()
{
    bool loop = false;
    if (--DelayCounter <= 0)
    {
        if (--Lines_Counter <= 0)
        {
            if (m_data[Positions_Pointer + 1] == 255)
            {
                Positions_Pointer = *(uint16_t*)(&m_data[Positions_Pointer + 2]);
                loop = true;
            }
            Lines_Counter = m_data[Positions_Pointer + 1];

            PSC_A.Address_In_Pattern = *(uint16_t*)(&m_data[Positions_Pointer + 2]);
            PSC_B.Address_In_Pattern = *(uint16_t*)(&m_data[Positions_Pointer + 4]);
            PSC_C.Address_In_Pattern = *(uint16_t*)(&m_data[Positions_Pointer + 6]);
            Positions_Pointer += 8;

            PSC_A.Note_Skip_Counter = 1;
            PSC_B.Note_Skip_Counter = 1;
            PSC_C.Note_Skip_Counter = 1;
        }
        if (--PSC_A.Note_Skip_Counter == 0) PatternInterpreter(PSC_A);
        if (--PSC_B.Note_Skip_Counter == 0) PatternInterpreter(PSC_B);
        if (--PSC_C.Note_Skip_Counter == 0) PatternInterpreter(PSC_C);

        PSC_A.Noise_Accumulator += Noise_Base;
        PSC_B.Noise_Accumulator += Noise_Base;
        PSC_C.Noise_Accumulator += Noise_Base;
        DelayCounter = Delay;
    }

    uint8_t TempMixer = 0;
    GetRegisters(PSC_A, TempMixer);
    GetRegisters(PSC_B, TempMixer);
    GetRegisters(PSC_C, TempMixer);

    m_regs[0][Mixer] = TempMixer;
    m_regs[0][A_Fine] = (PSC_A.Ton & 0xff);
    m_regs[0][A_Coarse] = ((PSC_A.Ton >> 8) & 0xf);
    m_regs[0][B_Fine] = (PSC_B.Ton & 0xff);
    m_regs[0][B_Coarse] = ((PSC_B.Ton >> 8) & 0xf);
    m_regs[0][C_Fine] = (PSC_C.Ton & 0xff);
    m_regs[0][C_Coarse] = ((PSC_C.Ton >> 8) & 0xf);
    m_regs[0][A_Volume] = PSC_A.Amplitude;
    m_regs[0][B_Volume] = PSC_B.Amplitude;
    m_regs[0][C_Volume] = PSC_C.Amplitude;
    return loop;
}

void DecodePSC::PatternInterpreter(Channel& chan)
{
    auto& hdr = reinterpret_cast<const Header&>(*m_data);

    bool quit;
    bool b1b, b2b, b3b, b4b, b5b, b6b, b7b;
    quit = b1b = b2b = b3b = b4b = b5b = b6b = b7b = false;
    do
    {
        uint8_t val = m_data[chan.Address_In_Pattern];
        if (val >= 0xc0)
        {
            chan.Note_Skip_Counter = val - 0xbf;
            quit = true;
        }
        else if (val >= 0xa0 && val <= 0xbf)
        {
            int o = (val - 0xa0);
            chan.OrnamentPointer = *(uint16_t*)(&m_data[hdr.PSC_OrnamentsPointer + o * 2]);
            if (m_version > 3) chan.OrnamentPointer += hdr.PSC_OrnamentsPointer;
        }
        else if (val >= 0x7e && val <= 0x9f)
        {
            if (val >= 0x80)
            {
                int s = (val - 0x80);
                chan.SamplePointer = hdr.PSC_SamplesPointers[s];
                if (m_version > 3) chan.SamplePointer += 0x4C;
            }
        }
        else if (val == 0x6b)
        {
            chan.Address_In_Pattern++;
            chan.Addition_To_Ton = m_data[chan.Address_In_Pattern];
            b5b = true;
        }
        else if (val == 0x6c)
        {
            chan.Address_In_Pattern++;
            chan.Addition_To_Ton = -(int8_t)(m_data[chan.Address_In_Pattern]);
            b5b = true;
        }
        else if (val == 0x6d)
        {
            b4b = true;
            chan.Address_In_Pattern++;
            chan.Addition_To_Ton = m_data[chan.Address_In_Pattern];
        }
        else if (val == 0x6e)
        {
            chan.Address_In_Pattern++;
            Delay = m_data[chan.Address_In_Pattern];
        }
        else if (val == 0x6f)
        {
            b1b = true;
            chan.Address_In_Pattern++;
        }
        else if (val == 0x70)
        {
            b3b = true;
            chan.Address_In_Pattern++;
            chan.Volume_Counter1 = m_data[chan.Address_In_Pattern];
        }
        else if (val == 0x71)
        {
            chan.Break_Ornament_Loop = true;
            chan.Address_In_Pattern++;
        }
        else if (val == 0x7a)
        {
            chan.Address_In_Pattern++;
            if (chan.num == 1)
            {
                m_regs[0][E_Shape] = m_data[chan.Address_In_Pattern] & 15;
                m_regs[0][E_Fine] = m_data[chan.Address_In_Pattern + 1];
                m_regs[0][E_Coarse] = m_data[chan.Address_In_Pattern + 2];
                chan.Address_In_Pattern += 2;
            }
        }
        else if (val == 0x7b)
        {
            chan.Address_In_Pattern++;
            if (chan.num == 1)
                Noise_Base = m_data[chan.Address_In_Pattern];
        }
        else if (val == 0x7c)
        {
            b1b = b3b = b4b = b5b = b6b = b7b = false;
            b2b = true;
        }
        else if (val == 0x7d)
        {
            chan.Break_Sample_Loop = true;
        }
        else if (val >= 0x58 && val <= 0x66)
        {
            chan.Initial_Volume = val - 0x57;
            chan.Envelope_Enabled = false;
            b6b = true;
        }
        else if (val == 0x57)
        {
            chan.Initial_Volume = 0xf;
            chan.Envelope_Enabled = true;
            b6b = true;
        }
        else if (val <= 0x56)
        {
            chan.Note = val;
            b6b = true;
            b7b = true;
        }
        else
        {
            chan.Address_In_Pattern++;
        }
        chan.Address_In_Pattern++;
    } while (!quit);

    if (b7b)
    {
        chan.Break_Ornament_Loop = false;
        chan.Ornament_Enabled = true;
        chan.Enabled = true;
        chan.Break_Sample_Loop = false;
        chan.Ton_Slide_Enabled = false;
        chan.Ton_Accumulator = 0;
        chan.Current_Ton_Sliding = 0;
        chan.Noise_Accumulator = 0;
        chan.Volume_Counter = 0;
        chan.Position_In_Sample = 0;
        chan.Position_In_Ornament = 0;
    }
    if (b6b)
    {
        chan.Volume = chan.Initial_Volume;
    }
    if (b5b)
    {
        chan.Gliss = false;
        chan.Ton_Slide_Enabled = true;
    }
    if (b4b)
    {
        chan.Current_Ton_Sliding = chan.Ton - NoteTable_ASM[chan.Note];
        chan.Gliss = true;
        if (chan.Current_Ton_Sliding >= 0)
            chan.Addition_To_Ton = -chan.Addition_To_Ton;
        chan.Ton_Slide_Enabled = true;
    }
    if (b3b)
    {
        chan.Volume_Counter = chan.Volume_Counter1;
        chan.Volume_Inc = true;
        if ((chan.Volume_Counter & 0x40) != 0)
        {
            chan.Volume_Counter = -(int8_t)(chan.Volume_Counter | 128);
            chan.Volume_Inc = false;
        }
        chan.Volume_Counter_Init = chan.Volume_Counter;
    }
    if (b2b)
    {
        chan.Break_Ornament_Loop = false;
        chan.Ornament_Enabled = false;
        chan.Enabled = false;
        chan.Break_Sample_Loop = false;
        chan.Ton_Slide_Enabled = false;
    }
    if (b1b)
        chan.Ornament_Enabled = false;
}

void DecodePSC::GetRegisters(Channel& chan, uint8_t& mixer)
{
    uint8_t j, b;

    if (chan.Enabled)
    {
        j = chan.Note;
        if (chan.Ornament_Enabled)
        {
            b = m_data[chan.OrnamentPointer + chan.Position_In_Ornament * 2];
            chan.Noise_Accumulator += b;
            j += m_data[chan.OrnamentPointer + chan.Position_In_Ornament * 2 + 1];
            if ((int8_t)j < 0)
                j += 0x56;
            if (j > 0x55)
                j -= 0x56;
            if (j > 0x55)
                j = 0x55;
            if ((b & 128) == 0)
                chan.Loop_Ornament_Position = chan.Position_In_Ornament;
            if ((b & 64) == 0)
            {
                if (!chan.Break_Ornament_Loop)
                    chan.Position_In_Ornament = chan.Loop_Ornament_Position;
                else
                {
                    chan.Break_Ornament_Loop = false;
                    if ((b & 32) == 0)
                        chan.Ornament_Enabled = false;
                    chan.Position_In_Ornament++;
                }
            }
            else
            {
                if ((b & 32) == 0)
                    chan.Ornament_Enabled = false;
                chan.Position_In_Ornament++;
            }
        }
        chan.Note = j;
        chan.Ton = *(uint16_t*)(&m_data[chan.SamplePointer + chan.Position_In_Sample * 6]);
        chan.Ton_Accumulator += chan.Ton;
        chan.Ton = NoteTable_ASM[j] + chan.Ton_Accumulator;
        if (chan.Ton_Slide_Enabled)
        {
            chan.Current_Ton_Sliding += chan.Addition_To_Ton;
            if (chan.Gliss && (((chan.Current_Ton_Sliding < 0) && (chan.Addition_To_Ton <= 0)) || ((chan.Current_Ton_Sliding >= 0) && (chan.Addition_To_Ton >= 0))))
                chan.Ton_Slide_Enabled = false;
            chan.Ton += chan.Current_Ton_Sliding;
        }
        chan.Ton = chan.Ton & 0xfff;
        b = m_data[chan.SamplePointer + chan.Position_In_Sample * 6 + 4];
        mixer |= ((b & 9) << 3);
        j = 0;
        if ((b & 2) != 0)
            j++;
        if ((b & 4) != 0)
            j--;
        if (chan.Volume_Counter > 0)
        {
            chan.Volume_Counter--;
            if (chan.Volume_Counter == 0)
            {
                if (chan.Volume_Inc)
                    j++;
                else
                    j--;
                chan.Volume_Counter = chan.Volume_Counter_Init;
            }
        }
        chan.Volume += j;
        if ((int8_t)chan.Volume < 0)
            chan.Volume = 0;
        else if (chan.Volume > 15)
            chan.Volume = 15;
        chan.Amplitude = ((chan.Volume + 1) * (m_data[chan.SamplePointer + chan.Position_In_Sample * 6 + 3] & 15)) >> 4;
        if (chan.Envelope_Enabled && ((b & 16) == 0))
            chan.Amplitude = chan.Amplitude | 16;
        if (((chan.Amplitude & 16) != 0) && ((b & 8) != 0))
        {
            uint16_t env = m_regs[0][E_Fine] | m_regs[0][E_Coarse] << 8;
            env += (int8_t)(m_data[chan.SamplePointer + chan.Position_In_Sample * 6 + 2]);
            m_regs[0][E_Fine] = (env & 0xff);
            m_regs[0][E_Coarse] = ((env >> 8) & 0xff);
        }
        else
        {
            chan.Noise_Accumulator += m_data[chan.SamplePointer + chan.Position_In_Sample * 6 + 2];
            if ((b & 8) == 0)
                m_regs[0][N_Period] = (chan.Noise_Accumulator & 31);
        }
        if ((b & 128) == 0)
            chan.Loop_Sample_Position = chan.Position_In_Sample;
        if ((b & 64) == 0)
        {
            if (!chan.Break_Sample_Loop)
                chan.Position_In_Sample = chan.Loop_Sample_Position;
            else
            {
                chan.Break_Sample_Loop = false;
                if ((b & 32) == 0)
                    chan.Enabled = false;
                chan.Position_In_Sample++;
            }
        }
        else
        {
            if ((b & 32) == 0)
                chan.Enabled = false;
            chan.Position_In_Sample++;
        }
    }
    else
        chan.Amplitude = 0;
    mixer >>= 1;
}
