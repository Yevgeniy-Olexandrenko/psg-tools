#include "SimAY8910.h"

SimAY8910::SimAY8910()
	: ChipSim(Type::AY8910)
{
	Reset();
}

void SimAY8910::Reset()
{
	m_frame.ResetData();
	m_frame.ResetChanges();

    m_pframe.ResetData();
    m_pframe.ResetChanges();
}

void SimAY8910::Write(int chip, Register reg, uint8_t data)
{
	m_frame[chip].Update(reg, data);
}

void SimAY8910::Simulate(int samples)
{
	// do nothing
}

void SimAY8910::Convert(Frame& frame)
{
    m_pframe += m_frame;
    m_frame.ResetChanges();

    static const PeriodRegister c_com[]
    {
        A_Period, B_Period, C_Period,
        E_Period, N_Period
    };

    static const PeriodRegister c_exp[]
    {
        A_Period,  B_Period,  C_Period,
        EA_Period, EB_Period, EC_Period,
        N_Period
    };

    if (m_clockRatio != 1.f)
    {
        for (int chip = 0; chip < 2; ++chip)
        {
            bool isExpMode = m_pframe[chip].IsExpMode();
            for (size_t i = 0; i < (isExpMode ? sizeof(c_exp) : sizeof(c_com)); ++i)
            {
                PeriodRegister preg = (isExpMode ? c_exp[i] : c_com[i]);
                uint16_t period = ConvertPeriod(m_pframe[chip].ReadPeriod(preg));
                m_pframe[chip].UpdatePeriod(preg, period);
            }
        }
    }

    frame += m_pframe;
    m_pframe.ResetChanges();
}
