// nes2wav: helper for A/B listening of the NES -> PSG conversion of psg-tools (build-nes2wav.bat).
// All written WAV files are mono, at one loudness (ITU-R BS.1770), with a 15 Hz high-pass.
//   render [--dmc] <vgm> <out.wav>   the NES APU part of a VGM through the RP2A03 emulator, DMC muted
//   level [--mono] <file.wav>...     scales 16-bit WAV files in place, --mono mixes them down
//   scan <vgm>...                    prints the NES features each VGM uses
//   tri <vgm> <out prefix>           the triangle alone: through the APU and YM2149 variants
//   chan <vgm> <out prefix> [AY8930] the conversion as psg-tools does it, channels A, B, C alone
//                                    (no files for an empty prefix), and its level steps per frame
//   notri <vgm> <out.wav> [2A03|YM2149|AY8930]  without the triangle: the 2A03, or the conversion
//                                    with channel B muted while it plays the envelope
//   nlevel [AY8930] <vgm>...         the noise level of the conversion against the NES one per frame

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <memory>
#include <zlib/zlib.h>
#include "emulation/RP2A03Apu.h"
#include "emulation/AY89XXPsg.h"
#include "simulation/SimRP2A03.h"

namespace
{
    const int    SampleRate      = 44100;
    const int    SamplesPerFrame = SampleRate / 60;
    const int    ChipClock       = 1773400;
    const double TargetLoudness  = -24.0; // LUFS, low enough that the peaks do not limit the gain
    const double MaxPeak         = 0.98;
    const double Pi              = 3.14159265358979323846;

    bool LoadFile(const char* path, std::vector<uint8_t>& data, bool gz)
    {
        if (gz)
        {
            gzFile f = gzopen(path, "rb");
            if (!f) return false;
            uint8_t buf[65536]; int n;
            while ((n = gzread(f, buf, sizeof(buf))) > 0) data.insert(data.end(), buf, buf + n);
            gzclose(f);
        }
        else
        {
            FILE* f = fopen(path, "rb");
            if (!f) return false;
            uint8_t buf[65536]; size_t n;
            while ((n = fread(buf, 1, sizeof(buf), f)) > 0) data.insert(data.end(), buf, buf + n);
            fclose(f);
        }
        return !data.empty();
    }

    uint32_t U32(const std::vector<uint8_t>& d, size_t o)
    {
        return (o + 4 <= d.size() ? d[o] | d[o + 1] << 8 | d[o + 2] << 16 | uint32_t(d[o + 3]) << 24 : 0);
    }

    struct Vgm
    {
        std::vector<uint8_t> d;
        size_t   start = 0;
        uint32_t nesClock = 0;
        uint32_t totalSamples = 0;

        bool Open(const char* path)
        {
            if (!LoadFile(path, d, true) || U32(d, 0) != 0x206D6756) return false;
            uint32_t version = U32(d, 0x08);
            start = 0x40;
            if (version >= 0x150 && U32(d, 0x34)) start = 0x34 + U32(d, 0x34);
            nesClock = (version >= 0x161 && start >= 0x88 ? U32(d, 0x84) & 0x3FFFFFFF : 0);
            totalSamples = U32(d, 0x18);
            return nesClock != 0;
        }
    };

    template<typename W, typename B, typename S>
    void Parse(const Vgm& vgm, W onWrite, B onBlock, S onWait)
    {
        const auto& d = vgm.d;
        size_t p = vgm.start;
        while (p < d.size())
        {
            uint8_t cmd = d[p];
            if (cmd == 0x66) break;
            else if (cmd == 0xB4) { if (p + 2 < d.size()) onWrite(d[p + 1], d[p + 2]); p += 3; }
            else if (cmd == 0x61) { if (p + 2 < d.size()) onWait(d[p + 1] | d[p + 2] << 8); p += 3; }
            else if (cmd == 0x62) { onWait(735); p += 1; }
            else if (cmd == 0x63) { onWait(882); p += 1; }
            else if ((cmd & 0xF0) == 0x70) { onWait((cmd & 0x0F) + 1); p += 1; }
            else if ((cmd & 0xF0) == 0x80) { onWait(cmd & 0x0F); p += 1; }
            else if (cmd == 0x67)
            {
                if (p + 7 > d.size()) break;
                uint8_t  type = d[p + 2];
                uint32_t size = U32(d, p + 3) & 0x7FFFFFFF;
                if (p + 7 + size > d.size()) break;
                onBlock(type, &d[p + 7], size);
                p += 7 + size;
            }
            else if (cmd == 0x68) p += 12;
            else if (cmd >= 0x30 && cmd <= 0x3F) p += 2;
            else if (cmd >= 0x40 && cmd <= 0x4E) p += 3;
            else if (cmd == 0x4F || cmd == 0x50) p += 2;
            else if (cmd >= 0x51 && cmd <= 0x5F) p += 3;
            else if (cmd == 0x90 || cmd == 0x91 || cmd == 0x95) p += 5;
            else if (cmd == 0x92) p += 6;
            else if (cmd == 0x93) p += 11;
            else if (cmd == 0x94) p += 2;
            else if (cmd >= 0xA0 && cmd <= 0xBF) p += 3;
            else if (cmd >= 0xC0 && cmd <= 0xDF) p += 4;
            else if (cmd >= 0xE0) p += 5;
            else { fprintf(stderr, "unknown command %02X at %zX\n", cmd, p); break; }
        }
    }

    // NES APU writes with the DMC muted, its sample data blocks are not loaded either
    void WriteWithoutDmc(RP2A03Apu& apu, uint8_t aa, uint8_t dd)
    {
        if (aa >= 0x20 || (aa >= 0x10 && aa <= 0x13)) return;
        apu.Write(0x4000 | aa, aa == 0x15 ? dd & ~0x10 : dd);
    }

    ////////////////////////////////////////////////////////////////////////////

    // removes DC from the unipolar outputs of the chips
    struct HighPass
    {
        double r = std::exp(-2 * Pi * 15 / SampleRate);
        double x1 = 0, y1 = 0;

        float Run(double x)
        {
            y1 = x - x1 + r * y1; x1 = x;
            return float(y1);
        }
    };

    struct Biquad
    {
        double b0, b1, b2, a1, a2;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        double Run(double x)
        {
            double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };

    // K-weighting of ITU-R BS.1770: high shelf + high-pass, designed for any sample rate
    void KWeighting(Biquad& shelf, Biquad& highpass)
    {
        double A  = std::pow(10.0, 3.999843853973347 / 40);
        double w0 = 2 * Pi * 1681.974450955533 / SampleRate;
        double al = std::sin(w0) / (2 * 0.7071752369554196);
        double c  = std::cos(w0), sa = 2 * std::sqrt(A) * al;
        double a0 = (A + 1) - (A - 1) * c + sa;
        shelf.b0 = A * ((A + 1) + (A - 1) * c + sa) / a0;
        shelf.b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
        shelf.b2 = A * ((A + 1) + (A - 1) * c - sa) / a0;
        shelf.a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
        shelf.a2 = ((A + 1) - (A - 1) * c - sa) / a0;

        w0 = 2 * Pi * 38.13547087602444 / SampleRate;
        al = std::sin(w0) / (2 * 0.5003270373238773);
        c  = std::cos(w0);
        a0 = 1 + al;
        highpass.b0 = 1 / a0; highpass.b1 = -2 / a0; highpass.b2 = 1 / a0;
        highpass.a1 = -2 * c / a0; highpass.a2 = (1 - al) / a0;
    }

    // gated loudness in LUFS of interleaved samples, channels weighted equally
    double Loudness(const std::vector<float>& s, int channels)
    {
        size_t frames = s.size() / channels;
        std::vector<double> power(frames, 0);
        for (int ch = 0; ch < channels; ++ch)
        {
            Biquad shelf, highpass;
            KWeighting(shelf, highpass);
            for (size_t i = 0; i < frames; ++i)
            {
                double y = highpass.Run(shelf.Run(s[i * channels + ch]));
                power[i] += y * y / channels;
            }
        }

        const size_t block = SampleRate * 4 / 10, step = SampleRate / 10;
        std::vector<double> blocks;
        for (size_t i = 0; i + block <= frames; i += step)
        {
            double z = 0;
            for (size_t j = i; j < i + block; ++j) z += power[j];
            blocks.push_back(z / block);
        }

        const auto Lufs = [](double z) { return -0.691 + 10 * std::log10(std::max(z, 1e-20)); };
        const auto GatedMean = [&](double gate)
        {
            double sum = 0; int count = 0;
            for (double z : blocks) if (Lufs(z) > gate) { sum += z; ++count; }
            return (count ? sum / count : 0);
        };
        double relative = Lufs(GatedMean(-70)) - 10;
        return Lufs(GatedMean(std::max(-70.0, relative)));
    }

    // gain that brings the samples to the target loudness without clipping
    double LevelGain(const std::vector<float>& s, int channels, const char* name)
    {
        double loudness = Loudness(s, channels);
        double peak = 0;
        for (float x : s) peak = std::max(peak, double(std::abs(x)));
        if (peak <= 0) return 1;

        double gain = std::pow(10.0, (TargetLoudness - loudness) / 20);
        if (peak * gain > MaxPeak)
        {
            double limited = MaxPeak / peak;
            fprintf(stderr, "%s: peak limited, %.1f dB below the target loudness\n", name, 20 * std::log10(gain / limited));
            gain = limited;
        }
        return gain;
    }

    int16_t ToPcm(double x)
    {
        return int16_t(std::lround(std::max(-1.0, std::min(1.0, x)) * 32767));
    }

    void Put16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(uint8_t(x)); v.push_back(uint8_t(x >> 8)); }
    void Put32(std::vector<uint8_t>& v, uint32_t x) { Put16(v, uint16_t(x)); Put16(v, uint16_t(x >> 16)); }

    bool WriteWav(const char* path, const std::vector<float>& pcm, int channels, double gain)
    {
        std::vector<uint8_t> wav;
        uint32_t dataSize = uint32_t(pcm.size() * 2);
        wav.insert(wav.end(), { 'R','I','F','F' }); Put32(wav, 36 + dataSize);
        wav.insert(wav.end(), { 'W','A','V','E','f','m','t',' ' }); Put32(wav, 16);
        Put16(wav, 1); Put16(wav, uint16_t(channels)); Put32(wav, SampleRate); Put32(wav, SampleRate * 2 * channels);
        Put16(wav, uint16_t(2 * channels)); Put16(wav, 16);
        wav.insert(wav.end(), { 'd','a','t','a' }); Put32(wav, dataSize);
        for (float s : pcm) Put16(wav, uint16_t(ToPcm(s * gain)));

        FILE* f = fopen(path, "wb");
        if (!f) { fprintf(stderr, "%s: can't write\n", path); return false; }
        fwrite(wav.data(), 1, wav.size(), f);
        fclose(f);
        return true;
    }

    ////////////////////////////////////////////////////////////////////////////

    int Render(const char* in, const char* out, bool dmc, bool triangle = true)
    {
        Vgm vgm;
        if (!vgm.Open(in)) { fprintf(stderr, "%s: not a NES APU VGM\n", in); return 1; }

        RP2A03Apu apu;
        apu.Init(SampleRate, vgm.nesClock);

        std::vector<float> pcm;
        pcm.reserve(vgm.totalSamples);
        HighPass highPass;

        Parse(vgm,
            [&](uint8_t aa, uint8_t dd)
            {
                // the triangle is muted by never enabling it
                if (!triangle && aa == 0x15) dd &= ~0x04;
                if (!dmc) WriteWithoutDmc(apu, aa, dd);
                else if (aa < 0x20) apu.Write(0x4000 | aa, dd);
            },
            [&](uint8_t type, const uint8_t* data, uint32_t size)
            {
                if (dmc && type == 0xC2 && size > 2) apu.WriteMemory(data[0] | data[1] << 8, data + 2, size - 2);
            },
            [&](int n)
            {
                while (n-- > 0) pcm.push_back(highPass.Run(apu.Output() / 2147483648.0));
            });

        return (WriteWav(out, pcm, 1, LevelGain(pcm, 1, out)) ? 0 : 1);
    }

    int Level(const char* path, bool mono)
    {
        std::vector<uint8_t> d;
        if (!LoadFile(path, d, false) || U32(d, 0) != 0x46464952) { fprintf(stderr, "%s: not a WAV\n", path); return 1; }

        size_t data = 0, size = 0; int channels = 0; bool pcm16 = false;
        for (size_t p = 12; p + 8 <= d.size();)
        {
            uint32_t id = U32(d, p), len = U32(d, p + 4);
            if (id == 0x20746D66) { pcm16 = (d[p + 8] == 1 && d[p + 22] == 16); channels = d[p + 10]; }
            if (id == 0x61746164) { data = p + 8; size = std::min<size_t>(len, d.size() - data); }
            p += 8 + len + (len & 1);
        }
        if (!data || !pcm16 || !channels) { fprintf(stderr, "%s: not a 16-bit PCM WAV\n", path); return 1; }

        int16_t* s = reinterpret_cast<int16_t*>(&d[data]);
        std::vector<float> pcm(s, s + size / 2);
        for (float& x : pcm) x /= 32768.0f;

        if (mono && channels > 1)
        {
            std::vector<float> mix(pcm.size() / channels);
            for (size_t i = 0; i < mix.size(); ++i)
            {
                for (int ch = 0; ch < channels; ++ch) mix[i] += pcm[i * channels + ch] / channels;
            }
            return (WriteWav(path, mix, 1, LevelGain(mix, 1, path)) ? 0 : 1);
        }

        double gain = LevelGain(pcm, channels, path);
        for (size_t i = 0; i < pcm.size(); ++i) s[i] = ToPcm(pcm[i] * gain);

        FILE* f = fopen(path, "wb");
        if (!f) { fprintf(stderr, "%s: can't write\n", path); return 1; }
        fwrite(d.data(), 1, d.size(), f);
        fclose(f);
        return 0;
    }

    ////////////////////////////////////////////////////////////////////////////

    // gives the triangle state that the converter reads every frame
    class TriangleApu : public RP2A03Apu
    {
    public:
        uint16_t Period() const { return m_triangle.timer_period; }
        bool Audible() const { return m_triangle.lin_counter && m_triangle.len_counter && m_triangle.timer_period >= 2; }
    };

    enum class Kind { EnvTriangle, EnvSaw, Square };

    struct Variant
    {
        const char* tag;
        Kind kind;
        int  octave; // 0 the NES pitch, 1 an octave lower
        bool tone;   // the tone gates the envelope at the same period
    };

    const Variant TriangleVariants[] =
    {
        { "T1 env triangle, octave down", Kind::EnvTriangle, 1, false },
        { "T2 env triangle",              Kind::EnvTriangle, 0, false },
        { "T3 square",                    Kind::Square,      0, false },
        { "T4 square, octave down",       Kind::Square,      1, false },
        { "T5 env triangle + tone",       Kind::EnvTriangle, 0, true  },
        { "T6 env saw",                   Kind::EnvSaw,      0, false },
        { "T7 env saw + tone",            Kind::EnvSaw,      0, true  },
    };

    struct TrianglePsg
    {
        const Variant& variant;
        std::unique_ptr<ChipYM2149> chip;
        std::vector<float> pcm;
        int shape = -1;

        TrianglePsg(const Variant& v, int clock) : variant(v), chip(new ChipYM2149(clock, SampleRate)) {}

        void Write(uint8_t reg, uint8_t data) { chip->Write(reg, data); }

        // registers of channel B for one frame
        void Update(bool audible, uint16_t nesPeriod, uint32_t nesClock, uint32_t clock)
        {
            if (!audible)
            {
                Write(7, 0x3F);
                Write(9, 0x00);
                return;
            }

            // period of the wave in chip clocks
            double period = 32.0 * (nesPeriod + 1) * clock / nesClock * (1 << variant.octave);
            const auto Round = [](double x, int max) { return std::max(1, std::min(max, int(x + 0.5))); };

            if (variant.kind == Kind::Square)
            {
                int tp = Round(period / 16, 0xFFF);
                Write(2, uint8_t(tp)); Write(3, uint8_t(tp >> 8));
                Write(7, 0x3D);
                Write(9, 13);
            }
            else
            {
                bool saw = (variant.kind == Kind::EnvSaw);
                int ep = Round(period / (saw ? 256 : 512), 0xFFFF);
                Write(11, uint8_t(ep)); Write(12, uint8_t(ep >> 8));

                int newShape = (saw ? 0x08 : 0x0A);
                if (shape != newShape) Write(13, uint8_t(shape = newShape));

                if (variant.tone)
                {
                    int tp = std::min(0xFFF, ep * (saw ? 16 : 32));
                    Write(2, uint8_t(tp)); Write(3, uint8_t(tp >> 8));
                }
                Write(7, variant.tone ? 0x3D : 0x3F);
                Write(9, 0x10);
            }
        }
    };

    int RenderTriangle(const char* in, const char* prefix)
    {
        Vgm vgm;
        if (!vgm.Open(in)) { fprintf(stderr, "%s: not a NES APU VGM\n", in); return 1; }

        TriangleApu apu;
        apu.Init(SampleRate, vgm.nesClock);

        std::vector<std::unique_ptr<TrianglePsg>> psgs;
        for (const auto& v : TriangleVariants) psgs.emplace_back(new TrianglePsg(v, ChipClock));

        std::vector<float> nes;
        nes.reserve(vgm.totalSamples);
        HighPass highPass;
        uint32_t sample = 0;

        Parse(vgm,
            [&](uint8_t aa, uint8_t dd)
            {
                // the triangle alone
                if (aa >= 0x08 && aa <= 0x0B) apu.Write(0x4000 | aa, dd);
                if (aa == 0x15) apu.Write(0x4015, dd & 0x04);
                if (aa == 0x17) apu.Write(0x4017, dd);
            },
            [&](uint8_t, const uint8_t*, uint32_t) {},
            [&](int n)
            {
                for (; n > 0; --n, ++sample)
                {
                    if (sample % SamplesPerFrame == 0)
                    {
                        for (auto& psg : psgs) psg->Update(apu.Audible(), apu.Period(), vgm.nesClock, ChipClock);
                    }
                    nes.push_back(highPass.Run(apu.Output() / 2147483648.0));

                    for (auto& psg : psgs)
                    {
                        psg->chip->Process();
                        psg->chip->RemoveDC();
                        psg->pcm.push_back(float(psg->chip->GetOutB()));
                    }
                }
            });

        std::string name = std::string(prefix) + " [T0 2A03].wav";
        int result = (WriteWav(name.c_str(), nes, 1, LevelGain(nes, 1, name.c_str())) ? 0 : 1);
        for (auto& psg : psgs)
        {
            name = std::string(prefix) + " [" + psg->variant.tag + "].wav";
            result |= (WriteWav(name.c_str(), psg->pcm, 1, LevelGain(psg->pcm, 1, name.c_str())) ? 0 : 1);
        }
        return result;
    }

    ////////////////////////////////////////////////////////////////////////////

    // writes the changed registers of a frame, as Output::Write does
    void WriteFrame(AY89XXPsg& psg, const Frame::Registers& regs)
    {
        bool switchBanks = false;
        if (regs.IsExpMode())
        {
            for (Register reg = Register::BankB_Fst; reg < Register::BankB_Lst; ++reg)
            {
                if (regs.IsChanged(reg))
                {
                    if (!switchBanks) psg.Write(Register::Mode_Bank, regs.GetData(Register::Mode_Bank) | 0x10);
                    switchBanks = true;
                    psg.Write(reg & 0x0F, regs.GetData(reg));
                }
            }
            if (switchBanks) psg.Write(Register::Mode_Bank, regs.GetData(Register::Mode_Bank));
        }
        for (Register reg = Register::BankA_Fst; reg <= Register::BankA_Lst; ++reg)
        {
            if (regs.IsChanged(reg) && !(switchBanks && reg == Register::Mode_Bank)) psg.Write(reg, regs.GetData(reg));
        }
    }

    // level steps below 20 Hz: the output of a 2-20 Hz band-pass changes per frame
    std::vector<double> FrameSteps(const std::vector<float>& x, int samplesPerFrame)
    {
        const double lp = std::exp(-2 * Pi * 20 / SampleRate), hp = std::exp(-2 * Pi * 2 / SampleRate);
        double l1 = 0, l2 = 0, h = 0, hx = 0;
        std::vector<double> band(x.size());
        for (size_t i = 0; i < x.size(); ++i)
        {
            l1 = x[i] + lp * (l1 - x[i]);
            l2 = l1 + lp * (l2 - l1);
            h = l2 - hx + hp * h; hx = l2;
            band[i] = h;
        }
        std::vector<double> steps;
        for (size_t b = 0; b + 2 * samplesPerFrame < band.size(); b += samplesPerFrame)
        {
            steps.push_back(band[b + 2 * samplesPerFrame] - band[b]);
        }
        return steps;
    }

    double Rms(const std::vector<double>& v)
    {
        double s = 0;
        for (double x : v) s += x * x;
        return (v.empty() ? 0 : std::sqrt(s / v.size()));
    }

    int AnalyzeChannels(const char* in, const char* prefix, bool ay8930)
    {
        Vgm vgm;
        if (!vgm.Open(in)) { fprintf(stderr, "%s: not a NES APU VGM\n", in); return 1; }

        SimRP2A03 sim;
        sim.ConfigureClock(vgm.nesClock, ChipClock);
        sim.ConfigureOutput(ay8930 ? SimRP2A03::OutputType::AY8930Chip : SimRP2A03::OutputType::SingleChip, Chip::Model::YM2149);

        // AY89XXPsg has no virtual destructor, so each model is owned by its own type
        std::unique_ptr<ChipYM2149> ym(ay8930 ? nullptr : new ChipYM2149(ChipClock, SampleRate));
        std::unique_ptr<ChipAY8930> ay(ay8930 ? new ChipAY8930(ChipClock, SampleRate) : nullptr);
        AY89XXPsg& psg = (ay8930 ? static_cast<AY89XXPsg&>(*ay) : *ym);

        RP2A03Apu apu;
        apu.Init(SampleRate, vgm.nesClock);

        Frame frame;
        std::vector<float> raw[3], nes;
        std::vector<uint8_t> causes[3]; // per frame: 1 volume, 2 mixer, 4 envelope mode, 8 tone period
        uint32_t sample = 0;

        Parse(vgm,
            [&](uint8_t aa, uint8_t dd)
            {
                if (aa < 0x20) sim.Write(0, aa, dd);
                WriteWithoutDmc(apu, aa, dd);
            },
            [&](uint8_t, const uint8_t*, uint32_t) {},
            [&](int n)
            {
                for (; n > 0; --n, ++sample)
                {
                    // frames are converted as DecodeVGM does
                    if (sample % SamplesPerFrame == 0)
                    {
                        sim.Convert(frame);
                        const auto& regs = frame[0];
                        for (int ch = 0; ch < 3; ++ch)
                        {
                            uint8_t c = 0;
                            if (regs.IsChanged(Register::volume[ch]))
                                c |= ((regs.GetDiff(Register::volume[ch]) & regs.emask()) ? 4 : 1);
                            if (regs.IsChanged(Register::Mixer, uint8_t(0x09 << ch))) c |= 2;
                            if (regs.IsChanged(Register::t_period[ch])) c |= 8;
                            causes[ch].push_back(c);
                        }
                        WriteFrame(psg, regs);
                        frame.ResetChanges();
                    }
                    sim.Simulate(1);
                    psg.Process();
                    raw[0].push_back(float(psg.GetOutA()));
                    raw[1].push_back(float(psg.GetOutB()));
                    raw[2].push_back(float(psg.GetOutC()));
                    nes.push_back(float(apu.Output() / 2147483648.0));
                }
            });

        // the sizes of the level steps, relative to the AC RMS of the signal
        const auto Report = [&](const char* name, const std::vector<float>& x, const std::vector<uint8_t>* c)
        {
            double mean = 0, ac = 0;
            for (float v : x) mean += v;
            mean /= x.size();
            for (float v : x) ac += (v - mean) * (v - mean);
            ac = std::sqrt(ac / x.size());

            auto steps = FrameSteps(x, SamplesPerFrame);
            int big = 0, byCause[16] = {};
            for (size_t f = 0; f < steps.size(); ++f)
            {
                if (std::abs(steps[f]) > 0.5 * ac)
                {
                    ++big;
                    if (c && f + 1 < c->size()) ++byCause[(*c)[f + 1]];
                }
            }
            printf("  %-6s steps RMS %5.1f%% of AC, %4d big (%.1f/s)", name, 100 * Rms(steps) / ac, big, big * 60.0 / steps.size());
            if (c)
            {
                printf(" by cause:");
                for (int k = 0; k < 16; ++k) if (byCause[k])
                    printf(" %s%s%s%s%s=%d", k & 1 ? "vol" : "", k & 2 ? "+mix" : "", k & 4 ? "+env" : "", k & 8 ? "+per" : "", k ? "" : "none", byCause[k]);
            }
            printf("\n");
        };

        printf("%s (%s)\n", prefix, ay8930 ? "AY8930" : "YM2149");
        Report("2A03", nes, nullptr);
        std::vector<float> mix(raw[0].size());
        for (size_t i = 0; i < mix.size(); ++i) mix[i] = raw[0][i] + raw[1][i] + raw[2][i];
        Report("PSG", mix, nullptr);
        const char* names[3] = { "A", "B", "C" };
        int result = 0;
        for (int ch = 0; ch < 3; ++ch)
        {
            Report(names[ch], raw[ch], &causes[ch]);
            if (*prefix)
            {
                std::vector<float> solo(raw[ch].size());
                HighPass highPass;
                for (size_t i = 0; i < solo.size(); ++i) solo[i] = highPass.Run(raw[ch][i]);
                std::string name = std::string(prefix) + " [" + names[ch] + (ay8930 ? " AY8930" : " YM2149") + "].wav";
                result |= (WriteWav(name.c_str(), solo, 1, LevelGain(solo, 1, name.c_str())) ? 0 : 1);
            }
        }
        return result;
    }

    // the conversion as psg-tools plays it, mono, with channel B muted while it plays
    // the envelope: the triangle is silent, while the noise is placed as with it
    int RenderPsgWithoutTriangle(const char* in, const char* out, bool ay8930)
    {
        Vgm vgm;
        if (!vgm.Open(in)) { fprintf(stderr, "%s: not a NES APU VGM\n", in); return 1; }

        SimRP2A03 sim;
        sim.ConfigureClock(vgm.nesClock, ChipClock);
        sim.ConfigureOutput(ay8930 ? SimRP2A03::OutputType::AY8930Chip : SimRP2A03::OutputType::SingleChip, Chip::Model::YM2149);

        std::unique_ptr<ChipYM2149> ym(ay8930 ? nullptr : new ChipYM2149(ChipClock, SampleRate));
        std::unique_ptr<ChipAY8930> ay(ay8930 ? new ChipAY8930(ChipClock, SampleRate) : nullptr);
        AY89XXPsg& psg = (ay8930 ? static_cast<AY89XXPsg&>(*ay) : *ym);

        Frame frame;
        std::vector<float> pcm;
        HighPass highPass;
        uint32_t sample = 0;
        Parse(vgm,
            [&](uint8_t aa, uint8_t dd) { if (aa < 0x20) sim.Write(0, aa, dd); },
            [&](uint8_t, const uint8_t*, uint32_t) {},
            [&](int n)
            {
                for (; n > 0; --n, ++sample)
                {
                    if (sample % SamplesPerFrame == 0)
                    {
                        sim.Convert(frame);
                        const auto& regs = frame[0];
                        WriteFrame(psg, regs);
                        uint8_t volumeB = regs.GetData(Register::volume[1]);
                        psg.Write(Register::volume[1], (volumeB & regs.emask()) ? 0 : volumeB);
                        frame.ResetChanges();
                    }
                    sim.Simulate(1);
                    psg.Process();
                    pcm.push_back(highPass.Run(psg.GetOutA() + psg.GetOutB() + psg.GetOutC()));
                }
            });
        return (WriteWav(out, pcm, 1, LevelGain(pcm, 1, out)) ? 0 : 1);
    }

    struct NoiseLevelProbe : SimRP2A03
    {
        bool Noise() const { return m_noise.len_counter && m_noise.envelope.out && !m_noise.mode; }
        int  Volume() const { return m_noise.envelope.out; }
        int  Duty(int i) const { return (i ? m_pulse2.duty : m_pulse1.duty); }
    };

    // per frame of the NES noise: its level against the level the conversion gives it
    int NoiseLevels(const char* path, bool ay8930)
    {
        Vgm vgm;
        if (!vgm.Open(path)) return 1;
        NoiseLevelProbe sim;
        sim.ConfigureClock(vgm.nesClock, ChipClock);
        sim.ConfigureOutput(ay8930 ? SimRP2A03::OutputType::AY8930Chip : SimRP2A03::OutputType::SingleChip, Chip::Model::YM2149);
        const float ym[32] = { 0.000f, 0.000f, 0.005f, 0.008f, 0.011f, 0.014f, 0.017f, 0.020f, 0.024f, 0.030f, 0.035f, 0.040f, 0.049f, 0.058f, 0.068f, 0.078f,
                               0.093f, 0.111f, 0.130f, 0.148f, 0.177f, 0.212f, 0.246f, 0.281f, 0.334f, 0.400f, 0.467f, 0.534f, 0.635f, 0.758f, 0.880f, 1.000f };
        struct Stat { int n = 0, off3 = 0; double sum = 0, abs = 0; } alone, shared;
        int dropped = 0, total = 0;
        Frame frame;
        uint32_t sample = 0;
        Parse(vgm,
            [&](uint8_t aa, uint8_t dd) { if (aa < 0x20) sim.Write(0, aa, dd); },
            [&](uint8_t, const uint8_t*, uint32_t) {},
            [&](int n)
            {
                for (; n > 0; --n, ++sample)
                {
                    if (sample % SamplesPerFrame == 0)
                    {
                        bool noise = sim.Noise(); int v = sim.Volume(); int duties[2] = { sim.Duty(0), sim.Duty(1) };
                        sim.Convert(frame);
                        const auto& regs = frame[0];
                        if (noise)
                        {
                            ++total;
                            double target = 159.79 / (12241.0 / v + 100) * 0.2712 / 0.0756;
                            uint8_t mixer = regs.GetData(Register::Mixer);
                            double parts = 0, products = 0, solo = 0; bool any = false, isShared = false;
                            for (int ch = 0; ch < 3; ++ch)
                            {
                                if (mixer & (8 << ch)) continue;
                                any = true;
                                uint8_t vol = regs.GetData(Register::volume[ch]);
                                double L = (ay8930 ? ym[vol & 0x1F] : ym[((vol & 0x0F) << 1) | 1]);
                                if (!(mixer & (1 << ch)) && !(ay8930 && ch == 1 && (vol & regs.emask())))
                                {
                                    isShared = true;
                                    const double ratios[4] = { 0.125, 0.25, 0.5, 0.75 };
                                    double d = (ay8930 ? ratios[duties[ch == 0 ? 0 : 1]] : 0.5);
                                    parts += (ay8930 ? 1 - d : d) * L; products += d * (1 - d) * L * L;
                                }
                                else solo = std::max(solo, L);
                            }
                            if (!any) ++dropped;
                            else
                            {
                                double level = (isShared ? std::sqrt(parts * parts + products) : solo);
                                double err = 20 * std::log10(level / target);
                                Stat& s = (isShared ? shared : alone);
                                ++s.n; s.sum += err; s.abs += std::abs(err); if (std::abs(err) > 3) ++s.off3;
                            }
                        }
                        frame.ResetChanges();
                    }
                    sim.Simulate(1);
                }
            });
        const auto P = [](const char* name, const Stat& s)
        {
            if (s.n) printf("  %s %5d frames: mean %+5.1f dB, mean |err| %4.1f dB, over 3 dB %4.1f%%", name, s.n, s.sum / s.n, s.abs / s.n, 100.0 * s.off3 / s.n);
            else printf("  %s     0 frames                                            ", name);
        };
        P("alone", alone); P("shared", shared);
        printf("  dropped %4.1f%%  %s\n", 100.0 * dropped / std::max(total, 1), path);
        return 0;
    }

    int Scan(const char* path)
    {
        Vgm vgm;
        if (!vgm.Open(path)) { printf("-\t%s\n", path); return 0; }

        int dmcStarts = 0, dacWrites = 0, shortNoise = 0, sweeps = 0, frameCnt = 0, hwEnv = 0, longWaits = 0, dmcBytes = 0;
        Parse(vgm,
            [&](uint8_t aa, uint8_t dd)
            {
                if (aa == 0x15 && (dd & 0x10)) ++dmcStarts;
                if (aa == 0x11) ++dacWrites;
                if (aa == 0x0E && (dd & 0x80)) ++shortNoise;
                if ((aa == 0x01 || aa == 0x05) && (dd & 0x80) && (dd & 0x07)) ++sweeps;
                if (aa == 0x17) ++frameCnt;
                if ((aa == 0x00 || aa == 0x04 || aa == 0x0C) && !(dd & 0x10)) ++hwEnv;
            },
            [&](uint8_t type, const uint8_t*, uint32_t size) { if (type == 0xC2) dmcBytes += size; },
            [&](int n) { if (n > 735) ++longWaits; });

        printf("%u\t%u\tdmc=%d/%dB\tdac=%d\tshort=%d\tsweep=%d\t4017=%d\thwenv=%d\tlongwait=%d\t%s\n",
            vgm.totalSamples / SampleRate, vgm.nesClock, dmcStarts, dmcBytes, dacWrites, shortNoise, sweeps, frameCnt, hwEnv, longWaits, path);
        return 0;
    }
}

int main(int argc, char* argv[])
{
    if (argc >= 5 && !strcmp(argv[1], "render") && !strcmp(argv[2], "--dmc")) return Render(argv[3], argv[4], true);
    if (argc >= 4 && !strcmp(argv[1], "render")) return Render(argv[2], argv[3], false);
    if (argc >= 3 && !strcmp(argv[1], "level"))
    {
        bool mono = !strcmp(argv[2], "--mono");
        int result = 0;
        for (int i = (mono ? 3 : 2); i < argc; ++i) result |= Level(argv[i], mono);
        return result;
    }
    if (argc >= 4 && !strcmp(argv[1], "tri")) return RenderTriangle(argv[2], argv[3]);
    if (argc >= 4 && !strcmp(argv[1], "chan")) return AnalyzeChannels(argv[2], argv[3], argc >= 5 && !strcmp(argv[4], "AY8930"));
    if (argc >= 3 && !strcmp(argv[1], "nlevel"))
    {
        bool ay = !strcmp(argv[2], "AY8930");
        for (int i = (ay ? 3 : 2); i < argc; ++i) NoiseLevels(argv[i], ay);
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "notri"))
    {
        const char* what = (argc >= 5 ? argv[4] : "2A03");
        if (!strcmp(what, "2A03")) return Render(argv[2], argv[3], false, false);
        return RenderPsgWithoutTriangle(argv[2], argv[3], !strcmp(what, "AY8930"));
    }
    if (argc >= 3 && !strcmp(argv[1], "scan"))
    {
        for (int i = 2; i < argc; ++i) Scan(argv[i]);
        return 0;
    }
    fprintf(stderr, "usage: nes2wav render [--dmc] <vgm> <out.wav> | level [--mono] <file.wav>... | scan <vgm>...\n"
                    "               | tri <vgm> <out prefix> | chan <vgm> <out prefix> [AY8930]\n"
                    "               | notri <vgm> <out.wav> [2A03|YM2149|AY8930] | nlevel [AY8930] <vgm>...\n");
    return 1;
}
