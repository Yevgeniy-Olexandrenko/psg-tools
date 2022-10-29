#pragma once

#include "Processing.h"

class AY8930EnvelopeFix : public Processing
{
    Chip m_chip;

public:
    AY8930EnvelopeFix(Chip dstChip)
        : m_chip(dstChip)
    {}

#ifdef Enable_AY8930EnvelopeFix
	const Frame& operator()(const Frame& frame) override
    {
        Update(frame);
        for (int chip = 0; chip <  m_chip.count(); ++chip)
        {
            if (m_chip.model(chip) == Chip::Model::AY8930)
            {
                uint8_t mixer = m_frame[chip].Read(Mixer);
                for (int chan = 0; chan < 3; ++chan)
                {
                    uint8_t vol_e = m_frame[chip].Read(A_Volume + chan);
                    bool enableT = !(mixer & m_frame[chip].tmask(chan));
                    bool enableN = !(mixer & m_frame[chip].nmask(chan));
                    bool enableE = (vol_e & m_frame[chip].emask());

                    if (enableE && !(enableT || enableN))
                    {
                        m_frame[chip].UpdatePeriod(A_Period + (2 * chan), 0);
                        m_frame[chip].Update(A_Duty + chan, 0x08);
                        mixer &= ~(m_frame[chip].tmask(chan));
                    }
                }
                m_frame[chip].Update(Mixer, mixer);
            }
        }
        return m_frame;
    }
#endif
};
