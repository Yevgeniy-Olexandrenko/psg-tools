#pragma once

#include <cstdint>
#include "Machine.h"

// Z80 CPU: executes one instruction at a time and counts its T-states.
// It has the documented instructions and the undocumented ones that music
// players use: IXH/IXL/IYH/IYL, SLL, DDCB/FDCB with a copy to a register,
// the flags 3 and 5. It does not have MEMPTR (the flags 3 and 5 of BIT n,(HL)).
class Z80Cpu
{
public:
	explicit Z80Cpu(Bus& bus);

	void Reset();

	// Executes one instruction (all its prefixes), returns its T-states.
	// A halted CPU executes NOPs of 4 T-states.
	int Step();

	// The maskable interrupt: the machine calls it when CanInterrupt() is true.
	// IM 0 executes RST 38h, as the 0xFF on the data bus of the ZX Spectrum.
	int  Interrupt();
	bool CanInterrupt() const { return m_iff1 && !m_eiDelay; }
	bool IsHalted() const { return m_halted; }
	bool IsInterruptEnabled() const { return m_iff1; }

	// The machine sets the registers before it runs the code.
	void SetAF(uint16_t v) { A = v >> 8; F = uint8_t(v); }
	void SetBC(uint16_t v) { B = v >> 8; C = uint8_t(v); }
	void SetDE(uint16_t v) { D = v >> 8; E = uint8_t(v); }
	void SetHL(uint16_t v) { H = v >> 8; L = uint8_t(v); }
	void SetIX(uint16_t v) { IXH = v >> 8; IXL = uint8_t(v); }
	void SetIY(uint16_t v) { IYH = v >> 8; IYL = uint8_t(v); }
	void ExchangeAll(); // EX AF,AF' and EXX

	uint16_t GetAF() const { return AF(); }
	uint16_t GetBC() const { return BC(); }
	uint16_t GetDE() const { return DE(); }
	uint16_t GetHL() const { return HL(); }

	uint16_t SP = 0, PC = 0;
	uint8_t  I = 0, R = 0, IM = 0;

private:
	uint8_t  Fetch() { return m_bus.Read(PC++); }
	uint16_t Fetch16() { uint8_t l = Fetch(); return uint16_t(l | Fetch() << 8); }
	uint16_t Read16(uint16_t addr) { uint8_t l = m_bus.Read(addr); return uint16_t(l | m_bus.Read(uint16_t(addr + 1)) << 8); }
	void     Write16(uint16_t addr, uint16_t v) { m_bus.Write(addr, uint8_t(v)); m_bus.Write(uint16_t(addr + 1), uint8_t(v >> 8)); }
	void     Push(uint16_t v) { SP -= 2; Write16(SP, v); }
	uint16_t Pop() { uint16_t v = Read16(SP); SP += 2; return v; }
	void     IncR() { R = (R & 0x80) | ((R + 1) & 0x7F); }

	uint16_t BC() const { return uint16_t(B << 8 | C); }
	uint16_t DE() const { return uint16_t(D << 8 | E); }
	uint16_t HL() const { return uint16_t(H << 8 | L); }
	uint16_t AF() const { return uint16_t(A << 8 | F); }

	// HL, IX or IY by the prefix
	uint16_t HLX() const;
	void     SetHLX(uint16_t v);
	// the address of (HL) or (IX+d), the displacement is fetched here
	uint16_t AddrHL();

	// r: B, C, D, E, H, L, (HL), A; H and L are IXH/IXL or IYH/IYL by the prefix
	// unless the other operand is (IX+d). addr is the address of (HL) or (IX+d).
	uint8_t& Reg(int r, bool index);
	uint8_t  Get(int r, uint16_t addr);
	void     Set(int r, uint16_t addr, uint8_t v);

	// rp: BC, DE, HL, SP; rp2: BC, DE, HL, AF
	uint16_t GetRP(int p) const;
	void     SetRP(int p, uint16_t v);
	uint16_t GetRP2(int p) const;
	void     SetRP2(int p, uint16_t v);
	bool     Condition(int y) const;

	void    Alu(int op, uint8_t v);
	uint8_t Inc8(uint8_t v);
	uint8_t Dec8(uint8_t v);
	uint8_t Rotate(int op, uint8_t v);
	void    AddHL(uint16_t v);
	void    AdcHL(uint16_t v);
	void    SbcHL(uint16_t v);

	int ExecMain(uint8_t op);
	int ExecCB();
	int ExecED();
	int ExecBlock(int y, int z);

private:
	Bus& m_bus;

	uint8_t A, F, B, C, D, E, H, L;
	uint8_t IXH, IXL, IYH, IYL;
	uint16_t m_af2, m_bc2, m_de2, m_hl2;

	int  m_index;   // 0: HL, 1: IX, 2: IY
	int  m_extra;   // T-states of (IX+d) in this instruction
	bool m_iff1, m_iff2;
	bool m_eiDelay; // EI: the interrupt waits one instruction
	bool m_halted;
};
