#pragma once

#include <cassert>
#include "FrameProcessor.h"

class ChannelsLayoutChange : public FrameProcessor
{
    Chip m_chip;

public:
    ChannelsLayoutChange(const Chip& dstChip)
        : m_chip(dstChip)
    {}

#ifdef Enable_ChannelsLayoutChange
	const Frame& Execute(const Frame& frame) override
    {
        assert(m_chip.outputKnown());
        assert(m_chip.stereoKnown());

        if (m_chip.output() == Chip::Output::Stereo && m_chip.stereo() != Chip::Stereo::ABC)
        {
            const auto SwapChannels = [&](int chip, int L, int R)
            {
                Frame::Channel chL, chR;
                m_frame[chip].Read(L, chL);
                m_frame[chip].Read(R, chR);
                m_frame[chip].Update(L, chR);
                m_frame[chip].Update(R, chL);
            };

            Update(frame);
            for (int chip = 0; chip < m_chip.count(); ++chip)
            {
                switch (m_chip.stereo())
                {
                case Chip::Stereo::ACB:
                    SwapChannels(chip, Frame::Channel::B, Frame::Channel::C);
                    break;

                case Chip::Stereo::BAC:
                    SwapChannels(chip, Frame::Channel::A, Frame::Channel::B);
                    break;

                case Chip::Stereo::BCA:
                    SwapChannels(chip, Frame::Channel::A, Frame::Channel::B);
                    SwapChannels(chip, Frame::Channel::B, Frame::Channel::C);
                    break;

                case Chip::Stereo::CAB:
                    SwapChannels(chip, Frame::Channel::B, Frame::Channel::C);
                    SwapChannels(chip, Frame::Channel::A, Frame::Channel::B);
                    break;

                case Chip::Stereo::CBA:
                    SwapChannels(chip, Frame::Channel::A, Frame::Channel::C);
                    break;
                }
            }
            return m_frame;
        }
        return frame;
    }
#endif
};
