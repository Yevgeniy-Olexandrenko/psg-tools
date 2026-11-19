#pragma once

#include <cstdint>
#include "Machine.h"

// The 6502 CPU of the NES (RP2A03): no decimal mode. It executes one
// instruction at a time and counts its cycles, with the extra cycles of page
// crossings and branches. It has the stable undocumented opcodes of the NMOS
// 6502 (LAX, SAX, DCP, ISC, SLO, RLA, SRE, RRA, ANC, ALR, ARR, AXS, NOPs); the
// unstable ones (SHA, SHX, SHY, TAS, LAS, XAA) behave as on most chips. The
// opcodes that jam the CPU stop it until Reset.
class RP2A03Cpu
{
public:
	enum class Clock
	{
		NTSC = 1789773, // Hz
		PAL  = 1662607  // Hz
	};

	explicit RP2A03Cpu(Bus& bus);

	// Sets the registers to their state after power on and PC to the reset vector.
	void Reset();

	// Executes one instruction, returns its cycles. A jammed CPU takes 2 cycles.
	int Step();

	// IRQ (only when the flag I is clear) and NMI. They return the cycles taken.
	int  IRQ();
	int  NMI();
	bool IsJammed() const { return m_jammed; }

	// The machine sets the registers to call the code of a music file.
	uint16_t PC = 0;
	uint8_t  A = 0, X = 0, Y = 0, S = 0xFD, P = 0x24;

	void Push(uint8_t v) { m_bus.Write(uint16_t(0x100 | S--), v); }
	uint8_t Pop() { return m_bus.Read(uint16_t(0x100 | ++S)); }

private:
	uint8_t  Fetch() { return m_bus.Read(PC++); }
	uint16_t Fetch16() { uint8_t l = Fetch(); return uint16_t(l | Fetch() << 8); }
	uint16_t Read16(uint16_t addr) { uint8_t l = m_bus.Read(addr); return uint16_t(l | m_bus.Read(uint16_t(addr + 1)) << 8); }

	uint16_t Address(char mode, bool& crossed);
	int  Branch(bool condition);
	int  Interrupt(uint16_t vector, bool brk);

	void SetNZ(uint8_t v);
	void SetFlag(uint8_t flag, bool on) { P = (on ? P | flag : P & ~flag); }
	void Adc(uint8_t v);
	void Compare(uint8_t reg, uint8_t v);
	uint8_t Asl(uint8_t v);
	uint8_t Lsr(uint8_t v);
	uint8_t Rol(uint8_t v);
	uint8_t Ror(uint8_t v);

private:
	Bus& m_bus;
	bool m_jammed = false;
};
