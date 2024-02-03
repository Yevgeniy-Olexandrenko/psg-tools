#pragma once

#include "FrameProcessor.h"

class ChipClockRateConvert : public FrameProcessor
{
    float m_ratio;
    int   m_count;

public:
    ChipClockRateConvert(const Chip& srcChip, const Chip& dstChip)
        : m_ratio(1.f)
        , m_count(dstChip.count())
    {
        if (srcChip.clockKnown() && dstChip.clockKnown())
        {
            m_ratio = float(dstChip.clockValue()) / float(srcChip.clockValue());
        }
    }

#ifdef Enable_ChipClockRateConvert
    // TODO: needs testing!
    const Frame& Execute(const Frame& frame) override
    {
        if (m_ratio != 1.f)
        {
            Update(frame);
            for (int chip = 0; chip < m_count; ++chip)
            {
                bool isExpMode = m_frame[chip].IsExpMode();
                uint16_t tBound = (isExpMode ? 0xFFFF : 0x0FFF);
                uint16_t nBound = (isExpMode ? 0x00FF : 0x001F);

                // safe period conversion based on clock ratio
                const auto ConvertPeriod = [&](PRegister preg, uint16_t bound)
                {
                    auto period = uint32_t(m_frame[chip].Read(preg) * m_ratio + 0.5f);
                    m_frame[chip].Update(preg, uint16_t(period > bound ? bound : period));
                };

                // convert tone and envelope periods
                for (int chan = 0; chan < 3; ++chan)
                {
                    ConvertPeriod(PRegister::t_period[chan], tBound);
                    ConvertPeriod(PRegister::e_period[chan], 0xFFFF);
                }

                // convert noise period
                ConvertPeriod(PRegister::N_Period, nBound);
            }
            return m_frame;
        }
        return frame;
    }
#endif
};
