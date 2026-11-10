#include "DecodeVGM.h"
#include <sstream>
#include <algorithm>
#include <zlib/zlib.h>
#include "simulation/SimAY8910.h"
#include "simulation/SimRP2A03.h"
#include "simulation/SimSN76489.h"
#include "simulation/SimHybrid.h"
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

        // Chips that have a simulator, in the order of priority.
        std::vector<ChipSim::Type> chips;
        if (header.version >= 0x161 && header.nesApuClock) chips.push_back(ChipSim::Type::RP2A03);
        if (header.sn76489Clock) chips.push_back(ChipSim::Type::SN76489);
        if (header.version >= 0x151 && header.ay8910Clock) chips.push_back(ChipSim::Type::AY8910);

        // Two different chips play on two PSGs, one simulator on each PSG. Reject
        // them if the user selects one PSG or if a dual chip needs a third PSG.
        bool oneChosen = (stream.dchip.first.model != Chip::Model::Compatible && !stream.dchip.second.modelKnown);
        bool hybrid = (chips.size() == 2);
        bool playable = (chips.size() == 1 || (hybrid && !oneChosen && !HasDualChips(header)));

        if (header.ident == VGMSignature && playable && !HasOtherChips(header))
        {
            if (hybrid)
            {
                std::shared_ptr<ChipSim> sims[2];
                for (int psg = 0; psg < 2; ++psg) sims[psg] = CreateSimulator(chips[psg]);
                m_simulator.reset(new SimHybrid(sims[0], sims[1]));
            }
            else
            {
                m_simulator = CreateSimulator(chips[0]);
            }

            int fileSize = 0x04 + header.eofOffset;
            m_data = new uint8_t[fileSize];

            if (ReadFile(stream.file.string().c_str(), m_data, fileSize))
            {
                m_dataPtr = m_data + dataOffset;

                int clockRate = 0;
                int frameRate = 60;

                if (hybrid)
                {
                    // Both PSGs use the destination clock, the ZX Spectrum clock by default.
                    stream.schip.clock = (stream.dchip.clockKnown ? stream.dchip.clock : Chip::Clock::F1773400);
                    stream.schip.output = (stream.dchip.outputKnown ? stream.dchip.output : Chip::Output::Mono);

                    // Each simulator plays its chip on one PSG of the destination model.
                    for (int psg = 0; psg < 2; ++psg)
                    {
                        ChipSim* sim = m_simulator->Find(chips[psg]);
                        Chip::Model model = stream.dchip.model(psg);
                        bool compatible = (model == Chip::Model::Compatible || model == Chip::Model::Unknown);
                        int clock = 0;

                        if (chips[psg] == ChipSim::Type::SN76489)
                        {
                            clock = (header.sn76489Clock & 0x3FFFFFFF);
                            if (compatible) model = Chip::Model::AY8910;
                        }
                        else if (chips[psg] == ChipSim::Type::AY8910)
                        {
                            clock = (header.ay8910Clock & 0x3FFFFFFF);
                            if (header.ay8910Flags & 0x10) clock /= 2;
                            if (compatible) model = AYModel(header);
                        }
                        else if (chips[psg] == ChipSim::Type::RP2A03)
                        {
                            clock = (header.nesApuClock & 0x3FFFFFFF);
                            if (clock / 1000 == 1662 || clock / 1000 == 1773) frameRate = 50;
                            if (compatible) model = Chip::Model::YM2149;

                            auto outputType = (model == Chip::Model::AY8930 ?
                                SimRP2A03::OutputType::AY8930Chip : SimRP2A03::OutputType::SingleChip);
                            static_cast<SimRP2A03*>(sim)->ConfigureOutput(outputType, model);
                        }

                        if (psg) stream.schip.second.model = model;
                        else stream.schip.first.model = model;
                        sim->ConfigureClock(clock, stream.schip.clockValue);
                    }
                }

                else if (m_simulator->type() == ChipSim::Type::SN76489)
                {
                    clockRate = (header.sn76489Clock & 0x3FFFFFFF);
                    auto twoChips = bool(header.sn76489Clock & 0x40000000);

                    Chip::Model model = Chip::Model::AY8910;
                    stream.schip.first.model = model;
                    stream.schip.clockValue = clockRate / 2;
                    if (twoChips) stream.schip.second.model = model;
                    //stream.schip.output(Chip::Output::Mono);

                    // TODO
                }
                
                else if (m_simulator->type() == ChipSim::Type::AY8910)
                {
                    clockRate = (header.ay8910Clock & 0x3FFFFFFF);
                    if (header.ay8910Flags & 0x10) clockRate /= 2;
                    // TODO: detect frame rate

                    // detect chip model and count
                    Chip::Model model = AYModel(header);
                    auto twoChips = bool(header.ay8910Clock & 0x40000000);
          
                    // setup chip model, clock rate and 2nd chip if needed
                    stream.schip.first.model = model;
                    stream.schip.clockValue = clockRate;
                    if (twoChips) stream.schip.second.model = model;

                    // ZX Spectrum like pan configuration (left, center, right),
                    // otherwise the outputs of all channels are mixed together
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

                else if (m_simulator->type() == ChipSim::Type::RP2A03)
                {
                    clockRate = (header.nesApuClock & 0x3FFFFFFF);
                    // PAL (1662607 Hz) and Dendy (1773448 Hz) play at 50 Hz
                    if (clockRate / 1000 == 1662 || clockRate / 1000 == 1773) frameRate = 50;

                    // setup chip clock rate (ZX Spectrum clock used by default)
                    if (stream.dchip.clockKnown)
                        stream.schip.clock = stream.dchip.clock;
                    else
                        stream.schip.clock = Chip::Clock::F1773400;

                    // setup first chip (YM2149 used by default)
                    if (stream.dchip.first.model != Chip::Model::Compatible)
                        stream.schip.first.model = stream.dchip.first.model;
                    else
                        stream.schip.first.model = Chip::Model::YM2149;

                    // setup output Strereo or Mono (Mono used by default)
                    if (stream.dchip.outputKnown)
                        stream.schip.output = stream.dchip.output;
                    else
                        stream.schip.output = Chip::Output::Mono;

                    // configure simulator output type and update chip if required
                    auto outputType = SimRP2A03::OutputType::SingleChip;
                    if (stream.dchip.first.model == Chip::Model::AY8930)
                    {
                        outputType = SimRP2A03::OutputType::AY8930Chip;
                        stream.schip.first.model = Chip::Model::AY8930;
                        if (stream.dchip.count == 2 && stream.dchip.second.model == Chip::Model::AY8930)
                        {
                            // two AY8930: the 2nd one plays the DMC
                            outputType = SimRP2A03::OutputType::AY8930DoubleChip;
                            stream.schip.second.model = Chip::Model::AY8930;
                        }
                    }
                    else if (stream.dchip.count == 2)
                    {
                        outputType = SimRP2A03::OutputType::DoubleChip;
                        stream.schip.second.model = stream.schip.first.model;
                    }
                    static_cast<SimRP2A03*>(m_simulator.get())->ConfigureOutput(outputType, stream.schip.first.model);
                }

                // use the recording rate when it is known
                if (header.version >= 0x101 && header.rate)
                {
                    frameRate = header.rate;
                }

                stream.info.type = "VGM stream";
                stream.play.frameRate = frameRate;

                if (!hybrid) m_simulator->ConfigureClock(clockRate, stream.schip.clockValue);
                m_samplesPerFrame = (44100 / frameRate);
                m_waitSamples = 0;

                m_loop = 0;
                m_loopTime = (header.loopSamples ? int(header.totalSamples - header.loopSamples) : -1);
                m_time = 0;
                m_frames = 0;
                m_writes = 0;
                m_written = false;
                m_sinceWrite = 0;
                m_sinceFrame = m_samplesPerFrame;
                m_burstTime = 0;
                m_updateTime = -1;
                m_updateTimes.assign(2 * m_samplesPerFrame, 0);
                m_updates = 0;

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
    // a player writes the registers once per frame, in a burst with short pauses inside,
    // and waits for the next frame: a frame is taken at the start of the long pause after
    // a burst, as the player has just set it, and every frame while nothing is written;
    // a burst too close to the last frame is a late part of its update
    const int longPause = (m_samplesPerFrame / 4);
    const int noUpdate = (m_samplesPerFrame + m_samplesPerFrame / 2);

    while (true)
    {
        if (!m_waitSamples)
        {
            int writes = m_writes;
            if (!(m_waitSamples = DecodeBlock())) return false;
            if (m_writes != writes)
            {
                if (!m_written) m_burstTime = m_time;
                m_written = true;
                m_sinceWrite = 0;
            }
        }

        bool update = (m_written && m_sinceWrite + m_waitSamples >= longPause);
        if (update && m_sinceFrame < longPause) update = m_written = false;

        if (update || m_sinceFrame >= noUpdate)
        {
            // count the times between the updates in a row, from the start of
            // a burst: the player starts it at the frame, its length varies
            if (update && m_updateTime >= 0 && m_burstTime - m_updateTime <= noUpdate)
            {
                m_updateTimes[m_burstTime - m_updateTime]++;
                m_updates++;
            }
            m_updateTime = (update ? m_burstTime : -1);

            m_simulator->Convert(frame);
            m_sinceFrame = (update ? 0 : m_sinceFrame - m_samplesPerFrame);
            m_written = false;

            if (m_loopTime >= 0 && m_time >= m_loopTime)
            {
                m_loop = m_frames;
                m_loopTime = -1;
            }
            ++m_frames;
            return true;
        }

        int samples = std::min(m_waitSamples, noUpdate - m_sinceFrame);
        m_simulator->Simulate(samples);
        m_waitSamples -= samples;
        m_sinceWrite  += samples;
        m_sinceFrame  += samples;
        m_time        += samples;
    }
}

void DecodeVGM::Close(Stream& stream)
{
    // The most common time between the updates in a row is the frame time, if most of
    // the updates come at about this time. The mean of these times, which vary by a few
    // samples, gives the real frame rate instead of the 50 or 60 Hz of the header.
    int mode = int(std::max_element(m_updateTimes.begin(), m_updateTimes.end()) - m_updateTimes.begin());
    int count = 0;
    double time = 0;
    for (int t = std::max(mode - 2, 0); t <= mode + 2 && t < int(m_updateTimes.size()); ++t)
    {
        count += m_updateTimes[t];
        time += double(t) * m_updateTimes[t];
    }
    if (count >= 50 && 2 * count >= m_updates)
    {
        stream.play.frameRate = (44100 * count / time);
    }
    stream.Finalize(m_loop);
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
        header.pwmClock, header.gbDmgClock, header.multiPcmClock, header.upd7759Clock,
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
    return ((header.sn76489Clock | header.ay8910Clock | header.nesApuClock) & 0x40000000);
}

Chip::Model DecodeVGM::AYModel(const Header& header)
{
    if (header.ay8910Type == 0x03) return Chip::Model::AY8930;
    if (header.ay8910Type >= 0x10) return Chip::Model::YM2149;
    return Chip::Model::AY8910;
}

std::shared_ptr<ChipSim> DecodeVGM::CreateSimulator(ChipSim::Type type)
{
    switch (type)
    {
    case ChipSim::Type::SN76489: return std::make_shared<SimSN76489>();
    case ChipSim::Type::AY8910:  return std::make_shared<SimAY8910>();
    case ChipSim::Type::RP2A03:  return std::make_shared<SimRP2A03>();
    default: return nullptr;
    }
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
