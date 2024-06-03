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

// TODO: needs testing!
// TODO: rewrite using processing unit!
void SimAY8910::Convert(Frame& frame)
{
    m_pframe += m_frame;
    m_frame.ResetChanges();

    if (m_clockRatio != 1.f)
    {
        for (int chip = 0; chip < 2; ++chip)
        {
            bool isExpMode = m_pframe[chip].IsExpMode();
            uint16_t tBound = (isExpMode ? 0xFFFF : 0x0FFF);
            uint16_t nBound = (isExpMode ? 0x00FF : 0x001F);

            // safe period conversion based on clock ratio
            const auto ConvertPeriod = [&](Register::Period regp, uint16_t bound)
            {
                auto period = uint32_t(m_pframe[chip].Read(regp) * m_clockRatio + 0.5f);
                m_pframe[chip].Update(regp, uint16_t(period > bound ? bound : period));
            };

            // convert tone and envelope periods
            for (int chan = 0; chan < 3; ++chan)
            {
                ConvertPeriod(Register::t_period[chan], tBound);
                ConvertPeriod(Register::e_period[chan], 0xFFFF);
            }

            // convert noise period
            ConvertPeriod(Register::Period::N, nBound);
        }
    }

    frame += m_pframe;
    m_pframe.ResetChanges();
}
