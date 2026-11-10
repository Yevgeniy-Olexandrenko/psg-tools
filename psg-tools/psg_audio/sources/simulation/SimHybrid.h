#pragma once

#include <memory>
#include "ChipSim.h"

// Simulates two different chips on two PSGs: the 1st chip on chip 0 of the
// frame, the 2nd chip on chip 1. Each simulator writes chip 0 of its own
// frame, as for one PSG.
class SimHybrid : public ChipSim
{
public:
	SimHybrid(std::shared_ptr<ChipSim> sim0, std::shared_ptr<ChipSim> sim1);

	ChipSim* Find(Type type) override;

	void Reset() override;
	void Write(int chip, Register reg, uint8_t data) override;
	void Simulate(int samples) override;
	void Convert(Frame& frame) override;

private:
	std::shared_ptr<ChipSim> m_sims[2];
	Frame m_frames[2]; // of the simulators, kept from frame to frame
};
