#include "RP2A03.h"

namespace
{
    constexpr uint8_t lengthLut[] =
    {
        10, 254, 20, 2, 40, 4, 80, 6,
        160, 8, 60, 10, 14, 12, 26, 14,
        12, 16, 24, 18, 48, 20, 96, 22,
        192, 24, 72, 26, 16, 28, 32, 30,
    };

    constexpr uint16_t noiseLut[] =
    {
        0x002, 0x004, 0x008, 0x010,
        0x020, 0x030, 0x040, 0x050,
        0x065, 0x07F, 0x0BE, 0x0FE,
        0x17D, 0x1FC, 0x3F9, 0x7F2,
    };

    constexpr uint32_t noise_freq_table[] =
    {
        0x002, 0x004, 0x008, 0x010,
        0x020, 0x030, 0x040, 0x050,
        0x065, 0x07F, 0x0BE, 0x0FE,
        0x17D, 0x1FC, 0x3F9, 0x7F2
    };

    enum
    {
        APU_RECT_VOL1         = 0x00,  // 4000
        APU_SWEEP1            = 0x01,  // 4001
        APU_RECT_FREQ1        = 0x02,  // 4002
        APU_RECT_LEN1         = 0x03,  // 4003

        APU_RECT_VOL2         = 0x04,  // 4004
        APU_SWEEP2            = 0x05,  // 4005
        APU_RECT_FREQ2        = 0x06,  // 4006
        APU_RECT_LEN2         = 0x07,  // 4007

        APU_TRIANGLE          = 0x08,  // 4008
        // NOT USED 0x09
        APU_TRI_FREQ          = 0x0A,  // 400A
        APU_TRI_LEN           = 0x0B,  // 400B

        APU_NOISE_VOL         = 0x0C,  // 400C
        // NOT USED 0x0D
        APU_NOISE_FREQ        = 0x0E,  // 400E
        APU_NOISE_LEN         = 0x0F,  // 400F

        APU_DMC_DMA_FREQ      = 0x10,  // 4010
        APU_DMC_DELTA_COUNTER = 0x11,  // 4011
        APU_DMC_ADDR          = 0x12,  // 4012
        APU_DMC_LEN           = 0x13,  // 4013
        APU_STATUS            = 0x15,  // 4014

        APU_LOW_TIMER         = 0x17,  // 4017
    };

    constexpr uint32_t counterScaler = ((NES_CPU_FREQUENCY << CONST_SHIFT_BITS) / SAMPLING_RATE);
    constexpr uint32_t frameCounterPeriod = ((NES_CPU_FREQUENCY << CONST_SHIFT_BITS) / 240);
}

#define VALUE_VOL_MASK      (0x0F)
#define FIXED_VOL_MASK      (0x10)
#define DISABLE_LEN_MASK    (0x20)
#define ENABLE_LOOP_MASK    (0x20)
#define DUTY_CYCLE_MASK     (0xC0)

#define SWEEP_ENABLE_MASK   (0x80)
#define SWEEP_SHIFT_MASK    (0x07)
#define SWEEP_DIR_MASK      (0x08)
#define SWEEP_RATE_MASK     (0x70)

#define PAL_MODE_MASK       (0x80)

#define CHAN1_ENABLE_MASK   (0x01)
#define CHAN2_ENABLE_MASK   (0x02)
#define TRI_ENABLE_MASK     (0x04)

#define NOISE_FREQ_MASK     (0x0F)
#define NOISE_MODE_MASK     (0x80)

#define DMC_ENABLE_MASK     (0x10)
#define DMC_RATE_MASK       (0x0F)
#define DMC_LOOP_MASK       (0x40)
#define DMC_IRQ_ENABLE_MASK (0x80)

void RP2A03::Write(uint16_t reg, uint8_t val)
{
    uint16_t originReg = reg;
    uint8_t oldVal = val;

    //LOGI("Write 0x%02X to [%04X] reg\n", val, getRegAddress(reg));

    reg &= 0x00FF;

    if (reg < 0x20)
    {
        oldVal = m_regs[reg];
        m_regs[reg] = val;
    }
    int chanIndex = (reg - APU_RECT_VOL1) / 4;

    switch (reg)
    {
    case APU_RECT_VOL1:
    case APU_RECT_VOL2:
    case APU_NOISE_VOL:
        //            m_chan[chanIndex].divider = val & VALUE_VOL_MASK;
        break;

    case APU_TRIANGLE:
        break;

    case APU_SWEEP1:
    case APU_SWEEP2:
        break;

    case APU_RECT_FREQ1:
    case APU_RECT_FREQ2:
    case APU_TRI_FREQ:
        m_chan[chanIndex].period = (m_chan[chanIndex].period & (0xFF00 << (CONST_SHIFT_BITS + 4))) | (val << (CONST_SHIFT_BITS + 4));
       /* if (m_chan[chanIndex].counter > m_chan[chanIndex].period)
            m_chan[chanIndex].counter = m_chan[chanIndex].period;*/
        break;

    case APU_NOISE_FREQ:
    {
        // reset noise generator when switching modes
        if ((oldVal & NOISE_MODE_MASK) != (val & NOISE_MODE_MASK))
        {
            m_shiftNoise = 0x0001;
        }
        m_chan[chanIndex].period = (noiseLut[val & NOISE_FREQ_MASK]) << (CONST_SHIFT_BITS + 4);
       /* if (m_chan[chanIndex].counter > m_chan[chanIndex].period)
            m_chan[chanIndex].counter = m_chan[chanIndex].period;*/
        break;
    }

    case APU_RECT_LEN1:
    case APU_RECT_LEN2:
        // We must reset duty cycle sequencer only for channels 1, 2 according to datasheet
        /*m_chan[chanIndex].sequencer = 0;*/
        // Reset counter also to prevent unexpected clicks
       /* m_chan[chanIndex].counter = 0;*/
        m_chan[chanIndex].updateEnvelope = true;
        // Unused on noise channel
        m_chan[chanIndex].period = (m_chan[chanIndex].period & (0x000000FF << (CONST_SHIFT_BITS + 4))) | (static_cast<uint32_t>(val & 0x07) << (8 + CONST_SHIFT_BITS + 4));
        m_chan[chanIndex].lenCounter = lengthLut[val >> 3];
        /*m_chan[chanIndex].counter = 0;*/
        break;

    case APU_TRI_LEN:
        // Do not reset triangle sequencer to prevent clicks. This is required per datasheet
        // Unused on noise channel
        m_chan[2].period = (m_chan[2].period & (0x000000FF << (CONST_SHIFT_BITS + 4))) | (static_cast<uint32_t>(val & 0x07) << (8 + CONST_SHIFT_BITS + 4));
        m_chan[2].lenCounter = lengthLut[val >> 3];
        m_chan[2].linearReloadFlag = true;
        /*m_chan[2].counter = 0;*/
        break;

    case APU_NOISE_LEN:
        m_chan[3].updateEnvelope = true;
        m_chan[3].lenCounter = lengthLut[val >> 3];
        /*m_chan[3].counter = 0;*/
        break;

    case APU_DMC_DMA_FREQ:
        //m_chan[4].period = dmcLut[val & DMC_RATE_MASK] << CONST_SHIFT_BITS;
        //if (m_chan[4].counter >= m_chan[4].period)
        //    m_chan[4].counter = m_chan[4].period;
        break;

    case APU_DMC_DELTA_COUNTER:
        //m_chan[4].volume = val & 0x7F;
        break;

    case APU_DMC_ADDR:
        break;

    case APU_DMC_LEN:
        break;

    case APU_STATUS:
        for (int i = 0; i < 4; i++)
        {
            if (!(val & (1 << i)))
            {
                m_chan[i].counter = 0;
                m_chan[i].lenCounter = 0;
            }
        }
        /*if ((m_regs[APU_STATUS] & 0x10) && !m_chan[4].dmcActive)
        {
            m_chan[4].dmcActive = true;
            m_chan[4].dmcAddr = m_regs[APU_DMC_ADDR] * 0x40 + 0xC000;
            m_chan[4].dmcLen = m_regs[APU_DMC_LEN] * 16 + 1;
            m_chan[4].dmcIrqFlag = false;
        }
        else if (!(m_regs[APU_STATUS] & 0x10))
        {
            m_chan[4].dmcActive = false;
        }*/
        break;

    case APU_LOW_TIMER:
        m_lastFrameCounter = 0;
        m_apuFrames = 0;
        break;

    default:
        // Check for sweep support on channels TRI and NOISE
        if (reg != 0x09 && reg != 0x0D)
        {
            //LOGE("Unknown reg 0x%02X [0x%04X]\n", reg, originReg);
        }
        break;
    }
}

void RP2A03::Update(int samples)
{
    while (samples--)
    {
        updateFrameCounter();
        updateRectChannel(0);
        updateRectChannel(1);
        updateTriangleChannel(m_chan[2]);
        updateNoiseChannel(m_chan[3]);
        updateDmcChannel(m_chan[4]);
    }
}

void RP2A03::updateFrameCounter()
{
    const uint8_t upperThreshold = (m_regs[APU_LOW_TIMER] & PAL_MODE_MASK) ? 5 : 4;

    m_quaterSignal = false;
    m_halfSignal = false;
    m_fullSignal = false;

    m_lastFrameCounter += counterScaler;
    if (m_lastFrameCounter >= frameCounterPeriod)
    {
        m_lastFrameCounter -= frameCounterPeriod;
        m_apuFrames++;
        if (m_apuFrames != 4 || upperThreshold != 5)
        {
            m_quaterSignal = true;
        }
        m_halfSignal = (m_apuFrames == 2) || (m_apuFrames >= upperThreshold);
        if (m_apuFrames >= upperThreshold)
        {
            m_fullSignal = true;
            m_apuFrames = 0;
        }
    }
}

void RP2A03::updateRectChannel(int i)
{
    ChannelInfo& chan = m_chan[i];
    //static constexpr uint8_t sequencerTable[] =
    //{
    //    0b01000000,
    //    0b01100000,
    //    0b01111000,
    //    0b10011111,
    //};

    if (!(m_regs[APU_STATUS] & (1 << i)))
    {
        chan.volume = 0;
        chan.output = chan.volume;
        return;
    }

    uint8_t volumeReg = m_regs[APU_RECT_VOL1 + i * 4];

    // Decay counters are always enabled
    // So, run the envelope anyway
    if (m_quaterSignal)
    {
        if (chan.updateEnvelope)
        {
            chan.decayCounter = 0x0F;
            chan.divider = volumeReg & VALUE_VOL_MASK;
            chan.updateEnvelope = false;
        }
        else if (chan.divider)
        {
            chan.divider--;
        }
        else
        {
            chan.divider = volumeReg & VALUE_VOL_MASK;
            if (chan.decayCounter)
            {
                chan.decayCounter--;
            }
            else if (volumeReg & ENABLE_LOOP_MASK)
            {
                chan.decayCounter = 0x0F;
            }
        }
    }
    if (volumeReg & FIXED_VOL_MASK) // fixed volume
    {
        chan.volume = volumeReg & VALUE_VOL_MASK;
    }
    else
    {
        chan.volume = chan.decayCounter;
    }
    // Countdown len counter if enabled
    if (!(volumeReg & DISABLE_LEN_MASK))
    {
        if (chan.lenCounter && m_halfSignal) // once per frame
        {
            chan.lenCounter--;
        }
    }
    // Sequencer
    // Sweep works only if lengthCounter is non-zero
    if (!chan.lenCounter)
    {
        chan.volume = 0;
        chan.output = chan.volume;
        return;
    }
    // Sweep works
    uint8_t sweepReg = m_regs[APU_SWEEP1 + i * 4];
    if ((sweepReg & SWEEP_ENABLE_MASK) && (sweepReg & SWEEP_RATE_MASK) &&
        chan.period >= (0x008 << (CONST_SHIFT_BITS + 4)) &&
        chan.period <= (0x7FF << (CONST_SHIFT_BITS + 4)))
    {
        if (m_halfSignal)
        {
            if (chan.sweepCounter == (sweepReg & SWEEP_RATE_MASK))
            {
                chan.sweepCounter = 0;
                auto delta = chan.period >> (sweepReg & SWEEP_SHIFT_MASK);
                if (sweepReg & SWEEP_DIR_MASK)
                {
                    delta = ~delta;
                    if (i == 1) delta += 1 << (CONST_SHIFT_BITS + 4);
                }
                delta &= (0xFFFF << (CONST_SHIFT_BITS + 4));
            }
            else
            {
                chan.sweepCounter += 0x10;
            }
        }
    }

    if (chan.period < (0x008 << (CONST_SHIFT_BITS + 4)) ||
        chan.period > (0x7FF << (CONST_SHIFT_BITS + 4)))
    {
        chan.volume = 0;
        chan.output = chan.volume;
        return;
    }

    //chan.counter += counterScaler << 3;
    //while (chan.counter >= chan.period + (1 << (CONST_SHIFT_BITS + 4)))
    //{
    //    chan.sequencer++;
    //    chan.sequencer &= 0x07;
    //    chan.counter -= (chan.period + (1 << (CONST_SHIFT_BITS + 4)));
    //}
    //if (!(sequencerTable[(volumeReg & DUTY_CYCLE_MASK) >> 6] & (1 << chan.sequencer)))
    //{
    //    chan.volume = 0;
    //}
    chan.output = chan.volume;
    chan.duty = (volumeReg & DUTY_CYCLE_MASK) >> 6;
}

void RP2A03::updateTriangleChannel(ChannelInfo& chan)
{
    /*static constexpr uint8_t triangleTable[] =
    {
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
        15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
    };*/
    bool disabled = !(m_regs[APU_STATUS] & TRI_ENABLE_MASK);

    if (disabled)
    {
        chan.output = 0;
        return;
    }
    // Linear counter control
    uint8_t triangleReg = m_regs[APU_TRIANGLE];
    if (m_quaterSignal)
    {
        if (chan.linearReloadFlag)
            chan.linearCounter = triangleReg & 0x7F;
        else if (chan.linearCounter)
            chan.linearCounter--;
        if (!(triangleReg & 0x80))
        {
            chan.linearReloadFlag = false;
        }
    }
    // Length counter control
    if (m_halfSignal && !(triangleReg & 0x80) && chan.lenCounter) // once per frame
    {
        chan.lenCounter--;
    }

    if ((!chan.lenCounter || !chan.linearCounter))
    {
        chan.output = 0;
        return;
    }

    /*chan.counter += counterScaler << 4;
    while (chan.counter >= (chan.period + (1 << (CONST_SHIFT_BITS + 4))))
    {
        chan.sequencer++;
        chan.sequencer &= 0x1F;
        chan.counter -= (chan.period + (1 << (CONST_SHIFT_BITS + 4)));
        chan.volume = triangleTable[chan.sequencer];
    }*/
    chan.output = 1;
}

void RP2A03::updateNoiseChannel(ChannelInfo& chan)
{
    if (!(m_regs[APU_STATUS] & (1 << 3)))
    {
        chan.volume = 0;
        chan.output = chan.volume;
        return;
    }

    uint8_t volumeReg = m_regs[APU_NOISE_VOL];

    // Decay counters are always enabled
    // So, run the envelope anyway
    if (m_quaterSignal)
    {
        if (chan.updateEnvelope)
        {
            chan.decayCounter = 0x0F;
            chan.divider = volumeReg & VALUE_VOL_MASK;
            chan.updateEnvelope = false;
        }
        else if (chan.divider)
        {
            chan.divider--;
        }
        else
        {
            chan.divider = volumeReg & VALUE_VOL_MASK;
            if (chan.decayCounter)
            {
                chan.decayCounter--;
            }
            else if (volumeReg & ENABLE_LOOP_MASK)
            {
                chan.decayCounter = 0x0F;
            }
        }
    }

    if (volumeReg & FIXED_VOL_MASK) // fixed volume
    {
        chan.volume = volumeReg & VALUE_VOL_MASK;
    }
    else
    {
        chan.volume = chan.decayCounter;
    }

    // Countdown len counter if enabled
    if (!(volumeReg & DISABLE_LEN_MASK))
    {
        if (chan.lenCounter && m_halfSignal) // once per frame
        {
            chan.lenCounter--;
        }
    }

    if (!chan.lenCounter)
    {
        chan.volume = 0;
        chan.output = chan.volume;
        return;
    }

    //chan.counter += counterScaler << 3;
    //while (chan.counter >= chan.period + (1 << (CONST_SHIFT_BITS + 4)))
    //{
    //    uint8_t temp;
    //    if (m_regs[APU_NOISE_FREQ] & NOISE_MODE_MASK)
    //    {
    //        // 93-bits
    //        temp = ((m_shiftNoise >> 6) ^ m_shiftNoise) & 1;
    //    }
    //    else
    //    {
    //        // 32768-bits
    //        temp = ((m_shiftNoise >> 1) ^ m_shiftNoise) & 1;
    //    }
    //    m_shiftNoise >>= 1;
    //    m_shiftNoise |= (temp << 14);
    //    chan.counter -= (chan.period + (1 << (CONST_SHIFT_BITS + 4)));
    //}
    //if (m_shiftNoise & 0x01)
    //{
    //    chan.volume = 0;
    //}
    chan.output = chan.volume;
}

void RP2A03::updateDmcChannel(ChannelInfo& info)
{
    //if (info.dmcActive && !info.sequencer)
    //{
    //    if (info.dmcLen == 0)
    //    {
    //        if (m_regs[APU_DMC_DMA_FREQ] & DMC_LOOP_MASK)
    //        {
    //            info.dmcAddr = m_regs[APU_DMC_ADDR] * 0x40 + 0xC000;
    //            info.dmcLen = m_regs[APU_DMC_LEN] * 16 + 1;
    //        }
    //        else
    //        {
    //            info.dmcIrqFlag = !!(m_regs[APU_DMC_DMA_FREQ] & DMC_IRQ_ENABLE_MASK);
    //            info.dmcActive = false;
    //            info.output = (static_cast<uint32_t>(m_dmcVolTable[15]) * info.volume) >> 7;
    //            return;
    //        }
    //    }
    //    info.dmcBuffer = m_cpu->read(info.dmcAddr);
    //    info.sequencer = 8;
    //    info.dmcAddr++;
    //    info.dmcLen--;
    //    if (info.dmcAddr == 0x0000) info.dmcAddr = 0x8000;
    //}

    //if (info.sequencer)
    //{
    //    info.counter += counterScaler;
    //    while (info.counter >= info.period)
    //    {
    //        if (info.dmcBuffer & 1)
    //        {
    //            if (info.volume <= 125) info.volume += 2;
    //        }
    //        else
    //        {
    //            if (info.volume >= 2) info.volume -= 2;
    //        }
    //        info.sequencer--;
    //        info.dmcBuffer >>= 1;
    //        info.counter -= info.period;
    //    }
    //}
    //info.output = (static_cast<uint32_t>(m_dmcVolTable[15]) * info.volume) >> 7;
}
