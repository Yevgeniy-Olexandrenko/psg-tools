#include "PCMOutput.h"

PCMOutput::PCMOutput()
	: m_panL{}
	, m_panR{}
{
}

PCMOutput::~PCMOutput()
{
}

bool PCMOutput::DeviceWrite(int chip, const Data& data)
{
	for (const auto& [reg, val] : data)
	{
		m_psg[chip]->Write(reg, val);
	}
	return true;
}

void PCMOutput::PCMInit(Chip& dchip)
{
	const auto SetPan = [&](int chan, float pan)
	{
		m_panL[chan] = 1.f - pan;
		m_panR[chan] = pan;
	};

	// create sound chip emulator instances
	for (int chip = 0; chip < dchip.count; ++chip)
	{
		// create sound chip emulator by model
		switch (dchip.model(chip))
		{
		case Chip::Model::AY8910: m_psg[chip].reset(new ChipAY8910(dchip.clockValue, k_PCMSampleRate)); break;
		case Chip::Model::YM2149: m_psg[chip].reset(new ChipYM2149(dchip.clockValue, k_PCMSampleRate)); break;
		case Chip::Model::AY8930: m_psg[chip].reset(new ChipAY8930(dchip.clockValue, k_PCMSampleRate)); break;
		}
		m_psg[chip]->Reset();
	}

	if (dchip.output == Chip::Output::Mono)
	{
		SetPan(0, 0.5f);
		SetPan(1, 0.5f);
		SetPan(2, 0.5f);
	}
	else
	{
		SetPan(0, 0.1f);
		SetPan(1, 0.5f);
		SetPan(2, 0.9f);
	}
}

void PCMOutput::PCMFill(std::vector<float>& buffer)
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
