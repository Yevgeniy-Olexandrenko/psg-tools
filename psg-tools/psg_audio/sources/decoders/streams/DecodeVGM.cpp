#include "DecodeVGM.h"
#include <sstream>
#include <algorithm>
#include <zlib/zlib.h>
#include "simulation/SimFactory.h"
#include "debug/DebugOutput.h"

static DebugOutput<DBG_DECODE_VGM> dbg;

////////////////////////////////////////////////////////////////////////////////

namespace
{
    const uint32_t VGMSignature = 0x206D6756;
    const uint32_t GD3Signature = 0x20336447;

    bool ReadFile(const char* path, uint8_t* dest, int size)
    {
        if (path && dest && size)
        {
            int read = 0;
            if (gzFile file = gzopen(path, "rb"))
            {
                read = gzread(file, dest, size);
                gzclose(file);
            }
            return (read == size);

        }
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////////

bool DecodeVGM::Open(Stream& stream)
{
    Header header;
    if (ReadFile(stream.file.string().c_str(), (uint8_t*)(&header), sizeof(header)))
    {
        uint32_t dataOffset = 0x40;
        if (header.version >= 0x150 && header.vgmDataOffset)
        {
            dataOffset = header.vgmDataOffset + 0x34;
        }

        // Treat as 0 the fields of later versions, the fields at or
        // after the data start and the fields in the extra header.
        uint32_t headerSize = sizeof(header);
        if (header.version < 0x151) headerSize = 0x38;
        else if (header.version < 0x161) headerSize = 0x80;
        else if (header.version < 0x170) headerSize = 0xBC;
        else if (header.version < 0x171) headerSize = 0xC0;
        if (header.version >= 0x170 && header.extraHeaderOffset)
        {
            headerSize = std::min(headerSize, 0xBC + header.extraHeaderOffset);
        }
        headerSize = std::min(headerSize, dataOffset);
        if (headerSize < sizeof(header))
        {
            memset((uint8_t*)(&header) + headerSize, 0, sizeof(header) - headerSize);
        }

        // Chips that have a simulator, in the order of priority (see SimFactory).
        // PAL (1662607 Hz) and Dendy (1773448 Hz) NES play at 50 Hz.
        std::vector<SimFactory::Source> chips;
        int frameRate = 60;
        if (header.version >= 0x161 && header.nesApuClock)
        {
            int clock = (header.nesApuClock & 0x3FFFFFFF);
            if (clock / 1000 == 1662 || clock / 1000 == 1773) frameRate = 50;
            chips.push_back({ ChipSim::Type::RP2A03, clock });
        }
        if (header.sn76489Clock)
        {
            int clock = (header.sn76489Clock & 0x3FFFFFFF);
            chips.push_back({ ChipSim::Type::SN76489, clock, Chip::Model::Unknown, bool(header.sn76489Clock & 0x40000000) });
        }
        if (header.version >= 0x161 && header.gbDmgClock)
        {
            chips.push_back({ ChipSim::Type::LR35902, int(header.gbDmgClock & 0x3FFFFFFF) });
        }
        if (header.version >= 0x151 && header.ay8910Clock)
        {
            int clock = (header.ay8910Clock & 0x3FFFFFFF);
            if (header.ay8910Flags & 0x10) clock /= 2;
            chips.push_back({ ChipSim::Type::AY8910, clock, AYModel(header), bool(header.ay8910Clock & 0x40000000) });
        }

        // Two different chips play on two PSGs, one simulator on each PSG. Reject
        // them if a dual chip needs a third PSG, or if the user selects one PSG.
        bool playable = (chips.size() == 1 || (chips.size() == 2 && !HasDualChips(header)));

        if (header.ident == VGMSignature && playable && !HasOtherChips(header)
            && (m_simulator = SimFactory::Create(chips, stream)))
        {
            int fileSize = 0x04 + header.eofOffset;
            m_data = new uint8_t[fileSize];

            if (ReadFile(stream.file.string().c_str(), m_data, fileSize))
            {
                m_dataPtr = m_data + dataOffset;

                // A single AY8910: ZX Spectrum like pan configuration (left, center,
                // right), otherwise the outputs of all channels are mixed together.
                if (chips.size() == 1 && chips[0].type == ChipSim::Type::AY8910)
                {
                    if (header.ay8910Flags & 0x80)
                    {
                        stream.schip.output = Chip::Output::Stereo;
                        stream.schip.stereo = Chip::Stereo::ABC;
                    }
                    else
                    {
                        stream.schip.output = Chip::Output::Mono;
                    }
                }

                // use the recording rate when it is known
                if (header.version >= 0x101 && header.rate)
                {
                    frameRate = header.rate;
                }

                stream.info.type = "VGM stream";
                stream.play.frameRate = frameRate;

                m_cutter.reset(new FrameCutter(44100 / frameRate));
                m_waitSamples = 0;

                m_loop = 0;
                m_loopTime = (header.loopSamples ? int(header.totalSamples - header.loopSamples) : -1);
                m_frames = 0;
                m_writes = 0;

                // GD3 tags reading
                if (uint8_t* gd3Offset = m_data + header.gd3Offset)
                {
                    uint32_t gd3 = *(uint32_t*)(gd3Offset + 0x14);
                    uint32_t ver = *(uint32_t*)(gd3Offset + 0x18);

                    if (gd3 == GD3Signature && ver == 0x00000100)
                    {
                        gd3Offset += 0x20;

                        auto ReadString = [&]() -> std::string
                        {
                            std::stringstream ss;
                            while (true)
                            {
                                wchar_t ch = *(wchar_t*)gd3Offset;
                                gd3Offset += sizeof(wchar_t);

                                if (!ch) break;
                                ss << (ch < 0x20 || ch > 0x7F ? '?' : char(ch));
                            }
                            return ss.str();
                        };

                        std::string GD3[]{
                            ReadString(), ReadString(), // Track name
                            ReadString(), ReadString(), // Game name
                            ReadString(), ReadString(), // System name
                            ReadString(), ReadString(), // Name of Original Track Author
                            ReadString(),               // Date of game's release written in the form yyyy/mm/dd, or just yyyy/mm or yyyy
                            ReadString(),               // Name of person who converted it to a VGM file
                            ReadString()                // Notes
                        };

                        stream.info.title = GD3[0];
                        stream.info.artist = GD3[6];
                        // EncodeVGM writes the comment to the notes, not to the game name
                        stream.info.comment = (GD3[2].empty() ? GD3[10] : GD3[2]);

                        if (!GD3[4].empty())
                            stream.info.type = (std::string)stream.info.type + " (" + GD3[4] + ")";
                    }
                }
                dbg.open("decode_vgm_" + stream.ToString(Stream::Property::Tag));
                return true;
            }
            else
            {
                delete[] m_data;
            }
        }
    }
    return false;
}

bool DecodeVGM::Decode(Frame& frame)
{
    // the frames are cut at the updates of the player (see FrameCutter)
    while (true)
    {
        if (!m_waitSamples)
        {
            int writes = m_writes;
            if (!(m_waitSamples = DecodeBlock())) return false;
            if (m_writes != writes) m_cutter->OnWrite();
        }

        if (m_cutter->IsFrameEnd(m_waitSamples))
        {
            m_simulator->Convert(frame);
            if (m_loopTime >= 0 && m_cutter->GetTime() >= m_loopTime)
            {
                m_loop = m_frames;
                m_loopTime = -1;
            }
            ++m_frames;
            return true;
        }

        int samples = m_cutter->Advance(m_waitSamples);
        m_simulator->Simulate(samples);
        m_waitSamples -= samples;
    }
}

void DecodeVGM::Close(Stream& stream)
{
    // the rate of the updates instead of the 50 or 60 Hz of the header
    FrameRate frameRate;
    if (m_cutter->GetFrameRate(frameRate)) stream.play.frameRate = frameRate;
    stream.Finalize(m_loop, true);
    delete[] m_data;
    dbg.close();
}

////////////////////////////////////////////////////////////////////////////////

bool DecodeVGM::HasOtherChips(const Header& header)
{
    // Chips without a simulator. Bit 31 selects a variant without a
    // simulator: T6W28 for SN76489, FDS for the NES APU.
    const uint32_t clocks[] =
    {
        header.ym2413Clock, header.ym2612Clock, header.ym2151Clock, header.segaPcmClock,
        header.rf5c68Clock, header.ym2203Clock, header.ym2608Clock, header.ym2610bClock,
        header.ym3812Clock, header.ym3526Clock, header.y8950Clock, header.ymf262Clock,
        header.ymf278bClock, header.ymf271Clock, header.ymz280bClock, header.rf5c164Clock,
        header.pwmClock, header.multiPcmClock, header.upd7759Clock,
        header.okim6258Clock, header.oki6295Clock, header.k051649Clock, header.k054539Clock,
        header.huc6280Clock, header.c140Clock, header.k053260Clock, header.pokeyClock,
        header.qsoundClock, header.scspClock, header.wswanClock, header.vsuClock,
        header.saa1090Clock, header.es5503Clock, header.es5506Clock, header.x1010Clock,
        header.c352Clock, header.ga20Clock,
        header.sn76489Clock & 0x80000000, header.nesApuClock & 0x80000000
    };
    for (uint32_t clock : clocks)
    {
        if (clock) return true;
    }

    // Clocks of the chips of later format versions.
    for (uint8_t byte : header.reserved4)
    {
        if (byte) return true;
    }
    return false;
}

bool DecodeVGM::HasDualChips(const Header& header)
{
    return ((header.sn76489Clock | header.ay8910Clock | header.nesApuClock | header.gbDmgClock) & 0x40000000);
}

Chip::Model DecodeVGM::AYModel(const Header& header)
{
    if (header.ay8910Type == 0x03) return Chip::Model::AY8930;
    if (header.ay8910Type >= 0x10) return Chip::Model::YM2149;
    return Chip::Model::AY8910;
}

int DecodeVGM::CommandSize(uint8_t command)
{
    // the size of the operands of a command, 0 for an unknown command
    if (command >= 0x30 && command <= 0x3F) return 1;
    if (command >= 0x40 && command <= 0x4E) return 2;
    if (command >= 0x4F && command <= 0x50) return 1;
    if (command >= 0x51 && command <= 0x5F) return 2;
    if (command == 0x68) return 11;
    if (command == 0x90 || command == 0x91 || command == 0x95) return 4;
    if (command == 0x92) return 5;
    if (command == 0x93) return 10;
    if (command == 0x94) return 1;
    if (command >= 0xA0 && command <= 0xBF) return 2;
    if (command >= 0xC0 && command <= 0xDF) return 3;
    if (command >= 0xE0) return 4;
    return 0;
}

int DecodeVGM::DecodeBlock()
{
    int samples = 0;
    while (!samples)
    {
        // YM2203 or AY8910 stereo mask
        if (m_dataPtr[0] == 0x31)
        {
            uint8_t dd = m_dataPtr[1];

            // b7.b6.b5.b4.b3.b2.b1.b0
            // II.YY.R3.L3.R2.L2.R1.L1
            // -----------------------
            // II       chip instance(0 or 1)
            // YY       set stereo mask for YM2203 (1) or AY8910 (0)
            // L1/L2/L3 enable channel 1/2/3 on left speaker
            // R1/R2/R3 enable channel 1/2/3 on right speaker

            m_dataPtr += 1;
        }

        // Game Gear PSG stereo, write dd to port 0x06. Not simulated yet: skip it.
        else if (m_dataPtr[0] == 0x4F)
        {
            m_dataPtr += 1;
        }

        // SN76489, write value dd
        else if (m_dataPtr[0] == 0x50 || m_dataPtr[0] == 0x30)
        {
            if (ChipSim* sim = m_simulator->Find(ChipSim::Type::SN76489))
            {
                uint8_t aa = m_dataPtr[0];
                uint8_t dd = m_dataPtr[1];
                sim->Write((aa & 0x20) >> 5, 0, dd);
                ++m_writes;
                dbg.print_message("r%02X:%02X ", aa, dd);
            }
            m_dataPtr += 1;
        }

        // AY8910, write value dd to register aa
        else if (m_dataPtr[0] == 0xA0)
        {
            if (ChipSim* sim = m_simulator->Find(ChipSim::Type::AY8910))
            {
                uint8_t aa = m_dataPtr[1];
                uint8_t dd = m_dataPtr[2];
                sim->Write((aa & 0x80) >> 7, aa & 0x7F, dd);
                ++m_writes;
                dbg.print_message("r%02X:%02X ", aa, dd);
            }
            m_dataPtr += 2;
        }

        // Game Boy DMG, write value dd to register aa
        else if (m_dataPtr[0] == 0xB3)
        {
            if (ChipSim* sim = m_simulator->Find(ChipSim::Type::LR35902))
            {
                uint8_t aa = m_dataPtr[1];
                uint8_t dd = m_dataPtr[2];
                sim->Write((aa & 0x80) >> 7, aa & 0x7F, dd);
                ++m_writes;
                dbg.print_message("r%02X:%02X ", aa, dd);
            }
            m_dataPtr += 2;
        }

        // RP2A03, write value dd to register aa
        else if (m_dataPtr[0] == 0xB4)
        {
            if (ChipSim* sim = m_simulator->Find(ChipSim::Type::RP2A03))
            {
                uint8_t aa = m_dataPtr[1];
                uint8_t dd = m_dataPtr[2];
                sim->Write(0, aa, dd);
                ++m_writes;
                dbg.print_message("r%02X:%02X ", aa, dd);
            }
            m_dataPtr += 2;
        }

        // Data block 0x67 0x66 tt ss ss ss ss, the NES APU RAM (tt = C2) is loaded
        // as aa aa dd...: the samples of the DMC at address aaaa
        else if (m_dataPtr[0] == 0x67)
        {
            uint32_t dataSize = *(uint32_t*)(&m_dataPtr[3]) - 2;
            ChipSim* sim = m_simulator->Find(ChipSim::Type::RP2A03);
            if (sim && m_dataPtr[2] == 0xC2)
            {
                uint16_t dataAddr = *(uint16_t*)(&m_dataPtr[7]);
                sim->WriteMemory(dataAddr, &m_dataPtr[9], dataSize);
                dbg.print_message("d%04X:%05d ", dataAddr, dataSize);
            }
            m_dataPtr += (6 + dataSize + 2);
        }

        // Wait n samples, n can range from 0 to 65535 (approx 1.49 seconds).
        // Longer pauses than this are represented by multiple wait commands.
        else if (m_dataPtr[0] == 0x61)
        {
            samples = *(uint16_t*)(&m_dataPtr[1]);
            m_dataPtr += 2;
        }

        // Wait 735 samples (60th of a second), a shortcut for 0x61 0xdf 0x02
        else if (m_dataPtr[0] == 0x62)
        {
            samples = 735;
        }

        // Wait 882 samples (50th of a second), a shortcut for 0x61 0x72 0x03
        else if (m_dataPtr[0] == 0x63)
        {
            samples = 882;
        }

        // End of sound data
        else if (m_dataPtr[0] == 0x66)
        {
            break;
        }

        // Wait n+1 samples, n can range from 0 to 15.
        else if ((m_dataPtr[0] & 0xF0) == 0x70)
        {
            samples = (1 + (m_dataPtr[0] & 0x0F));
        }

        // YM2612 port 0 address 2A write from the data bank, then wait n samples
        else if ((m_dataPtr[0] & 0xF0) == 0x80)
        {
            samples = (m_dataPtr[0] & 0x0F);
        }

        // Skip the other commands of known size: writes to chips without a simulator,
        // DAC stream control (Furnace adds it to AY8930 VGMs), PCM RAM writes, seeks
        else if (int size = CommandSize(m_dataPtr[0]))
        {
            m_dataPtr += size;
        }

        // Unknown command, stop decoding
        else
        {
            break;
        }
        m_dataPtr++;
    }
    dbg.print_message("w%05d ", samples);
    return samples;
}
