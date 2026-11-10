#pragma once

#include <memory>
#include "ChipSim.h"
#include "processing/ChipClockRateConvert.h"
#include "stream/ChipBus.h"

class SimAY8910 : public ChipSim
{
public:
	SimAY8910();

	void ConfigureClock(int srcClock, int dstClock) override;
	void Reset() override;
	void Write(int chip, Register reg, uint8_t data) override;
	void Simulate(int samples) override;
	void Convert(Frame& frame) override;

private:
	Frame m_frame;
#ifdef Enable_ChipBus
	ChipBus m_bus[2];
#else
	bool m_bankB[2];
#endif
	std::unique_ptr<ChipClockRateConvert> m_convert;
};