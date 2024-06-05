#pragma once

#include "FrameProcessor.h"

class AY8930EnvelopeFix : public FrameProcessor
{
    Chip m_chip;

public:
    AY8930EnvelopeFix(Chip dstChip)
        : m_chip(dstChip)
    {}

#ifdef Enable_AY8930EnvelopeFix
	const Frame& Execute(const Frame& frame) override
    {
        Update(frame);
        for (int chip = 0; chip <  m_chip.count; ++chip)
        {
            if (m_chip.model(chip) == Chip::Model::AY8930)
            {
                uint8_t mixer = m_frame[chip].Read(Register::Mixer);
                for (int chan = 0; chan < 3; ++chan)
                {
                    bool enableT = m_frame[chip].IsToneEnabled(chan);
                    bool enableN = m_frame[chip].IsNoiseEnabled(chan);
                    bool enableE = m_frame[chip].IsEnvelopeEnabled(chan);

                    if (enableE && !(enableT || enableN))
                    {
                        m_frame[chip].Update(Register::t_period[chan], uint16_t(0));
                        m_frame[chip].Update(Register::t_duty[chan], 8);
                        mixer &= ~(m_frame[chip].tmask(chan));
                    }
                }
                m_frame[chip].Update(Register::Mixer, mixer);
            }
        }
        return m_frame;
    }
#endif
};
