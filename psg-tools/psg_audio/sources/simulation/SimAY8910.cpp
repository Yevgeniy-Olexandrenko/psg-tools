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
#ifdef Enable_ChipBus
	m_bus[0].Reset();
	m_bus[1].Reset();
#else
	m_bankB[0] = m_bankB[1] = false;
#endif
}

void SimAY8910::Write(int chip, Register reg, uint8_t data)
{
#ifdef Enable_ChipBus
	m_bus[chip].Receive(reg, data, m_frame[chip]);
#else
	// An AY8930 stream selects the register bank through R13 as the chip does
	// (0xB0 in the high nibble: expanded mode, bank B). In bank B, registers
	// 0x00-0x0C go to 0x10-0x1C of the frame. R13 is in both banks.
	// EncodeVGM writes bank B directly as 0x10-0x1A, as the PSG format.
	if (reg == Register::Mode_Bank)
	{
		m_bankB[chip] = ((data & 0xF0) == 0xB0);
	}
	else if (reg >= Register::BankB_Fst)
	{
		// bank B register as is
	}
	else if (reg > Register::Mode_Bank)
	{
		return; // I/O ports and test registers
	}
	else if (m_bankB[chip])
	{
		reg = Register::Value(Register::BankB_Fst + reg);
	}
	m_frame[chip].Update(reg, data);
#endif
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
