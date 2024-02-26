#include <thread>
#include "Emulator.h"
#include "stream/Stream.h"

Emulator::Emulator()
    : m_panL{}
    , m_panR{}
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
    return WaveAudio::Open(k_emulatorSampleRate, 2, 4, 366);
}

bool Emulator::DeviceInit(const Stream& stream, Chip& dchip)
{
    WaveAudio::Pause();

    // create sound chip emulator instances
    for (int chip = 0; chip < dchip.count(); ++chip)
    {
        // create sound chip emulator by model
        switch (dchip.model(chip))
        {
        case Chip::Model::AY8910: m_psg[chip].reset(new ChipAY8910(dchip.clockValue(), k_emulatorSampleRate)); break;
        case Chip::Model::YM2149: m_psg[chip].reset(new ChipYM2149(dchip.clockValue(), k_emulatorSampleRate)); break;
        case Chip::Model::AY8930: m_psg[chip].reset(new ChipAY8930(dchip.clockValue(), k_emulatorSampleRate)); break;
        }
        m_psg[chip]->Reset();   
    }

    if (dchip.output() == Chip::Output::Mono)
    {
        SetPan(0, 0.5f, false);
        SetPan(1, 0.5f, false);
        SetPan(2, 0.5f, false);
    }
    else
    {
        SetPan(0, 0.1f, false);
        SetPan(1, 0.5f, false);
        SetPan(2, 0.9f, false);
    }

    WaveAudio::Resume();
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

void Emulator::FillBuffer(std::vector<float>& buffer)
{
    if (m_psg[0])
    {
        if (m_psg[1])
        {
            for (size_t i = 0; i < buffer.size();)
            {
                m_psg[0]->Process();
                m_psg[1]->Process();
                m_psg[0]->RemoveDC();
                m_psg[1]->RemoveDC();

                auto A = float(0.5f * (m_psg[0]->GetOutA() + m_psg[1]->GetOutA()));
                auto B = float(0.5f * (m_psg[0]->GetOutB() + m_psg[1]->GetOutB()));
                auto C = float(0.5f * (m_psg[0]->GetOutC() + m_psg[1]->GetOutC()));

                buffer[i++] = m_panL[0] * A + m_panL[1] * B + m_panL[2] * C;
                buffer[i++] = m_panR[0] * A + m_panR[1] * B + m_panR[2] * C;
            }
        }
        else
        {
            for (size_t i = 0; i < buffer.size();)
            {
                m_psg[0]->Process();
                m_psg[0]->RemoveDC();

                auto A = float(0.5f * m_psg[0]->GetOutA());
                auto B = float(0.5f * m_psg[0]->GetOutB());
                auto C = float(0.5f * m_psg[0]->GetOutC());

                buffer[i++] = m_panL[0] * A + m_panL[1] * B + m_panL[2] * C;
                buffer[i++] = m_panR[0] * A + m_panR[1] * B + m_panR[2] * C;
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

void Emulator::SetPan(int chan, float pan, bool isEqp)
{
	if (isEqp)
	{
		m_panL[chan] = sqrt(1 - pan);
		m_panR[chan] = sqrt(pan);
	}
	else
	{
		m_panL[chan] = 1 - pan;
		m_panR[chan] = pan;
	}
}
