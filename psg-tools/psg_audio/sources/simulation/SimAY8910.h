#pragma once

#include <memory>
#include <complex>
#include "ChipSim.h"
#include "stream/Chip.h"
#include "processing/ChipClockRateConvert.h"

// The AY8910 of a VGM or a machine: the writes go to the frame as they are.
// A player can write a volume many times over a frame, as a sound of its own:
// a gate of the tone (a pulse of a varying width) or a digital sound. A frame
// keeps one volume, so the simulator gives the volume of the same loudness:
// the level of the fundamental of the tone, or the mean level without the tone.
// Several writes in one update of the player keep the last one.
class SimAY8910 : public ChipSim
{
public:
	SimAY8910();

	void ConfigureClock(int srcClock, int dstClock) override;
	void Reset() override;
	void Write(int chip, Register reg, uint8_t data) override;
	uint8_t Read(int chip, Register reg) override;
	void Simulate(int samples) override;
	void Convert(Frame& frame) override;

private:
	struct Channel
	{
		uint32_t phase; // of the tone, in clocks of the chip from its start
		int  writes;    // of the volume in the frame
		bool envelope;  // a write in the frame selects the envelope
		float firstWrite, lastWrite; // clocks from the start of the frame

		// over the frame: the output at the frequency of the tone (gated
		// tone x level) and its reference (gated tone at full level),
		// the sum of the levels and the time (clocks)
		std::complex<float> output;
		std::complex<float> reference;
		float levelSum;
		float time;
	};

	void ResetFrame();
	uint8_t FrameVolume(int chip, int chan) const;

	Frame m_frame;
	ChipDecoder m_decoder;
	std::unique_ptr<ChipClockRateConvert> m_convert;
	Channel  m_channels[2][3];
	uint64_t m_clockRest; // of the conversion from samples to clocks
};
