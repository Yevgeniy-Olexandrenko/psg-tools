#pragma once

#include <vector>
#include "emulation/Machine.h"
#include "emulation/Z80Cpu.h"

// The machine of the AY files (ZXAYEMUL): a Z80 and one AY chip, on the ports
// of the ZX Spectrum 128 or of the Amstrad CPC. The first write to the AY
// selects the system. The format has no timing of its own: a Z80 at 3.5 MHz,
// 50 frames a second (70000 T-states a frame), the interrupt waits 32
// T-states at the frame start, as on the ZX Spectrum. The CPC songs of the
// format are called at 50 Hz too, so the timing is the same for them.
class AYMachine : public Machine
{
public:
	enum class System { Unknown, ZX, CPC };

	// The code of a song: the points and the blocks of memory.
	struct Block
	{
		uint16_t addr;
		const uint8_t* data;
		uint32_t size;
	};
	struct Song
	{
		uint16_t stack, init, interrupt;
		uint8_t  hiReg, loReg;
		std::vector<Block> blocks;
	};

	AYMachine();

	// Loads the song and starts the frames from it.
	void Load(const Song& song);
	System GetSystem() const { return m_system; }

	uint8_t ReadPort(uint16_t port) override;
	void WritePort(uint16_t port, uint8_t data) override;

protected:
	void Execute(uint64_t cycle) override;
	uint64_t GetCycle() const override { return m_cycle; }

private:
	void WriteAY(uint8_t data);

private:
	Z80Cpu   m_cpu;
	uint8_t  m_memory[0x10000];
	uint64_t m_cycle = 0; // T-states from the start

	System  m_system = System::Unknown;
	uint8_t m_register = 0;    // selected AY register
	uint8_t m_cpcData = 0;     // CPC: PPI port A
	uint8_t m_cpcFunction = 0; // CPC: the AY function from PPI port C, 0 when none
};
