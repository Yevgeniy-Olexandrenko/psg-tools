#include <algorithm>
#include <cmath>
#include <cstring>
#include "NSFMachine.h"

namespace
{
	const uint32_t k_bankSize = 0x1000;

	// Without a play period in the header, play is called at each frame of
	// the hardware: 29780.5 (NTSC) or 33247.5 (PAL) CPU cycles, here x 1000000.
	const uint64_t k_ntscFrame = 29780500000ULL;
	const uint64_t k_palFrame = 33247500000ULL;

	// The routines return here. There is no code at this address: the CPU
	// waits at it for the next call.
	const uint16_t k_idleAddr = 0x4100;

	const uint8_t k_jsrCycles = 6;
}

uint32_t NSFMachine::GetCpuClock(const Header& header)
{
	return uint32_t(IsPAL(header) ? RP2A03Cpu::Clock::PAL : RP2A03Cpu::Clock::NTSC);
}

NSFMachine::NSFMachine(std::shared_ptr<ChipSim> sim, const Header& header, bool play5B)
	: m_header(header)
	, m_play5B(play5B && (header.expansion & S5B))
	, m_cpu(*this)
	, m_cpuClock(GetCpuClock(header))
{
	const bool pal = IsPAL(header);
	const uint16_t speed = (pal ? header.palSpeed : header.ntscSpeed);
	m_playPeriod = (speed ? uint64_t(speed) * m_cpuClock : pal ? k_palFrame : k_ntscFrame);
	ConfigureTiming(sim, m_cpuClock, std::max(1, int(std::lround(GetPlayRate()))));
}

FrameRate NSFMachine::GetPlayRate() const
{
	return (1000000.0 * m_cpuClock / m_playPeriod);
}

bool NSFMachine::Load(const uint8_t* data, size_t size, int song)
{
	const bool fds = (m_header.expansion & FDS);

	// the appended NSF2 metadata is not a part of the program
	size_t programSize = (m_header.programSize[0] | m_header.programSize[1] << 8 | m_header.programSize[2] << 16);
	if (programSize && programSize < size) size = programSize;
	if (!size) return false;

	// RAM mirrored in 0x0000-0x1FFF; the rest goes to OnRead/OnWrite
	memset(m_ram, 0, sizeof(m_ram));
	memset(m_wram, 0, sizeof(m_wram));
	for (uint16_t addr = 0x0000; addr < 0x2000; addr += sizeof(m_ram)) Map(addr, sizeof(m_ram), m_ram, m_ram);
	if (!fds) Map(0x6000, sizeof(m_wram), m_wram, m_wram);

	// Bank switching: the program starts at the offset (loadAddr & 0xFFF) of
	// its first bank. Else it is loaded at loadAddr into fixed banks from
	// 0x8000, or from 0x6000 for the FDS (its program can be loaded in RAM).
	m_bankSwitching = std::any_of(m_header.banks, m_header.banks + 8, [](uint8_t bank) { return bank != 0; });
	m_rom.clear();
	if (m_bankSwitching)
	{
		// a bank number has 8 bits: the data after bank 255 cannot be selected
		size_t offset = (m_header.loadAddr & (k_bankSize - 1));
		size_t count = std::min<size_t>((offset + size + k_bankSize - 1) / k_bankSize, 256);
		m_rom.assign(count * k_bankSize, 0);
		memcpy(m_rom.data() + offset, data, std::min(size, m_rom.size() - offset));

		for (int slot = 0; slot < 8; ++slot) SelectBank(uint16_t(0x8000 + slot * k_bankSize), m_header.banks[slot]);
		if (fds)
		{
			// the FDS banks of 0x6000-0x7FFF start as those of 0xE000-0xFFFF
			SelectBank(0x6000, m_header.banks[6]);
			SelectBank(0x7000, m_header.banks[7]);
		}
	}
	else
	{
		const uint32_t base = (fds ? 0x6000 : 0x8000);
		if (m_header.loadAddr < base) return false;
		m_rom.assign(0x10000 - base, 0);
		size_t offset = (m_header.loadAddr - base);
		memcpy(m_rom.data() + offset, data, std::min(size, m_rom.size() - offset));

		for (uint32_t addr = base; addr < 0x10000; addr += k_bankSize)
		{
			SelectBank(uint16_t(addr), uint8_t((addr - base) / k_bankSize));
		}
	}

	// what the CPU sees of the expansion chips
	memset(m_mmc5Ram, 0, sizeof(m_mmc5Ram));
	memset(m_mmc5Factors, 0, sizeof(m_mmc5Factors));
	memset(m_n163Ram, 0, sizeof(m_n163Ram));
	memset(m_fdsWave, 0, sizeof(m_fdsWave));
	m_n163Address = 0;
	m_5BRegister = 0;
	if (m_header.expansion & MMC5) Map(0x5C00, 0x300, m_mmc5Ram, m_mmc5Ram);

	// the APU after the reset, as the format describes it
	m_sim->Reset();
	for (uint8_t reg = 0x00; reg <= 0x13; ++reg) m_sim->Write(0, reg, 0x00);
	m_sim->Write(0, 0x15, 0x00);
	m_sim->Write(0, 0x15, 0x0F);
	m_sim->Write(0, 0x17, 0x40);

	// init: A is the song, X is 1 for PAL, Y is 0 (no NSF2 features)
	m_cpu.Reset();
	m_cpu.A = uint8_t(song);
	m_cpu.X = (IsPAL(m_header) ? 1 : 0);
	m_cpu.Y = 0;
	m_cycle = 0;
	m_playing = false;
	Call(m_header.initAddr);
	m_nextPlay = m_playPeriod;
	StartFrames();
	return true;
}

void NSFMachine::Call(uint16_t addr)
{
	uint16_t ret = uint16_t(k_idleAddr - 1);
	m_cpu.Push(uint8_t(ret >> 8));
	m_cpu.Push(uint8_t(ret));
	m_cpu.PC = addr;
	m_cycle += k_jsrCycles;
}

// The ticks that came while a routine ran are skipped (the spec allows it).
void NSFMachine::SkipMissedTicks()
{
	while (m_nextPlay <= m_cycle * 1000000) m_nextPlay += m_playPeriod;
}

void NSFMachine::Execute(uint64_t cycle)
{
	while (m_cycle < cycle && !HasPendingEvent())
	{
		if (m_cpu.PC == k_idleAddr)
		{
			if (m_cycle * 1000000 < m_nextPlay)
			{
				uint64_t next = (m_nextPlay + 999999) / 1000000;
				m_cycle = std::min(next, cycle);
				continue;
			}
			m_nextPlay += m_playPeriod;
			Call(m_header.playAddr);
			m_playing = true;
		}
		else if (m_cpu.IsJammed())
		{
			m_cycle = cycle;
		}
		else
		{
			m_cycle += m_cpu.Step();
			if (m_cpu.PC == k_idleAddr)
			{
				// a routine has returned; the frame ends with all the writes of play
				SkipMissedTicks();
				if (m_playing) EndUpdate();
				m_playing = false;
			}
		}
	}
}

void NSFMachine::SelectBank(uint16_t addr, uint8_t bank)
{
	uint8_t* data = (size_t(bank + 1) * k_bankSize <= m_rom.size() ? &m_rom[bank * k_bankSize] : nullptr);

	// the FDS can write its program in 0x6000-0xDFFF
	bool writable = (data && (m_header.expansion & FDS) && addr < 0xE000);
	Map(addr, k_bankSize, (data ? data : m_zeros), (writable ? data : nullptr));

	// the DMC of the APU plays its samples from 0xC000-0xFFFF
	ChipSim* apu = m_sim->Find(ChipSim::Type::RP2A03);
	if (apu && addr >= 0x8000) apu->WriteMemory(addr, (data ? data : m_zeros), k_bankSize);
}

uint8_t NSFMachine::OnRead(uint16_t addr)
{
	const uint8_t expansion = m_header.expansion;
	if (addr == 0x4015)
	{
		// the APU status: the length counters and the DMC
		return ReadChip(0, Register(0x15));
	}
	if ((expansion & FDS) && addr >= 0x4040 && addr <= 0x407F)
	{
		return m_fdsWave[addr - 0x4040];
	}
	if ((expansion & N163) && addr == 0x4800)
	{
		uint8_t data = m_n163Ram[m_n163Address & 0x7F];
		if (m_n163Address & 0x80) m_n163Address = (0x80 | ((m_n163Address + 1) & 0x7F));
		return data;
	}
	if (expansion & MMC5)
	{
		uint16_t product = uint16_t(m_mmc5Factors[0] * m_mmc5Factors[1]);
		if (addr == 0x5205) return uint8_t(product);
		if (addr == 0x5206) return uint8_t(product >> 8);
		if (addr >= 0x5F00 && addr <= 0x5FF5) return m_mmc5Ram[addr - 0x5C00];
	}
	// the open bus
	return uint8_t(addr >> 8);
}

void NSFMachine::OnWrite(uint16_t addr, uint8_t data)
{
	const uint8_t expansion = m_header.expansion;
	if (addr >= 0x4000 && addr <= 0x4017)
	{
		// 0x4014 (sprite DMA) and 0x4016 (controllers) are not the APU
		if (addr != 0x4014 && addr != 0x4016) WriteChip(0, Register(addr & 0x1F), data);
	}
	else if ((expansion & FDS) && addr >= 0x4040 && addr <= 0x407F)
	{
		m_fdsWave[addr - 0x4040] = (data & 0x3F);
	}
	else if ((expansion & N163) && addr == 0x4800)
	{
		m_n163Ram[m_n163Address & 0x7F] = data;
		if (m_n163Address & 0x80) m_n163Address = (0x80 | ((m_n163Address + 1) & 0x7F));
	}
	else if ((expansion & MMC5) && (addr == 0x5205 || addr == 0x5206))
	{
		m_mmc5Factors[addr - 0x5205] = data;
	}
	else if ((expansion & MMC5) && addr >= 0x5F00 && addr <= 0x5FF5)
	{
		m_mmc5Ram[addr - 0x5C00] = data;
	}
	else if ((expansion & FDS) && (addr == 0x5FF6 || addr == 0x5FF7))
	{
		if (m_bankSwitching) SelectBank(uint16_t(0x6000 + (addr - 0x5FF6) * k_bankSize), data);
	}
	else if (addr >= 0x5FF8 && addr <= 0x5FFF)
	{
		if (m_bankSwitching) SelectBank(uint16_t(0x8000 + (addr - 0x5FF8) * k_bankSize), data);
	}
	else if ((expansion & N163) && addr >= 0xF800)
	{
		m_n163Address = data;
	}
	else if (m_play5B && (addr & 0xE000) == 0xC000)
	{
		m_5BRegister = (data & 0x0F);
	}
	else if (m_play5B && (addr & 0xE000) == 0xE000 && m_5BRegister < 14)
	{
		// 4 bits of the envelope shape, as on the AY-3-8910
		if (m_5BRegister == Register::E_Shape) data &= 0x0F;
		WriteChip(1, Register(m_5BRegister), data);
	}
	// the other writes (the sound of VRC6, VRC7, FDS, MMC5, N163) are not played
}
