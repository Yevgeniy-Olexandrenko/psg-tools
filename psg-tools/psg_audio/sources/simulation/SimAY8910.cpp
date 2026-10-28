#include "SimAY8910.h"

SimAY8910::SimAY8910()
	: ChipSim(Type::AY8910)
{
	Reset();
}

void SimAY8910::ConfigureClock(int srcClock, int dstClock)
{
	ChipSim::ConfigureClock(srcClock, dstClock);

	// always process both chips: an unused second chip holds only
	// zeros, they stay zeros after conversion and are never changed
	m_convert.reset(new ChipClockRateConvert(srcClock, dstClock, 2));
}

void SimAY8910::Reset()
{
	m_frame.ResetData();
	m_frame.ResetChanges();
}

void SimAY8910::Write(int chip, Register reg, uint8_t data)
{
	m_frame[chip].Update(reg, data);
}

void SimAY8910::Simulate(int samples)
{
	// do nothing
}

void SimAY8910::Convert(Frame& frame)
{
	// convert the source clock rate to the closest one supported by chip,
	// the changes in the converted frame are not reliable, but += takes
	// only its data and recomputes the changes against the previous frame
	frame += (m_convert ? m_convert->Execute(m_frame) : m_frame);
	m_frame.ResetChanges();
}
