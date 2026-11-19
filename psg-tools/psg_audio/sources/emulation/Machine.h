#pragma once

#include <cstdint>
#include <memory>
#include "stream/Frame.h"
#include "simulation/ChipSim.h"
#include "decoders/FrameCutter.h"

// The bus of a CPU with 16 address lines. A table of 256 pages of 256 bytes
// points to the memory of each page, for reads and for writes. A page without
// memory goes to OnRead/OnWrite: the registers of the devices, the bank
// switching. The Z80 also has a separate I/O space for IN and OUT.
class Bus
{
public:
	Bus();
	virtual ~Bus() = default;

	uint8_t Read(uint16_t addr)
	{
		const uint8_t* page = m_readPages[addr >> 8];
		return (page ? page[addr & 0xFF] : OnRead(addr));
	}

	void Write(uint16_t addr, uint8_t data)
	{
		uint8_t* page = m_writePages[addr >> 8];
		if (page) page[addr & 0xFF] = data; else OnWrite(addr, data);
	}

	virtual uint8_t ReadPort(uint16_t port) { return 0xFF; }
	virtual void WritePort(uint16_t port, uint8_t data) {}

protected:
	// Maps the pages of [addr, addr + size) to the memory, size is a multiple of 256.
	// A null read or write sends the access of the pages to OnRead or OnWrite.
	void Map(uint16_t addr, uint32_t size, const uint8_t* read, uint8_t* write);

	virtual uint8_t OnRead(uint16_t addr) { return 0xFF; }
	virtual void OnWrite(uint16_t addr, uint8_t data) {}

private:
	const uint8_t* m_readPages[256];
	uint8_t* m_writePages[256];
};

// A machine runs the code of a music file on its CPU. The CPU writes to the
// sound chips, and a simulator (the same as for VGM) turns the writes into the
// registers of the destination chips. The machine runs without a break and
// gives the frames in one pass. While the CPU takes the interrupts, the player
// runs on them, and a frame ends at each interrupt. While it does not take
// them (a player with its own timing), a FrameCutter cuts the frames at the
// updates of the player, or every frame of the hardware without them.
class Machine : public Bus
{
public:
	// Runs the code up to the end of the next frame and gives the registers of the chips.
	void RunFrame(Frame& frame);

	// The rate of the frames: the rate of the updates of the player, or the mean
	// rate of the frames when the updates have no clear rate. False when most
	// frames end at the interrupts: then the rate is that of the hardware.
	bool GetFrameRate(FrameRate& frameRate) const;

	// The time of the frames given, in samples of 44.1 kHz.
	int GetTime() const { return int(m_sample - m_start); }

protected:
	// cpuClock: Hz, frameRate: the interrupts of the hardware a second.
	void ConfigureTiming(std::shared_ptr<ChipSim> sim, uint32_t cpuClock, int frameRate);

	// Starts the frames from the current state of the machine (after a reset).
	void StartFrames();

	// Runs the CPU up to the cycle (from the start) or up to the next event.
	virtual void Execute(uint64_t cycle) = 0;
	virtual uint64_t GetCycle() const = 0;

	// The CPU writes to a chip or takes an interrupt. The event waits, so the
	// frame cutter knows the time to it, and Execute stops at it.
	void WriteChip(int chip, Register reg, uint8_t data);
	void OnInterrupt();
	bool HasPendingEvent() const { return (m_pending || m_interrupt); }

	// The state of the chip after the writes up to now.
	uint8_t ReadChip(int chip, Register reg) { return m_sim->Read(chip, reg); }

protected:
	std::shared_ptr<ChipSim> m_sim;

private:
	int64_t ToSample(uint64_t cycle) const { return int64_t(cycle * 44100 / m_cpuClock); }
	void EndFrame(Frame& frame);

	uint32_t m_cpuClock = 1;
	int m_frameTime = 0; // samples of a frame of the hardware
	std::unique_ptr<FrameCutter> m_cutter;
	// the times in samples of 44.1 kHz
	int64_t m_start = 0;
	int64_t m_sample = 0;     // of the simulator
	int64_t m_frameStart = 0; // the end of the last frame
	int64_t m_lastInterrupt = 0;
	bool m_onInterrupts = false;
	int m_frames = 0;
	int m_interruptFrames = 0;

	bool     m_interrupt = false;
	uint64_t m_interruptCycle = 0;

	bool     m_pending = false;
	uint64_t m_pendingCycle = 0;
	int      m_pendingChip = 0;
	uint8_t  m_pendingReg = 0;
	uint8_t  m_pendingData = 0;
};
