#pragma once

#include <memory>
#include <vector>
#include "ChipSim.h"
#include "stream/Stream.h"

// Prepares the simulation of the chips of a file on the PSGs: it creates the
// simulators and sets the source chips of the stream (stream.schip) from the
// chips the user selects (stream.dchip).
//
// A single AY8910 or SN76489 plays as it is, the processing converts it to
// the chips the user selects: the PSG has the model and the clock of the
// AY8910, or the AY8910 at half the clock of the SN76489 (the SN76489 divides
// the clock by 32, the AY by 16, so the periods stay the same). A dual chip of
// the file takes two PSGs.
//
// A single chip of NES or Game Boy plays on the 1st PSG; its simulator takes
// two PSGs when the user selects two. Two different chips play on two PSGs,
// one on each (SimHybrid). These PSGs take the model, the clock and the output
// the user selects, else: YM2149 for the chips of NES and Game Boy, AY8910 for
// the SN76489, the model from the file for an AY8910; 1.7734 MHz; mono.
namespace SimFactory
{
	struct Source
	{
		ChipSim::Type type;
		int clock; // Hz, as the chip runs (a divider of the chip applied)
		Chip::Model model = Chip::Model::Unknown; // of an AY8910, when the file gives it
		bool dual = false; // two chips of this type (a single AY8910 or SN76489 only)
	};

	// Returns null when the PSGs cannot take the chips: more than two, or two
	// when the user selects one PSG. Then the stream is not changed.
	std::shared_ptr<ChipSim> Create(const std::vector<Source>& sources, Stream& stream);
}
