#include <thread>
#include "Emulator.h"
#include "stream/Stream.h"

Emulator::Emulator()
{
}

Emulator::~Emulator()
{
    DeviceClose();
}

const std::string Emulator::GetDeviceName() const
{
    return "Emulator";
}

bool Emulator::DeviceOpen()
{
    if (WaveAudio::Open(k_emulatorSampleRate, 100, 2, 2))
    {
        // make some delay for warm up the wave audio
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return true;
    }
    return false;
}

bool Emulator::DeviceInit(const Stream& stream, Chip& dchip)
{
    // create sound chip emulator instances
    std::lock_guard<std::mutex> lock(m_mutex);
    for (int chip = 0; chip < dchip.count(); ++chip)
    {
        // create sound chip emulator by model
        switch (dchip.model(chip))
        {
        case Chip::Model::AY8910: m_psg[chip].reset(new ChipAY8910(dchip.clockValue(), k_emulatorSampleRate)); break;
        case Chip::Model::YM2149: m_psg[chip].reset(new ChipYM2149(dchip.clockValue(), k_emulatorSampleRate)); break;
        case Chip::Model::AY8930: m_psg[chip].reset(new ChipAY8930(dchip.clockValue(), k_emulatorSampleRate)); break;
        }
        if (!m_psg[chip]) return false;
        
        // initialize sound chip emulator
        m_psg[chip]->Reset();
        if (dchip.output() == Chip::Output::Mono)
        {
            m_psg[chip]->SetPan(0, 0.5, false);
            m_psg[chip]->SetPan(1, 0.5, false);
            m_psg[chip]->SetPan(2, 0.5, false);
        }
        else
        {
            m_psg[chip]->SetPan(0, 0.1, false);
            m_psg[chip]->SetPan(1, 0.5, false);
            m_psg[chip]->SetPan(2, 0.9, false);
        }        
    }
    return true;
}

bool Emulator::DeviceWrite(int chip, const Data& data)
{
    for (const auto& pair : data)
    {
        const uint8_t& reg = pair.first;
        const uint8_t& val = pair.second;
        m_psg[chip]->Write(reg, val);
    }
    return true;
}

void Emulator::FillBuffer(unsigned char* buffer, unsigned long size)
{
    if (m_psg[0])
    {
        // buffer format must be 2 ch x 16 bit
        auto sampbuf = (int16_t*)buffer;
        auto samples = (int)(size / sizeof(int16_t));

        if (m_psg[1])
        {
            for (int i = 0; i < samples;)
            {
                m_psg[0]->Process();
                m_psg[1]->Process();
                m_psg[0]->RemoveDC();
                m_psg[1]->RemoveDC();

                double L = 0.5 * (m_psg[0]->GetOutL() + m_psg[1]->GetOutL());
                double R = 0.5 * (m_psg[0]->GetOutR() + m_psg[1]->GetOutR());

                L = L > +1.0 ? +1.0 : (L < -1.0 ? -1.0 : L);
                R = R > +1.0 ? +1.0 : (R < -1.0 ? -1.0 : R);

                sampbuf[i++] = (int16_t)(INT16_MAX * L + 0.5);
                sampbuf[i++] = (int16_t)(INT16_MAX * R + 0.5);
            }
        }
        else
        {
            for (int i = 0; i < samples;)
            {
                m_psg[0]->Process();
                m_psg[0]->RemoveDC();

                double L = 0.5 * m_psg[0]->GetOutL();
                double R = 0.5 * m_psg[0]->GetOutR();

                L = L > +1.0 ? +1.0 : (L < -1.0 ? -1.0 : L);
                R = R > +1.0 ? +1.0 : (R < -1.0 ? -1.0 : R);

                sampbuf[i++] = (int16_t)(INT16_MAX * L + 0.5);
                sampbuf[i++] = (int16_t)(INT16_MAX * R + 0.5);
            }
        }
    }
}

void Emulator::DeviceClose()
{
    WaveAudio::Close();
    m_psg[0].reset();
    m_psg[1].reset();
}
