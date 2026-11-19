#pragma once

#include <vector>
#include "emulation/Machine.h"
#include "emulation/RP2A03Cpu.h"

// The machine of the NSF files: the RP2A03 CPU of the NES, 2K of RAM, 8K of
// RAM at 0x6000, and the program in 0x8000-0xFFFF. A file with bank switching
// has its program in banks of 4K: the registers 0x5FF8-0x5FFF select the bank
// of each 4K of 0x8000-0xFFFF (a bank past the end of the file reads zeros).
// The APU registers 0x4000-0x4017 go to chip 0 of the simulator.
//
// The expansion chips: the Sunsoft 5B (an AY with the clock divided by 2:
// 0xC000 selects a register, 0xE000 writes it) can go to chip 1 of the
// simulator. The sound of the other chips is not played, but the machine
// keeps what the CPU sees of them: the RAM of the FDS (0x6000-0xDFFF can be
// written, 0x5FF6/0x5FF7 select the banks of 0x6000/0x7000), the RAM and the
// multiplier of the MMC5, the RAM of the N163.
//
// The machine calls the init routine of the song, then the play routine at
// the rate of the header. A routine returns to an address without code, where
// the CPU waits. When play has not returned at a tick, that tick is skipped:
// the next call comes at the next tick. The return from play ends an update of
// the player (see Machine::EndUpdate). NSF2 features (play from an interrupt,
// IRQ) are not supported: Y = 0 at init tells this to the program.
class NSFMachine : public Machine
{
public:
	// Spec: https://www.nesdev.org/wiki/NSF
	#pragma pack(push, 1)
	struct Header
	{
		uint8_t  id[5];          // 00 NESM\x1A
		uint8_t  version;        // 05
		uint8_t  songs;          // 06 number of songs
		uint8_t  startSong;      // 07 first song, from 1
		uint16_t loadAddr;       // 08
		uint16_t initAddr;       // 0A
		uint16_t playAddr;       // 0C
		char     songName[32];   // 0E
		char     artistName[32]; // 2E
		char     copyright[32];  // 4E
		uint16_t ntscSpeed;      // 6E play period in microseconds (NTSC)
		uint8_t  banks[8];       // 70 initial banks, all 0 when no bank switching
		uint16_t palSpeed;       // 78 play period in microseconds (PAL)
		uint8_t  palNtscBits;    // 7A bit 0: PAL (for both: the preferred one), bit 1: both
		uint8_t  expansion;      // 7B the expansion chips
		uint8_t  nsf2Flags;      // 7C NSF2
		uint8_t  programSize[3]; // 7D the size of the program, 0 for the rest of the file
	};
	#pragma pack(pop)
	static_assert(sizeof(Header) == 0x80, "NSF header size");

	// the bits of Header::expansion
	enum Expansion : uint8_t { VRC6 = 0x01, VRC7 = 0x02, FDS = 0x04, MMC5 = 0x08, N163 = 0x10, S5B = 0x20 };

	// the clock of the 5B: the CPU clock divided by 2
	static const int k_5BClockDivider = 2;

	// The system of the file and the clock of its CPU.
	static bool IsPAL(const Header& header) { return (header.palNtscBits & 0x01); }
	static uint32_t GetCpuClock(const Header& header);

	// The simulator takes the APU writes as chip 0 and, with play5B, the writes
	// of the 5B as chip 1. Its clocks are configured by the caller.
	NSFMachine(std::shared_ptr<ChipSim> sim, const Header& header, bool play5B);

	// Loads the program of the file (after the header) and calls the init
	// routine of the song (from 0). False when the program does not fit.
	bool Load(const uint8_t* data, size_t size, int song);

	// The calls of the play routine a second.
	FrameRate GetPlayRate() const;

protected:
	void Execute(uint64_t cycle) override;
	uint64_t GetCycle() const override { return m_cycle; }

	uint8_t OnRead(uint16_t addr) override;
	void OnWrite(uint16_t addr, uint8_t data) override;

private:
	void Call(uint16_t addr);
	void SkipMissedTicks();
	void SelectBank(uint16_t addr, uint8_t bank);

private:
	const Header m_header;
	const bool   m_play5B;
	RP2A03Cpu    m_cpu;
	uint32_t     m_cpuClock;
	uint64_t     m_cycle = 0;

	// the times in CPU cycles x 1000000, the play period is exact in them
	uint64_t m_playPeriod;
	uint64_t m_nextPlay = 0;
	bool     m_playing = false; // the play routine has not returned yet

	uint8_t m_ram[0x800];
	uint8_t m_wram[0x2000];
	std::vector<uint8_t> m_rom;  // banks of 4K
	uint8_t m_zeros[0x1000]{};   // a bank past the end of the file
	bool    m_bankSwitching = false;

	uint8_t m_5BRegister = 0;
	uint8_t m_mmc5Ram[0x400];
	uint8_t m_mmc5Factors[2];
	uint8_t m_n163Ram[0x80];
	uint8_t m_n163Address = 0;  // bit 7: increment after each access
	uint8_t m_fdsWave[0x40];
};
