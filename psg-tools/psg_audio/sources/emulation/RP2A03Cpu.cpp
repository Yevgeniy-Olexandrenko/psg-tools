#include "RP2A03Cpu.h"

namespace
{
	enum : uint8_t { CF = 0x01, ZF = 0x02, IF = 0x04, DF = 0x08, BF = 0x10, UF = 0x20, VF = 0x40, NF = 0x80 };

	// The addressing modes by opcode: i implied or accumulator, # immediate,
	// z zp, x zp,X, y zp,Y, a abs, X abs,X, Y abs,Y, n (abs), I (zp,X), J (zp),Y,
	// r relative.
	const char c_modes[] =
		"iIiIzzzzi#i#aaaa" "rJiJxxxxiYiYXXXX"
		"aIiIzzzzi#i#aaaa" "rJiJxxxxiYiYXXXX"
		"iIiIzzzzi#i#aaaa" "rJiJxxxxiYiYXXXX"
		"iIiIzzzzi#i#naaa" "rJiJxxxxiYiYXXXX"
		"#I#Izzzzi#i#aaaa" "rJiJxxyyiYiYXXYY"
		"#I#Izzzzi#i#aaaa" "rJiJxxyyiYiYXXYY"
		"#I#Izzzzi#i#aaaa" "rJiJxxxxiYiYXXXX"
		"#I#Izzzzi#i#aaaa" "rJiJxxxxiYiYXXXX";

	// The cycles by opcode, without page crossings and taken branches.
	const uint8_t c_cycles[256] =
	{
		7,6,2,8,3,3,5,5,3,2,2,2,4,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
		6,6,2,8,3,3,5,5,4,2,2,2,4,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
		6,6,2,8,3,3,5,5,3,2,2,2,3,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
		6,6,2,8,3,3,5,5,4,2,2,2,5,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
		2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4, 2,6,2,6,4,4,4,4,2,5,2,5,5,5,5,5,
		2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4, 2,5,2,5,4,4,4,4,2,4,2,4,4,4,4,4,
		2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
		2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
	};
}

RP2A03Cpu::RP2A03Cpu(Bus& bus)
	: m_bus(bus)
{
}

void RP2A03Cpu::Reset()
{
	A = X = Y = 0;
	S = 0xFD;
	P = (IF | UF);
	PC = Read16(0xFFFC);
	m_jammed = false;
}

int RP2A03Cpu::IRQ()
{
	return (P & IF ? 0 : Interrupt(0xFFFE, false));
}

int RP2A03Cpu::NMI()
{
	return Interrupt(0xFFFA, false);
}

int RP2A03Cpu::Interrupt(uint16_t vector, bool brk)
{
	Push(uint8_t(PC >> 8));
	Push(uint8_t(PC));
	Push(uint8_t(brk ? (P | UF | BF) : ((P | UF) & ~BF)));
	P |= IF;
	PC = Read16(vector);
	return 7;
}

uint16_t RP2A03Cpu::Address(char mode, bool& crossed)
{
	uint16_t base, addr;
	switch (mode)
	{
	case '#': return PC++;
	case 'z': return Fetch();
	case 'x': return uint8_t(Fetch() + X);
	case 'y': return uint8_t(Fetch() + Y);
	case 'a': return Fetch16();
	case 'X':
		base = Fetch16();
		addr = uint16_t(base + X);
		crossed = ((base ^ addr) & 0xFF00) != 0;
		return addr;
	case 'Y':
		base = Fetch16();
		addr = uint16_t(base + Y);
		crossed = ((base ^ addr) & 0xFF00) != 0;
		return addr;
	case 'n': // the 6502 takes the high byte from the same page
		base = Fetch16();
		return uint16_t(m_bus.Read(base) | m_bus.Read(uint16_t((base & 0xFF00) | ((base + 1) & 0xFF))) << 8);
	case 'I':
	{
		uint8_t zp = uint8_t(Fetch() + X);
		return uint16_t(m_bus.Read(zp) | m_bus.Read(uint8_t(zp + 1)) << 8);
	}
	case 'J':
	{
		uint8_t zp = Fetch();
		base = uint16_t(m_bus.Read(zp) | m_bus.Read(uint8_t(zp + 1)) << 8);
		addr = uint16_t(base + Y);
		crossed = ((base ^ addr) & 0xFF00) != 0;
		return addr;
	}
	default: return 0;
	}
}

int RP2A03Cpu::Branch(bool condition)
{
	int8_t offset = int8_t(Fetch());
	if (!condition) return 0;

	uint16_t target = uint16_t(PC + offset);
	int cycles = (((target ^ PC) & 0xFF00) ? 2 : 1);
	PC = target;
	return cycles;
}

void RP2A03Cpu::SetNZ(uint8_t v)
{
	P = (P & ~(NF | ZF)) | (v & NF) | (v ? 0 : ZF);
}

void RP2A03Cpu::Adc(uint8_t v)
{
	unsigned sum = A + v + (P & CF);
	SetFlag(VF, (~(A ^ v) & (A ^ sum) & 0x80) != 0);
	SetFlag(CF, sum > 0xFF);
	A = uint8_t(sum);
	SetNZ(A);
}

void RP2A03Cpu::Compare(uint8_t reg, uint8_t v)
{
	SetFlag(CF, reg >= v);
	SetNZ(uint8_t(reg - v));
}

uint8_t RP2A03Cpu::Asl(uint8_t v) { SetFlag(CF, v & 0x80); v <<= 1; SetNZ(v); return v; }
uint8_t RP2A03Cpu::Lsr(uint8_t v) { SetFlag(CF, v & 0x01); v >>= 1; SetNZ(v); return v; }
uint8_t RP2A03Cpu::Rol(uint8_t v) { uint8_t c = (P & CF); SetFlag(CF, v & 0x80); v = uint8_t(v << 1 | c); SetNZ(v); return v; }
uint8_t RP2A03Cpu::Ror(uint8_t v) { uint8_t c = (P & CF); SetFlag(CF, v & 0x01); v = uint8_t(v >> 1 | c << 7); SetNZ(v); return v; }

int RP2A03Cpu::Step()
{
	if (m_jammed) return 2;

	const uint8_t op = Fetch();
	const char mode = c_modes[op];
	bool crossed = false;
	const uint16_t addr = Address(mode, crossed);

	// a read with abs,X, abs,Y or (zp),Y takes a cycle more on a page crossing
	int cycles = c_cycles[op];
	if (crossed && cycles == (mode == 'J' ? 5 : 4)) ++cycles;

	// read-modify-write of the memory, or of A in the accumulator mode
	auto Modify = [&](uint8_t(RP2A03Cpu::*func)(uint8_t)) -> uint8_t
	{
		if (mode == 'i') return (A = (this->*func)(A));
		uint8_t v = (this->*func)(m_bus.Read(addr));
		m_bus.Write(addr, v);
		return v;
	};
	auto Load = [&]() { return m_bus.Read(addr); };
	auto Change = [&](int delta)
	{
		uint8_t v = uint8_t(m_bus.Read(addr) + delta);
		m_bus.Write(addr, v);
		return v;
	};
	auto Store = [&](uint8_t v) { m_bus.Write(addr, v); };
	// SHA, SHX, SHY, TAS store the value AND the high byte of the address + 1
	auto StoreHigh = [&](uint8_t v) { m_bus.Write(addr, uint8_t(v & ((addr >> 8) + 1))); };

	switch (op)
	{
	// loads and stores
	case 0xA9: case 0xA5: case 0xB5: case 0xAD: case 0xBD: case 0xB9: case 0xA1: case 0xB1:
		SetNZ(A = Load()); break;
	case 0xA2: case 0xA6: case 0xB6: case 0xAE: case 0xBE:
		SetNZ(X = Load()); break;
	case 0xA0: case 0xA4: case 0xB4: case 0xAC: case 0xBC:
		SetNZ(Y = Load()); break;
	case 0x85: case 0x95: case 0x8D: case 0x9D: case 0x99: case 0x81: case 0x91:
		Store(A); break;
	case 0x86: case 0x96: case 0x8E:
		Store(X); break;
	case 0x84: case 0x94: case 0x8C:
		Store(Y); break;

	// transfers
	case 0xAA: SetNZ(X = A); break;
	case 0xA8: SetNZ(Y = A); break;
	case 0xBA: SetNZ(X = S); break;
	case 0x8A: SetNZ(A = X); break;
	case 0x9A: S = X; break;
	case 0x98: SetNZ(A = Y); break;

	// stack
	case 0x48: Push(A); break;
	case 0x08: Push(P | BF | UF); break;
	case 0x68: SetNZ(A = Pop()); break;
	case 0x28: P = (Pop() & ~BF) | UF; break;

	// logic and arithmetic
	case 0x29: case 0x25: case 0x35: case 0x2D: case 0x3D: case 0x39: case 0x21: case 0x31:
		SetNZ(A &= Load()); break;
	case 0x49: case 0x45: case 0x55: case 0x4D: case 0x5D: case 0x59: case 0x41: case 0x51:
		SetNZ(A ^= Load()); break;
	case 0x09: case 0x05: case 0x15: case 0x0D: case 0x1D: case 0x19: case 0x01: case 0x11:
		SetNZ(A |= Load()); break;
	case 0x69: case 0x65: case 0x75: case 0x6D: case 0x7D: case 0x79: case 0x61: case 0x71:
		Adc(Load()); break;
	case 0xE9: case 0xEB: case 0xE5: case 0xF5: case 0xED: case 0xFD: case 0xF9: case 0xE1: case 0xF1:
		Adc(~Load()); break;
	case 0xC9: case 0xC5: case 0xD5: case 0xCD: case 0xDD: case 0xD9: case 0xC1: case 0xD1:
		Compare(A, Load()); break;
	case 0xE0: case 0xE4: case 0xEC:
		Compare(X, Load()); break;
	case 0xC0: case 0xC4: case 0xCC:
		Compare(Y, Load()); break;
	case 0x24: case 0x2C:
	{
		uint8_t v = Load();
		P = (P & ~(NF | VF | ZF)) | (v & (NF | VF)) | ((A & v) ? 0 : ZF);
		break;
	}

	// increments, decrements and shifts
	case 0xE6: case 0xF6: case 0xEE: case 0xFE: SetNZ(Change(+1)); break;
	case 0xC6: case 0xD6: case 0xCE: case 0xDE: SetNZ(Change(-1)); break;
	case 0xE8: SetNZ(++X); break;
	case 0xC8: SetNZ(++Y); break;
	case 0xCA: SetNZ(--X); break;
	case 0x88: SetNZ(--Y); break;
	case 0x0A: case 0x06: case 0x16: case 0x0E: case 0x1E: Modify(&RP2A03Cpu::Asl); break;
	case 0x4A: case 0x46: case 0x56: case 0x4E: case 0x5E: Modify(&RP2A03Cpu::Lsr); break;
	case 0x2A: case 0x26: case 0x36: case 0x2E: case 0x3E: Modify(&RP2A03Cpu::Rol); break;
	case 0x6A: case 0x66: case 0x76: case 0x6E: case 0x7E: Modify(&RP2A03Cpu::Ror); break;

	// jumps, calls, branches
	case 0x4C: case 0x6C: PC = addr; break;
	case 0x20:
		--PC;
		Push(uint8_t(PC >> 8));
		Push(uint8_t(PC));
		PC = addr;
		break;
	case 0x60:
	{
		uint8_t l = Pop();
		PC = uint16_t((l | Pop() << 8) + 1);
		break;
	}
	case 0x40:
	{
		P = (Pop() & ~BF) | UF;
		uint8_t l = Pop();
		PC = uint16_t(l | Pop() << 8);
		break;
	}
	case 0x00: ++PC; Interrupt(0xFFFE, true); break; // BRK skips a byte
	case 0x10: cycles += Branch(!(P & NF)); break;
	case 0x30: cycles += Branch( (P & NF)); break;
	case 0x50: cycles += Branch(!(P & VF)); break;
	case 0x70: cycles += Branch( (P & VF)); break;
	case 0x90: cycles += Branch(!(P & CF)); break;
	case 0xB0: cycles += Branch( (P & CF)); break;
	case 0xD0: cycles += Branch(!(P & ZF)); break;
	case 0xF0: cycles += Branch( (P & ZF)); break;

	// flags
	case 0x18: P &= ~CF; break;
	case 0x38: P |= CF; break;
	case 0x58: P &= ~IF; break;
	case 0x78: P |= IF; break;
	case 0xB8: P &= ~VF; break;
	case 0xD8: P &= ~DF; break;
	case 0xF8: P |= DF; break;

	// undocumented: combined operations
	case 0xA7: case 0xB7: case 0xAF: case 0xBF: case 0xA3: case 0xB3: case 0xAB:
		SetNZ(A = X = Load()); break; // LAX
	case 0x87: case 0x97: case 0x8F: case 0x83:
		Store(A & X); break; // SAX
	case 0xC7: case 0xD7: case 0xCF: case 0xDF: case 0xDB: case 0xC3: case 0xD3:
		Compare(A, Change(-1)); break; // DCP
	case 0xE7: case 0xF7: case 0xEF: case 0xFF: case 0xFB: case 0xE3: case 0xF3:
		Adc(~Change(+1)); break; // ISC
	case 0x07: case 0x17: case 0x0F: case 0x1F: case 0x1B: case 0x03: case 0x13:
		SetNZ(A |= Modify(&RP2A03Cpu::Asl)); break; // SLO
	case 0x27: case 0x37: case 0x2F: case 0x3F: case 0x3B: case 0x23: case 0x33:
		SetNZ(A &= Modify(&RP2A03Cpu::Rol)); break; // RLA
	case 0x47: case 0x57: case 0x4F: case 0x5F: case 0x5B: case 0x43: case 0x53:
		SetNZ(A ^= Modify(&RP2A03Cpu::Lsr)); break; // SRE
	case 0x67: case 0x77: case 0x6F: case 0x7F: case 0x7B: case 0x63: case 0x73:
		Adc(Modify(&RP2A03Cpu::Ror)); break; // RRA

	// undocumented: immediate
	case 0x0B: case 0x2B: SetNZ(A &= Load()); SetFlag(CF, A & 0x80); break; // ANC
	case 0x4B: A = Lsr(A & Load()); break; // ALR
	case 0x6B: // ARR
		A &= Load();
		A = uint8_t(A >> 1 | (P & CF) << 7);
		SetNZ(A);
		SetFlag(CF, A & 0x40);
		SetFlag(VF, ((A >> 6) ^ (A >> 5)) & 1);
		break;
	case 0xCB: // AXS
	{
		uint8_t v = Load(), ax = (A & X);
		SetFlag(CF, ax >= v);
		SetNZ(X = uint8_t(ax - v));
		break;
	}
	case 0x8B: SetNZ(A = X & Load()); break; // XAA

	// undocumented: unstable stores and loads
	case 0x93: case 0x9F: StoreHigh(A & X); break; // SHA
	case 0x9E: StoreHigh(X); break; // SHX
	case 0x9C: StoreHigh(Y); break; // SHY
	case 0x9B: S = (A & X); StoreHigh(S); break; // TAS
	case 0xBB: SetNZ(A = X = S = (Load() & S)); break; // LAS

	// jams
	case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52:
	case 0x62: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2:
		m_jammed = true;
		--PC;
		break;

	// NOPs: 0xEA and the undocumented ones, which read the operand
	default:
		break;
	}
	return cycles;
}
