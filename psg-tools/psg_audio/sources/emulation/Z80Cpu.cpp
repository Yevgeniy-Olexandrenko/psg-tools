#include "Z80Cpu.h"

namespace
{
	enum : uint8_t { CF = 0x01, NF = 0x02, PF = 0x04, XF = 0x08, HF = 0x10, YF = 0x20, ZF = 0x40, SF = 0x80 };

	// the flags S, Z, 5, 3 of a result, and with P the parity
	struct FlagTables
	{
		uint8_t sz[256];
		uint8_t szp[256];

		FlagTables()
		{
			for (int v = 0; v < 256; ++v)
			{
				int bits = 0;
				for (int b = 0; b < 8; ++b) bits += (v >> b) & 1;
				sz[v] = uint8_t((v & (SF | YF | XF)) | (v ? 0 : ZF));
				szp[v] = uint8_t(sz[v] | (bits & 1 ? 0 : PF));
			}
		}
	};
	const FlagTables c_flags;
	const uint8_t* const SZ = c_flags.sz;
	const uint8_t* const SZP = c_flags.szp;
}

Z80Cpu::Z80Cpu(Bus& bus)
	: m_bus(bus)
{
	Reset();
}

void Z80Cpu::Reset()
{
	A = F = B = C = D = E = H = L = 0xFF;
	IXH = IXL = IYH = IYL = 0xFF;
	m_af2 = m_bc2 = m_de2 = m_hl2 = 0xFFFF;
	SP = 0xFFFF;
	PC = 0;
	I = R = IM = 0;
	m_index = m_extra = 0;
	m_iff1 = m_iff2 = false;
	m_eiDelay = m_halted = false;
}

void Z80Cpu::ExchangeAll()
{
	uint16_t t;
	t = AF(); SetAF(m_af2); m_af2 = t;
	t = BC(); SetBC(m_bc2); m_bc2 = t;
	t = DE(); SetDE(m_de2); m_de2 = t;
	t = HL(); SetHL(m_hl2); m_hl2 = t;
}

int Z80Cpu::Step()
{
	m_eiDelay = false;
	IncR();
	if (m_halted) return 4;

	// the prefixes DD and FD select IX or IY, the last one counts
	int prefixes = 0;
	m_index = m_extra = 0;
	uint8_t op = Fetch();
	while (op == 0xDD || op == 0xFD)
	{
		m_index = (op == 0xDD ? 1 : 2);
		prefixes += 4;
		op = Fetch();
		IncR();
	}
	if (op == 0xED)
	{
		m_index = 0;
		return prefixes + ExecED();
	}
	return prefixes + ExecMain(op);
}

int Z80Cpu::Interrupt()
{
	m_halted = false;
	m_iff1 = m_iff2 = false;
	IncR();
	Push(PC);
	if (IM == 2)
	{
		PC = Read16(uint16_t(I << 8 | 0xFF));
		return 19;
	}
	PC = 0x0038;
	return 13;
}

////////////////////////////////////////////////////////////////////////////////

uint16_t Z80Cpu::HLX() const
{
	switch (m_index)
	{
	case 1:  return uint16_t(IXH << 8 | IXL);
	case 2:  return uint16_t(IYH << 8 | IYL);
	default: return HL();
	}
}

void Z80Cpu::SetHLX(uint16_t v)
{
	switch (m_index)
	{
	case 1:  SetIX(v); break;
	case 2:  SetIY(v); break;
	default: SetHL(v); break;
	}
}

uint16_t Z80Cpu::AddrHL()
{
	if (!m_index) return HL();

	// (IX+d) takes 8 T-states more than (HL)
	int8_t d = int8_t(Fetch());
	m_extra = 8;
	return uint16_t(HLX() + d);
}

uint8_t& Z80Cpu::Reg(int r, bool index)
{
	switch (r)
	{
	case 0: return B;
	case 1: return C;
	case 2: return D;
	case 3: return E;
	case 4: return (index && m_index ? (m_index == 1 ? IXH : IYH) : H);
	case 5: return (index && m_index ? (m_index == 1 ? IXL : IYL) : L);
	default: return A;
	}
}

uint8_t Z80Cpu::Get(int r, uint16_t addr)
{
	return (r == 6 ? m_bus.Read(addr) : Reg(r, true));
}

void Z80Cpu::Set(int r, uint16_t addr, uint8_t v)
{
	if (r == 6) m_bus.Write(addr, v); else Reg(r, true) = v;
}

uint16_t Z80Cpu::GetRP(int p) const
{
	switch (p)
	{
	case 0:  return BC();
	case 1:  return DE();
	case 2:  return HLX();
	default: return SP;
	}
}

void Z80Cpu::SetRP(int p, uint16_t v)
{
	switch (p)
	{
	case 0:  SetBC(v); break;
	case 1:  SetDE(v); break;
	case 2:  SetHLX(v); break;
	default: SP = v; break;
	}
}

uint16_t Z80Cpu::GetRP2(int p) const
{
	return (p == 3 ? AF() : GetRP(p));
}

void Z80Cpu::SetRP2(int p, uint16_t v)
{
	if (p == 3) SetAF(v); else SetRP(p, v);
}

bool Z80Cpu::Condition(int y) const
{
	// NZ, Z, NC, C, PO, PE, P, M
	static const uint8_t c_masks[4] = { ZF, CF, PF, SF };
	return bool(F & c_masks[y >> 1]) == bool(y & 1);
}

////////////////////////////////////////////////////////////////////////////////

void Z80Cpu::Alu(int op, uint8_t v)
{
	int c = 0, res;
	switch (op)
	{
	case 1: // ADC
		c = (F & CF);
		[[fallthrough]];
	case 0: // ADD
		res = A + v + c;
		F = uint8_t(SZ[res & 0xFF] | ((res >> 8) & CF) | ((A ^ v ^ res) & HF) | (((A ^ ~v) & (A ^ res) & 0x80) >> 5));
		A = uint8_t(res);
		break;
	case 3: // SBC
		c = (F & CF);
		[[fallthrough]];
	case 2: // SUB
		res = A - v - c;
		F = uint8_t(SZ[res & 0xFF] | NF | ((res >> 8) & CF) | ((A ^ v ^ res) & HF) | (((A ^ v) & (A ^ res) & 0x80) >> 5));
		A = uint8_t(res);
		break;
	case 4: A &= v; F = SZP[A] | HF; break;
	case 5: A ^= v; F = SZP[A]; break;
	case 6: A |= v; F = SZP[A]; break;
	default: // CP: the flags 3 and 5 come from the operand
		res = A - v;
		F = uint8_t((SZ[res & 0xFF] & ~(XF | YF)) | (v & (XF | YF)) | NF | ((res >> 8) & CF) |
			((A ^ v ^ res) & HF) | (((A ^ v) & (A ^ res) & 0x80) >> 5));
		break;
	}
}

uint8_t Z80Cpu::Inc8(uint8_t v)
{
	uint8_t res = uint8_t(v + 1);
	F = uint8_t((F & CF) | SZ[res] | ((res & 0x0F) ? 0 : HF) | (v == 0x7F ? PF : 0));
	return res;
}

uint8_t Z80Cpu::Dec8(uint8_t v)
{
	uint8_t res = uint8_t(v - 1);
	F = uint8_t((F & CF) | NF | SZ[res] | ((res & 0x0F) == 0x0F ? HF : 0) | (v == 0x80 ? PF : 0));
	return res;
}

uint8_t Z80Cpu::Rotate(int op, uint8_t v)
{
	uint8_t c;
	switch (op)
	{
	case 0:  c = v >> 7; v = uint8_t(v << 1 | c); break;              // RLC
	case 1:  c = v & 1;  v = uint8_t(v >> 1 | c << 7); break;         // RRC
	case 2:  c = v >> 7; v = uint8_t(v << 1 | (F & CF)); break;       // RL
	case 3:  c = v & 1;  v = uint8_t(v >> 1 | (F & CF) << 7); break;  // RR
	case 4:  c = v >> 7; v = uint8_t(v << 1); break;                  // SLA
	case 5:  c = v & 1;  v = uint8_t(v >> 1 | (v & 0x80)); break;     // SRA
	case 6:  c = v >> 7; v = uint8_t(v << 1 | 1); break;              // SLL
	default: c = v & 1;  v = uint8_t(v >> 1); break;                  // SRL
	}
	F = SZP[v] | c;
	return v;
}

void Z80Cpu::AddHL(uint16_t v)
{
	uint16_t hl = HLX();
	int res = hl + v;
	F = uint8_t((F & (SF | ZF | PF)) | ((res >> 16) & CF) | (((hl ^ v ^ res) >> 8) & HF) | ((res >> 8) & (XF | YF)));
	SetHLX(uint16_t(res));
}

void Z80Cpu::AdcHL(uint16_t v)
{
	uint16_t hl = HL();
	int res = hl + v + (F & CF);
	F = uint8_t(((res >> 16) & CF) | (((hl ^ v ^ res) >> 8) & HF) | ((res >> 8) & (SF | XF | YF)) |
		((res & 0xFFFF) ? 0 : ZF) | (((~(hl ^ v)) & (hl ^ res) & 0x8000) >> 13));
	SetHL(uint16_t(res));
}

void Z80Cpu::SbcHL(uint16_t v)
{
	uint16_t hl = HL();
	int res = hl - v - (F & CF);
	F = uint8_t(NF | ((res >> 16) & CF) | (((hl ^ v ^ res) >> 8) & HF) | ((res >> 8) & (SF | XF | YF)) |
		((res & 0xFFFF) ? 0 : ZF) | (((hl ^ v) & (hl ^ res) & 0x8000) >> 13));
	SetHL(uint16_t(res));
}

////////////////////////////////////////////////////////////////////////////////

// The opcode fields: x = op[7:6], y = op[5:3], z = op[2:0], p = y[2:1], q = y[0].
// The returned T-states are of the instruction without the prefixes DD and FD.
int Z80Cpu::ExecMain(uint8_t op)
{
	const int x = (op >> 6), y = (op >> 3) & 7, z = (op & 7), p = (y >> 1), q = (y & 1);
	switch (x)
	{
	case 0:
		switch (z)
		{
		case 0:
			switch (y)
			{
			case 0: return 4; // NOP
			case 1: { uint16_t t = AF(); SetAF(m_af2); m_af2 = t; return 4; }
			case 2: { int8_t d = int8_t(Fetch()); if (--B) { PC += d; return 13; } return 8; } // DJNZ
			case 3: { int8_t d = int8_t(Fetch()); PC += d; return 12; }
			default: { int8_t d = int8_t(Fetch()); if (Condition(y - 4)) { PC += d; return 12; } return 7; }
			}
		case 1:
			if (!q) { SetRP(p, Fetch16()); return 10; }
			AddHL(GetRP(p));
			return 11;
		case 2:
			switch (y)
			{
			case 0: m_bus.Write(BC(), A); return 7;
			case 1: A = m_bus.Read(BC()); return 7;
			case 2: m_bus.Write(DE(), A); return 7;
			case 3: A = m_bus.Read(DE()); return 7;
			case 4: Write16(Fetch16(), HLX()); return 16;
			case 5: SetHLX(Read16(Fetch16())); return 16;
			case 6: m_bus.Write(Fetch16(), A); return 13;
			default: A = m_bus.Read(Fetch16()); return 13;
			}
		case 3:
			SetRP(p, uint16_t(GetRP(p) + (q ? -1 : 1)));
			return 6;
		case 4:
		case 5:
		{
			uint16_t addr = (y == 6 ? AddrHL() : 0);
			uint8_t v = Get(y, addr);
			Set(y, addr, z == 4 ? Inc8(v) : Dec8(v));
			return (y == 6 ? 11 + m_extra : 4);
		}
		case 6:
		{
			uint16_t addr = 0;
			if (y == 6)
			{
				// LD (IX+d),n takes 5 T-states more than LD (HL),n
				addr = AddrHL();
				if (m_index) m_extra = 5;
			}
			Set(y, addr, Fetch());
			return (y == 6 ? 10 + m_extra : 7);
		}
		default:
		{
			uint8_t c;
			switch (y)
			{
			case 0: // RLCA
				A = uint8_t(A << 1 | A >> 7);
				F = (F & (SF | ZF | PF)) | (A & (XF | YF | CF));
				break;
			case 1: // RRCA
				c = (A & 1); A = uint8_t(A >> 1 | c << 7);
				F = (F & (SF | ZF | PF)) | c | (A & (XF | YF));
				break;
			case 2: // RLA
				c = (A >> 7); A = uint8_t(A << 1 | (F & CF));
				F = (F & (SF | ZF | PF)) | c | (A & (XF | YF));
				break;
			case 3: // RRA
				c = (A & 1); A = uint8_t(A >> 1 | (F & CF) << 7);
				F = (F & (SF | ZF | PF)) | c | (A & (XF | YF));
				break;
			case 4: // DAA
			{
				uint8_t corr = 0, h;
				c = (F & CF);
				if ((F & HF) || (A & 0x0F) > 9) corr = 0x06;
				if (c || A > 0x99) { corr |= 0x60; c = CF; }
				if (F & NF) { h = ((F & HF) && (A & 0x0F) < 6) ? HF : 0; A -= corr; }
				else { h = ((A & 0x0F) > 9) ? HF : 0; A += corr; }
				F = SZP[A] | (F & NF) | c | h;
				break;
			}
			case 5: // CPL
				A = ~A;
				F = (F & (SF | ZF | PF | CF)) | HF | NF | (A & (XF | YF));
				break;
			case 6: // SCF
				F = (F & (SF | ZF | PF)) | CF | (A & (XF | YF));
				break;
			default: // CCF
				F = (F & (SF | ZF | PF)) | ((F & CF) ? HF : CF) | (A & (XF | YF));
				break;
			}
			return 4;
		}
		}

	case 1:
	{
		if (op == 0x76) // HALT: the CPU executes NOPs until an interrupt
		{
			m_halted = true;
			return 4;
		}
		// with (IX+d), the other operand is H or L, not IXH or IXL
		if (y == 6 || z == 6)
		{
			uint16_t addr = AddrHL();
			if (y == 6) m_bus.Write(addr, Reg(z, false));
			else Reg(y, false) = m_bus.Read(addr);
			return 7 + m_extra;
		}
		Reg(y, true) = Reg(z, true);
		return 4;
	}

	case 2:
	{
		uint16_t addr = (z == 6 ? AddrHL() : 0);
		Alu(y, Get(z, addr));
		return (z == 6 ? 7 + m_extra : 4);
	}

	default:
		switch (z)
		{
		case 0:
			if (Condition(y)) { PC = Pop(); return 11; }
			return 5;
		case 1:
			if (!q) { SetRP2(p, Pop()); return 10; }
			switch (p)
			{
			case 0: PC = Pop(); return 10;
			case 1:
			{
				uint16_t t;
				t = BC(); SetBC(m_bc2); m_bc2 = t;
				t = DE(); SetDE(m_de2); m_de2 = t;
				t = HL(); SetHL(m_hl2); m_hl2 = t;
				return 4;
			}
			case 2: PC = HLX(); return 4;
			default: SP = HLX(); return 6;
			}
		case 2:
		{
			uint16_t nn = Fetch16();
			if (Condition(y)) PC = nn;
			return 10;
		}
		case 3:
			switch (y)
			{
			case 0: PC = Fetch16(); return 10;
			case 1: return ExecCB();
			case 2: { uint8_t n = Fetch(); m_bus.WritePort(uint16_t(A << 8 | n), A); return 11; }
			case 3: { uint8_t n = Fetch(); A = m_bus.ReadPort(uint16_t(A << 8 | n)); return 11; }
			case 4: { uint16_t v = Read16(SP); Write16(SP, HLX()); SetHLX(v); return 19; }
			case 5: { uint16_t t = DE(); SetDE(HL()); SetHL(t); return 4; } // EX DE,HL: never IX
			case 6: m_iff1 = m_iff2 = false; return 4;
			default: m_iff1 = m_iff2 = true; m_eiDelay = true; return 4;
			}
		case 4:
		{
			uint16_t nn = Fetch16();
			if (Condition(y)) { Push(PC); PC = nn; return 17; }
			return 10;
		}
		case 5:
			if (!q) { Push(GetRP2(p)); return 11; }
			if (!p) { uint16_t nn = Fetch16(); Push(PC); PC = nn; return 17; }
			return 4; // the prefixes DD, ED, FD: Step takes them
		case 6:
			Alu(y, Fetch());
			return 7;
		default: // RST
			Push(PC);
			PC = uint16_t(y << 3);
			return 11;
		}
	}
}

// CB: rotations and bits. DD CB d op: the operand is (IX+d), and the result
// of a rotation, RES or SET also goes to the register of op (undocumented).
int Z80Cpu::ExecCB()
{
	uint16_t addr;
	uint8_t op;
	if (m_index)
	{
		addr = AddrHL();
		op = Fetch();
	}
	else
	{
		op = Fetch();
		IncR();
		addr = HL();
	}

	const int x = (op >> 6), y = (op >> 3) & 7, z = (op & 7);
	const bool memory = (m_index || z == 6);
	uint8_t v = (memory ? m_bus.Read(addr) : Reg(z, false));

	if (x == 1) // BIT: the flags 3 and 5 of (HL) come from MEMPTR, here from the address
	{
		uint8_t bit = (v & (1 << y));
		F = uint8_t((F & CF) | HF | (bit ? (bit & SF) : (ZF | PF)) | ((memory ? (addr >> 8) : v) & (XF | YF)));
		return (m_index ? 16 : memory ? 12 : 8);
	}

	switch (x)
	{
	case 0:  v = Rotate(y, v); break;
	case 2:  v &= ~(1 << y); break;
	default: v |= (1 << y); break;
	}
	if (memory)
	{
		m_bus.Write(addr, v);
		if (m_index && z != 6) Reg(z, false) = v;
	}
	else Reg(z, false) = v;
	return (m_index ? 19 : memory ? 15 : 8);
}

// ED: the returned T-states include the prefix ED. Unused opcodes are NOPs.
int Z80Cpu::ExecED()
{
	uint8_t op = Fetch();
	IncR();

	const int x = (op >> 6), y = (op >> 3) & 7, z = (op & 7), p = (y >> 1), q = (y & 1);
	if (x == 2 && z <= 3 && y >= 4) return ExecBlock(y, z);
	if (x != 1) return 8;

	switch (z)
	{
	case 0: // IN r,(C); IN (C) sets only the flags
	{
		uint8_t v = m_bus.ReadPort(BC());
		if (y != 6) Reg(y, false) = v;
		F = (F & CF) | SZP[v];
		return 12;
	}
	case 1:
		m_bus.WritePort(BC(), y == 6 ? 0 : Reg(y, false));
		return 12;
	case 2:
		if (q) AdcHL(GetRP(p)); else SbcHL(GetRP(p));
		return 15;
	case 3:
		if (q) SetRP(p, Read16(Fetch16())); else Write16(Fetch16(), GetRP(p));
		return 20;
	case 4: // NEG
	{
		uint8_t v = A;
		A = 0;
		Alu(2, v);
		return 8;
	}
	case 5: // RETN, RETI
		m_iff1 = m_iff2;
		PC = Pop();
		return 14;
	case 6:
	{
		static const uint8_t c_modes[8] = { 0, 0, 1, 2, 0, 0, 1, 2 };
		IM = c_modes[y];
		return 8;
	}
	default:
		switch (y)
		{
		case 0: I = A; return 9;
		case 1: R = A; return 9;
		case 2: A = I; F = (F & CF) | SZ[A] | (m_iff2 ? PF : 0); return 9;
		case 3: A = R; F = (F & CF) | SZ[A] | (m_iff2 ? PF : 0); return 9;
		case 4: // RRD
		{
			uint8_t v = m_bus.Read(HL());
			m_bus.Write(HL(), uint8_t(A << 4 | v >> 4));
			A = (A & 0xF0) | (v & 0x0F);
			F = (F & CF) | SZP[A];
			return 18;
		}
		case 5: // RLD
		{
			uint8_t v = m_bus.Read(HL());
			m_bus.Write(HL(), uint8_t(v << 4 | (A & 0x0F)));
			A = (A & 0xF0) | (v >> 4);
			F = (F & CF) | SZP[A];
			return 18;
		}
		default: return 8;
		}
	}
}

// LDI, CPI, INI, OUTI and their D, R and DR forms. A repeat moves PC back
// to the instruction and takes 21 T-states.
int Z80Cpu::ExecBlock(int y, int z)
{
	const int dir = ((y & 1) ? -1 : 1);
	const bool repeat = (y >= 6);
	const uint16_t hl = HL();
	bool again = false;

	switch (z)
	{
	case 0: // LDI
	{
		uint8_t v = m_bus.Read(hl);
		m_bus.Write(DE(), v);
		SetHL(uint16_t(hl + dir));
		SetDE(uint16_t(DE() + dir));
		SetBC(uint16_t(BC() - 1));
		uint8_t n = uint8_t(v + A);
		F = uint8_t((F & (SF | ZF | CF)) | (BC() ? PF : 0) | (n & XF) | ((n << 4) & YF));
		again = (BC() != 0);
		break;
	}
	case 1: // CPI
	{
		uint8_t v = m_bus.Read(hl);
		uint8_t res = uint8_t(A - v);
		uint8_t h = ((A ^ v ^ res) & HF);
		SetHL(uint16_t(hl + dir));
		SetBC(uint16_t(BC() - 1));
		uint8_t n = uint8_t(res - (h ? 1 : 0));
		F = uint8_t((F & CF) | NF | (SZ[res] & ~(XF | YF)) | h | (BC() ? PF : 0) | (n & XF) | ((n << 4) & YF));
		again = (BC() != 0 && res != 0);
		break;
	}
	case 2: // INI: the port has B before the decrement
	{
		uint8_t v = m_bus.ReadPort(BC());
		m_bus.Write(hl, v);
		SetHL(uint16_t(hl + dir));
		--B;
		unsigned k = v + uint8_t(C + dir);
		F = uint8_t(SZ[B] | ((v >> 6) & NF) | (k > 255 ? (HF | CF) : 0) | (SZP[(k & 7) ^ B] & PF));
		again = (B != 0);
		break;
	}
	default: // OUTI: the port has B after the decrement
	{
		uint8_t v = m_bus.Read(hl);
		--B;
		m_bus.WritePort(BC(), v);
		SetHL(uint16_t(hl + dir));
		unsigned k = v + L;
		F = uint8_t(SZ[B] | ((v >> 6) & NF) | (k > 255 ? (HF | CF) : 0) | (SZP[(k & 7) ^ B] & PF));
		again = (B != 0);
		break;
	}
	}

	if (repeat && again)
	{
		PC -= 2;
		return 21;
	}
	return 16;
}
