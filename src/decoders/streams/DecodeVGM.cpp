#include "DecodeVGM.h"
#include "zlib/zlib.h"
#include "decoders/chipsims/SimAY8910.h"
#include "decoders/chipsims/SimRP2A03.h"
#include "decoders/chipsims/SimSN76489.h"

namespace
{
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

/// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ ///

bool DecodeVGM::Open(Stream& stream)
{
    Header header;
    if (ReadFile(stream.file.string().c_str(), (uint8_t*)(&header), sizeof(header)))
    {
        if (header.version >= 0x151 && header.ay8910Clock) m_chip.reset(new SimAY8910());
        if (header.version >= 0x161 && header.nesApuClock) m_chip.reset(new SimRP2A03());

        if (header.ident == 0x206D6756 && m_chip)
        {
            int fileSize = 0x04 + header.eofOffset;
            m_data = new uint8_t[fileSize];

            if (ReadFile(stream.file.string().c_str(), m_data, fileSize))
            {
                uint32_t vgmDataOffset = 0x40;
                if (header.version >= 0x150 && header.vgmDataOffset)
                {
                    vgmDataOffset = header.vgmDataOffset + 0x34;
                }
                m_dataPtr = m_data + vgmDataOffset;

                int frameRate = 60;// DetectFrameRate();
                stream.playback.frameRate(frameRate);
                stream.info.type("VGM stream");

                if (m_chip->type() == ChipSim::Type::AY8910)
                {
                    m_isTS = bool(header.ay8910Clock & 0x40000000);
                    auto divider = bool(header.ay8910Flags & 0x10);
                    auto ym_chip = bool(header.ay8910Type  & 0xF0);
                    stream.chip.model(ym_chip ? Chip::Model::YM : Chip::Model::AY);
                    stream.chip.count(m_isTS  ? Chip::Count::TurboSound : Chip::Count::SingleChip);
                    stream.chip.freqValue((header.ay8910Clock & 0x3FFFFFFF) / (divider ? 2 : 1));
                }

                else if (m_chip->type() == ChipSim::Type::RP2A03)
                {
                    m_isTS = true;
                    stream.chip.model(Chip::Model::YM);
                    stream.chip.count(m_isTS ? Chip::Count::TurboSound : Chip::Count::SingleChip);
                    stream.chip.freqValue(header.nesApuClock & 0x3FFFFFFF);
                }

                m_samplesPerFrame = (44100 / frameRate);
                m_processedSamples = 0;

                if (header.loopSamples)
                {
                    m_loop = (header.totalSamples - header.loopSamples) / m_samplesPerFrame;
                }
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
    if (m_processedSamples >= m_samplesPerFrame)
    {
        m_chip->Simulate(m_samplesPerFrame);
    }

    else while (m_processedSamples < m_samplesPerFrame)
    {
        if (int samples = DecodeBlock())
        {
            m_chip->Simulate(samples);
            m_processedSamples += samples;
        }
        else
        {
            return false;
        }
    }

    m_chip->ConvertToPSG(frame);
    m_processedSamples -= m_samplesPerFrame;
    return true;
}

void DecodeVGM::Close(Stream& stream)
{
    if (m_loop) stream.loop.frameId(m_loop);
    delete[] m_data;

    m_chip->PostProcess(stream);
}

/// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ ///

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

        // AY8910, write value dd to register aa
        else if (m_dataPtr[0] == 0xA0)
        {
            if (m_chip->type() == ChipSim::Type::AY8910)
            {
                uint8_t aa = m_dataPtr[1];
                uint8_t dd = m_dataPtr[2];
                m_chip->Write((aa & 0x80) >> 7, aa & 0x7F, dd);
            }
            m_dataPtr += 2;
        }

        // RP2A03, write value dd to register aa
        else if (m_dataPtr[0] == 0xB4)
        {
            if (m_chip->type() == ChipSim::Type::RP2A03)
            {
                uint8_t aa = m_dataPtr[1];
                uint8_t dd = m_dataPtr[2];
                m_chip->Write(0, aa, dd);
            }
            m_dataPtr += 2;
        }

        // Data block 0x67 0x66 tt ss ss ss ss, just skip it
        else if (m_dataPtr[0] == 0x67)
        {
            uint32_t dataLength = *(uint32_t*)(&m_dataPtr[3]);
            m_dataPtr += (6 + dataLength);
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
    return samples;
}
