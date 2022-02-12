#include <thread>
#include "AY38910.h"
#include "module/Module.h"

namespace
{
    const int k_is_ym = 0;
    const int k_clock_rate = 1750000;
    const int k_sample_rate = 48000;// 44100;
}

AY38910::AY38910(const Module& module)
    : Output(module)
    , WaveAudio(k_sample_rate, 100, 2, 2)
    , m_ts(module.chip.config() == ChipConfig::TurboSound)
    , m_ay{0}
{
    m_isOpened = InitChip(0, module);
    if (m_ts) m_isOpened &= InitChip(1, module);
}

AY38910::~AY38910()
{
    Close();
}

void AY38910::Open()
{
    if (m_isOpened)
    {
        WaveAudio::Start();
        if (m_isOpened &= m_working)
        {
            // make some delay for wave audio warm up
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

bool AY38910::OutFrame(const Frame& frame, bool force)
{
    if (m_isOpened)
    {
        WriteToChip(0, frame, force);
        if (m_ts) WriteToChip(1, frame, force);
    }
    return m_isOpened;
}

void AY38910::Close()
{
    WaveAudio::Stop();
}

void AY38910::FillBuffer(unsigned char* buffer, unsigned long size)
{
    // buffer format must be 2 ch x 16 bit
    auto sampbuf = (int16_t*)buffer;
    auto samples = (int)(size / sizeof(int16_t));

    if (m_ts)
    {
        for (int i = 0; i < samples;)
        {
            ayumi_process(&m_ay[0]);
            ayumi_process(&m_ay[1]);
            ayumi_remove_dc(&m_ay[0]);
            ayumi_remove_dc(&m_ay[1]);

            double L = 0.5 * (m_ay[0].left  + m_ay[1].left );
            double R = 0.5 * (m_ay[0].right + m_ay[1].right);

            L = L > +1.0 ? +1.0 : (L < -1.0 ? -1.0 : L);
            R = R > +1.0 ? +1.0 : (R < -1.0 ? -1.0 : R);

            sampbuf[i++] = (int16_t)(INT16_MAX * L + 0.5);
            sampbuf[i++] = (int16_t)(INT16_MAX * R + 0.5);
        }
    }
    else
    {
        ayumi& chip = m_ay[0];
        for (int i = 0; i < samples;)
        {
            ayumi_process(&chip);
            ayumi_remove_dc(&chip);

            double L = chip.left  > +1.0 ? +1.0 : (chip.left  < -1.0 ? -1.0 : chip.left );
            double R = chip.right > +1.0 ? +1.0 : (chip.right < -1.0 ? -1.0 : chip.right);

            sampbuf[i++] = (int16_t)(INT16_MAX * L + 0.5);
            sampbuf[i++] = (int16_t)(INT16_MAX * R + 0.5);
        }
    }
}

bool AY38910::InitChip(uint8_t chip, const Module& module)
{
    uint32_t clockRate = module.chip.freqValue(k_clock_rate);
    bool isYM = module.chip.typeKnown() ? (module.chip.type() == ChipType::YM) : bool(k_is_ym);

    ayumi* ay = &m_ay[chip];
    if (ayumi_configure(ay, isYM, clockRate, k_sample_rate))
    {
        switch (module.chip.stereo())
        {
        case ChipStereo::MONO:
            ayumi_set_pan(ay, 0, 0.5, true);
            ayumi_set_pan(ay, 1, 0.5, true);
            ayumi_set_pan(ay, 2, 0.5, true);
            break;

        case ChipStereo::ACB:
            ayumi_set_pan(ay, 0, 0.1, true);
            ayumi_set_pan(ay, 1, 0.9, true);
            ayumi_set_pan(ay, 2, 0.5, true);
            break;

        default: // ABC
            ayumi_set_pan(ay, 0, 0.1, true);
            ayumi_set_pan(ay, 1, 0.5, true);
            ayumi_set_pan(ay, 2, 0.9, true);
            break;
        }
        return true;
    }
    return false;
}

void AY38910::WriteToChip(uint8_t chip, const Frame& frame, bool force)
{
    ayumi* ay = &m_ay[chip];

    uint8_t r7 = frame.data(chip, Mixer_Flags);
    uint8_t r8 = frame.data(chip, VolA_EnvFlg);
    uint8_t r9 = frame.data(chip, VolB_EnvFlg);
    uint8_t rA = frame.data(chip, VolC_EnvFlg);

    ayumi_set_tone(ay, 0, frame.data(chip, TonA_PeriodL) | frame.data(chip, TonA_PeriodH) << 8);
    ayumi_set_tone(ay, 1, frame.data(chip, TonB_PeriodL) | frame.data(chip, TonB_PeriodH) << 8);
    ayumi_set_tone(ay, 2, frame.data(chip, TonC_PeriodL) | frame.data(chip, TonC_PeriodH) << 8);

    ayumi_set_noise(ay, frame.data(chip, Noise_Period));

    ayumi_set_mixer(ay, 0, r7 >> 0 & 0x01, r7 >> 3 & 0x01, r8 >> 4);
    ayumi_set_mixer(ay, 1, r7 >> 1 & 0x01, r7 >> 4 & 0x01, r9 >> 4);
    ayumi_set_mixer(ay, 2, r7 >> 2 & 0x01, r7 >> 5 & 0x01, rA >> 4);

    ayumi_set_volume(ay, 0, r8 & 0x0F);
    ayumi_set_volume(ay, 1, r9 & 0x0F);
    ayumi_set_volume(ay, 2, rA & 0x0F);

    ayumi_set_envelope(ay, frame.data(chip, Env_PeriodL) | frame.data(chip, Env_PeriodH) << 8);
    if (force || frame.changed(chip, Env_Shape))
    {
        ayumi_set_envelope_shape(ay, frame.data(chip, Env_Shape));
    }
}

