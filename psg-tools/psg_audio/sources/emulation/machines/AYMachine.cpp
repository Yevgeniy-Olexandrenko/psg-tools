#include <algorithm>
#include <cstring>
#include "AYMachine.h"
#include "simulation/SimAY8910.h"

namespace
{
	const uint32_t k_frameTacts = 70000; // 3.5 MHz / 50 Hz
	const uint32_t k_intLength  = 32;
	const int      k_frameRate  = 50;
	const uint32_t k_cpuClock   = k_frameTacts * k_frameRate;

	// the clocks of the AY: ZX Spectrum 128, Amstrad CPC
	const int k_zxChipClock  = 1773400;
	const int k_cpcChipClock = 1000000;

	// The code at address 0 (as the format describes): DI; CALL init; then each frame
	// IM 2; EI; HALT; JR (the song sets its own interrupt handler), or
	// IM 1; EI; HALT; CALL interrupt; JR.
	const uint8_t c_loaderIM2[] = { 0xF3, 0xCD, 0, 0, 0xED, 0x5E, 0xFB, 0x76, 0x18, 0xFA };
	const uint8_t c_loaderIM1[] = { 0xF3, 0xCD, 0, 0, 0xED, 0x56, 0xFB, 0x76, 0xCD, 0, 0, 0x18, 0xF7 };
}

AYMachine::AYMachine()
	: m_cpu(*this)
{
	Map(0x0000, 0x10000, m_memory, m_memory);

	ConfigureTiming(std::make_shared<SimAY8910>(), k_cpuClock, k_frameRate);
}

void AYMachine::Load(const Song& song)
{
	// the memory as the format describes it: RET in the first page, with
	// EI; RET at the IM 1 handler 0x38, 0xFF up to 0x3FFF, zeros above
	memset(m_memory + 0x0000, 0xC9, 0x0100);
	memset(m_memory + 0x0100, 0xFF, 0x3F00);
	memset(m_memory + 0x4000, 0x00, 0xC000);
	m_memory[0x38] = 0xFB;

	if (song.interrupt)
	{
		memcpy(m_memory, c_loaderIM1, sizeof(c_loaderIM1));
		m_memory[9] = uint8_t(song.interrupt);
		m_memory[10] = uint8_t(song.interrupt >> 8);
	}
	else
	{
		memcpy(m_memory, c_loaderIM2, sizeof(c_loaderIM2));
	}

	// without the init address, the code starts at the first block
	uint16_t init = song.init;
	if (!init && !song.blocks.empty()) init = song.blocks.front().addr;
	m_memory[2] = uint8_t(init);
	m_memory[3] = uint8_t(init >> 8);

	for (const Block& block : song.blocks)
	{
		uint32_t size = std::min<uint32_t>(block.size, 0x10000 - block.addr);
		memcpy(m_memory + block.addr, block.data, size);
	}

	// all register pairs, the alternate ones too, start with hiReg:loReg
	uint16_t pair = uint16_t(song.hiReg << 8 | song.loReg);
	m_cpu.Reset();
	for (int bank = 0; bank < 2; ++bank)
	{
		m_cpu.SetAF(pair);
		m_cpu.SetBC(pair);
		m_cpu.SetDE(pair);
		m_cpu.SetHL(pair);
		m_cpu.ExchangeAll();
	}
	m_cpu.SetIX(pair);
	m_cpu.SetIY(pair);
	m_cpu.I = 3;
	m_cpu.SP = song.stack;
	m_cpu.PC = 0;
	m_cycle = 0;

	m_system = System::Unknown;
	m_register = 0;
	m_cpcData = m_cpcFunction = 0;
	m_sim->ConfigureClock(k_zxChipClock, k_zxChipClock);
	m_sim->Reset();
	StartFrames();
}

void AYMachine::Execute(uint64_t cycle)
{
	while (m_cycle < cycle && !HasPendingEvent())
	{
		// the interrupt comes at the start of each frame of the hardware
		uint32_t tact = uint32_t(m_cycle % k_frameTacts);
		if (tact < k_intLength && m_cpu.CanInterrupt())
		{
			EndUpdate();
			m_cycle += m_cpu.Interrupt();
		}
		else if (m_cpu.IsHalted() && tact + 4 >= k_intLength)
		{
			// HALT executes NOPs of 4 T-states up to the next interrupt
			uint64_t until = std::min<uint64_t>(m_cycle - tact + k_frameTacts, cycle);
			m_cycle += ((until - m_cycle + 3) & ~uint64_t(3));
		}
		else
		{
			m_cycle += m_cpu.Step();
		}
	}
}

// ZX Spectrum 128: port 0xFFFD selects the register, port 0xBFFD writes it,
// only the address lines 15, 14 and 1 count. Amstrad CPC: the PPI port A
// (0xF4xx) holds the data, the bits 7-6 of the PPI port C (0xF6xx) select the
// function: 0xC0 selects the register, 0x80 writes it, 0x00 ends the function.
// The beeper (ZX port 0xFE) is not supported.
void AYMachine::WritePort(uint16_t port, uint8_t data)
{
	const uint8_t high = (port >> 8);
	if (m_system != System::ZX && (high == 0xF4 || high == 0xF6))
	{
		if (high == 0xF4)
		{
			m_cpcData = data;
		}
		else if (!m_cpcFunction)
		{
			m_cpcFunction = (data & 0xC0);
		}
		else if (!(data & 0xC0))
		{
			if (m_cpcFunction == 0xC0)
			{
				m_register = m_cpcData;
			}
			else if (m_cpcFunction == 0x80 && m_register < 14)
			{
				if (m_system != System::CPC) m_sim->ConfigureClock(k_cpcChipClock, k_cpcChipClock);
				m_system = System::CPC;
				WriteAY(m_cpcData);
			}
			m_cpcFunction = 0;
		}
	}
	else if (m_system != System::CPC)
	{
		if ((port & 0xC002) == 0xC000)
		{
			m_system = System::ZX;
			m_register = data;
		}
		else if ((port & 0xC002) == 0x8000 && m_register < 14)
		{
			m_system = System::ZX;
			WriteAY(data);
		}
	}
}

uint8_t AYMachine::ReadPort(uint16_t port)
{
	// the players read the registers to change some bits (the mixer, a volume)
	if (m_system != System::CPC && (port & 0xC002) == 0xC000)
	{
		return ReadChip(0, Register(m_register));
	}
	return 0xFF;
}

void AYMachine::WriteAY(uint8_t data)
{
	// the AY-3-8910 has 4 bits of the envelope shape: the upper bits
	// must not select the expanded mode of the AY8930 in the frame
	if (m_register == Register::E_Shape) data &= 0x0F;
	WriteChip(0, Register(m_register), data);
}
