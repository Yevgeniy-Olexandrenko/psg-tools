#include <algorithm>
#include "Machine.h"

Bus::Bus()
{
	Map(0x0000, 0x10000, nullptr, nullptr);
}

void Bus::Map(uint16_t addr, uint32_t size, const uint8_t* read, uint8_t* write)
{
	for (uint32_t page = 0, first = (addr >> 8); page < (size >> 8) && first + page < 256; ++page)
	{
		m_readPages[first + page] = (read ? read + (page << 8) : nullptr);
		m_writePages[first + page] = (write ? write + (page << 8) : nullptr);
	}
}

void Machine::ConfigureTiming(std::shared_ptr<ChipSim> sim, uint32_t cpuClock, int frameRate)
{
	m_sim = sim;
	m_cpuClock = cpuClock;
	m_frameTime = (44100 / frameRate);
}

void Machine::StartFrames()
{
	m_cutter.reset(new FrameCutter(m_frameTime));
	m_start = m_sample = m_frameStart = m_lastInterrupt = ToSample(GetCycle());
	// the player is on the interrupts until it takes none for longer than a frame
	m_onInterrupts = true;
	m_pending = m_interrupt = false;
	m_frames = m_interruptFrames = 0;
}

bool Machine::GetFrameRate(FrameRate& frameRate) const
{
	if (2 * m_interruptFrames >= m_frames) return false;
	if (m_cutter->GetFrameRate(frameRate)) return true;

	// The updates have no clear rate, but they can come more often than the
	// frames of the hardware: the mean rate of the frames keeps the tempo.
	if (m_sample <= m_start) return false;
	frameRate = (44100.0 * m_frames / (m_sample - m_start));
	return true;
}

void Machine::WriteChip(int chip, Register reg, uint8_t data)
{
	m_pending = true;
	m_pendingCycle = GetCycle();
	m_pendingChip = chip;
	m_pendingReg = reg;
	m_pendingData = data;
}

void Machine::OnInterrupt()
{
	m_interrupt = true;
	m_interruptCycle = GetCycle();
}

void Machine::RunFrame(Frame& frame)
{
	while (true)
	{
		// the CPU runs ahead of the simulator up to its next event (or one frame
		// of the hardware), so the cutter knows how long it is to a write
		if (!HasPendingEvent() && ToSample(GetCycle()) <= m_sample)
		{
			Execute(GetCycle() + uint64_t(m_frameTime) * m_cpuClock / 44100);
		}
		uint64_t cycle = (m_pending ? m_pendingCycle : m_interrupt ? m_interruptCycle : GetCycle());
		int64_t next = ToSample(cycle);
		int wait = int(next > m_sample ? next - m_sample : 0);

		if (m_interrupt && !wait)
		{
			// the first interrupt after the start ends no frame
			m_interrupt = false;
			m_lastInterrupt = m_sample;
			m_onInterrupts = true;
			if (m_sample > m_frameStart)
			{
				m_cutter->EndFrame();
				m_interruptFrames++;
				EndFrame(frame);
				return;
			}
			continue;
		}

		// without the interrupts for longer than a frame, the cutter ends the frames
		const int noInterrupt = (m_frameTime + m_frameTime / 2);
		if (m_onInterrupts && m_sample - m_lastInterrupt > noInterrupt)
		{
			m_onInterrupts = false;
			m_cutter->EndFrame(int(m_sample - m_frameStart));
		}
		if (!m_onInterrupts && m_cutter->IsFrameEnd(wait))
		{
			EndFrame(frame);
			return;
		}
		if (m_pending && !wait)
		{
			m_sim->Write(m_pendingChip, m_pendingReg, m_pendingData);
			m_cutter->OnWrite();
			m_pending = false;
			continue;
		}
		int samples = (m_onInterrupts
			? int(std::min<int64_t>(wait, m_lastInterrupt + noInterrupt + 1 - m_sample))
			: m_cutter->Advance(wait));
		if (samples > 0)
		{
			m_sim->Simulate(samples);
			m_sample += samples;
		}
	}
}

void Machine::EndFrame(Frame& frame)
{
	m_sim->Convert(frame);
	m_frameStart = m_sample;
	m_frames++;
}
