#pragma once

#include <array>
#include "FrameProcessor.h"

class ChannelsOutputEnable : public FrameProcessor
{
    int m_count;
    std::array<bool, 5> m_enables;

public:
    ChannelsOutputEnable(const Chip& dstChip)
        : m_enables{ true, true, true, true, true }
        , m_count(dstChip.count)

    {}

    std::array<bool, 5>& GetEnables()
    {
        return m_enables;
    }

#ifdef Enable_ChannelsOutputEnable
	const Frame& Execute(const Frame& frame) override
    {
        for (const auto& enable : m_enables)
        {
            if (!enable)
            {
                const auto ApplyEnable = [&](int chip, int chan, bool enableT, bool enableE, bool enableN)
                {
                    uint8_t mixer = m_frame[chip].Read(Register::Mixer);
                    uint8_t vol_e = m_frame[chip].Read(Register::volume[chan]);

                    // disable tone and noise
                    if (!enableT) mixer |= m_frame[chip].tmask(chan);
                    if (!enableN) mixer |= m_frame[chip].nmask(chan);

                    // disable envelope and volume if possible
                    if (!enableT && !enableN) vol_e &= m_frame[chip].emask();
                    if (!enableE) vol_e &= m_frame[chip].vmask();

                    m_frame[chip].Update(Register::Mixer, mixer);
                    m_frame[chip].Update(Register::volume[chan], vol_e);
                };

                Update(frame);
                for (int chip = 0; chip < m_count; ++chip)
                {
                    for (int chan = 0; chan < 3; ++chan)
                    {
                        ApplyEnable(chip, chan, m_enables[chan], m_enables[3], m_enables[4]);
                    }
                }
                return m_frame;
            }
        }
        return frame;
    }
#endif
};
