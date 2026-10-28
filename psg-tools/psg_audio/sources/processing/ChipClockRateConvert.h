#pragma once

#include "FrameProcessor.h"

class ChipClockRateConvert : public FrameProcessor
{
    uint32_t m_srcClock;
    uint32_t m_dstClock;
    int      m_count;

public:
    ChipClockRateConvert(const Chip& srcChip, const Chip& dstChip)
        : m_srcClock(1)
        , m_dstClock(1)
        , m_count(dstChip.count)
    {
        if (srcChip.clockKnown && dstChip.clockKnown)
        {
            m_srcClock = srcChip.clockValue;
            m_dstClock = dstChip.clockValue;
        }
    }

    // for exact clock rates that are not in the list of chip clocks
    ChipClockRateConvert(uint32_t srcClock, uint32_t dstClock, int count)
        : m_srcClock(srcClock)
        , m_dstClock(dstClock)
        , m_count(count)
    {}

#ifdef Enable_ChipClockRateConvert
    const Frame& Execute(const Frame& frame) override
    {
        if (m_srcClock != m_dstClock)
        {
            Update(frame);
            for (int chip = 0; chip < m_count; ++chip)
            {
                auto& regs = m_frame[chip];
                bool isExpMode = regs.IsExpMode();
                uint16_t tBound = (isExpMode ? 0xFFFF : 0x0FFF);
                uint16_t nBound = (isExpMode ? 0x00FF : 0x001F);

                // safe period conversion based on clock ratio, integer
                // math gives exact rounding where float precision fails
                const auto ConvertPeriod = [&](uint16_t period, uint16_t bound)
                {
                    auto result = (uint64_t(period) * m_dstClock + m_srcClock / 2) / m_srcClock;
                    return uint16_t(result > bound ? bound : result);
                };

                // envelope period used by the channel (one shared in comp mode)
                const auto EnvelopePeriod = [&](int chan)
                {
                    return (isExpMode ? Register::e_period[chan] : Register::Period::E);
                };

                // read the source periods before any of them is updated
                uint16_t tSrc[3], eSrc[3];
                for (int chan = 0; chan < 3; ++chan)
                {
                    tSrc[chan] = regs.Read(Register::t_period[chan]);
                    eSrc[chan] = regs.Read(EnvelopePeriod(chan));
                }

                for (int chan = 0; chan < 3; ++chan)
                {
                    uint16_t eDst = ConvertPeriod(eSrc[chan], 0xFFFF);
                    uint16_t tDst = ConvertPeriod(tSrc[chan], tBound);

                    // tone and periodic envelope played together are often tuned so
                    // that one period is a power of two multiple of another (buzzer
                    // and sync bass effects), rounding each period separately breaks
                    // this relation and makes them beat, so derive tone from envelope
                    if (IsSynced(tSrc[chan], eSrc[chan]) && regs.IsToneEnabled(chan) &&
                        regs.IsEnvelopeEnabled(chan) && regs.IsPeriodicEnvelope(chan))
                    {
                        auto result = (uint32_t(tSrc[chan]) * eDst + eSrc[chan] / 2) / eSrc[chan];
                        tDst = uint16_t(result > tBound ? tBound : result);
                    }

                    regs.Update(Register::t_period[chan], tDst);
                    if (isExpMode || chan == 0) regs.Update(EnvelopePeriod(chan), eDst);
                }

                // convert noise period
                regs.Update(Register::Period::N, ConvertPeriod(regs.Read(Register::Period::N), nBound));
            }
            return m_frame;
        }
        return frame;
    }

private:
    static bool IsSynced(uint16_t tone, uint16_t envelope)
    {
        if (!tone || !envelope) return false;
        uint16_t lo = (tone < envelope ? tone : envelope);
        uint16_t hi = (tone < envelope ? envelope : tone);
        uint16_t k  = hi / lo;
        return (hi % lo == 0 && (k & (k - 1)) == 0);
    }
#endif
};
