#include <algorithm>
#include "Machine.h"

namespace
{
	const uint64_t k_keyBasis = 0xCBF29CE484222325; // FNV-1a
	const uint64_t k_keyPrime = 0x100000001B3;
}

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
	m_start = m_sample = m_frameStart = m_lastUpdateEnd = ToSample(GetCycle());
	// the frames end at EndUpdate until there is none for longer than 1.5 frames
	m_onUpdates = true;
	m_pending = m_updateEnd = false;
	m_frames = m_updateFrames = 0;
	m_writesKey = m_frameKey = k_keyBasis;
}

bool Machine::GetFrameRate(FrameRate& frameRate) const
{
	if (2 * m_updateFrames >= m_frames) return false;
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

void Machine::EndUpdate()
{
	m_updateEnd = true;
	m_updateEndCycle = GetCycle();
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
		uint64_t cycle = (m_pending ? m_pendingCycle : m_updateEnd ? m_updateEndCycle : GetCycle());
		int64_t next = ToSample(cycle);
		int wait = int(next > m_sample ? next - m_sample : 0);

		if (m_updateEnd && !wait)
		{
			// the first end of an update right at the start ends no frame
			m_updateEnd = false;
			m_lastUpdateEnd = m_sample;
			m_onUpdates = true;
			if (m_sample > m_frameStart)
			{
				m_cutter->EndFrame();
				m_updateFrames++;
				EndFrame(frame);
				return;
			}
			continue;
		}

		// without the ends of the updates for 1.5 frames, the cutter ends the frames
		const int noUpdate = (m_frameTime + m_frameTime / 2);
		if (m_onUpdates && m_sample - m_lastUpdateEnd > noUpdate)
		{
			m_onUpdates = false;
			m_cutter->EndFrame(int(m_sample - m_frameStart));
		}
		if (!m_onUpdates && m_cutter->IsFrameEnd(wait))
		{
			EndFrame(frame);
			return;
		}
		if (m_pending && !wait)
		{
			m_sim->Write(m_pendingChip, m_pendingReg, m_pendingData);
			for (uint8_t byte : { uint8_t(m_pendingChip), m_pendingReg, m_pendingData })
			{
				m_frameKey = (m_frameKey ^ byte) * k_keyPrime;
			}
			m_cutter->OnWrite();
			m_pending = false;
			continue;
		}
		int samples = (m_onUpdates
			? int(std::min<int64_t>(wait, m_lastUpdateEnd + noUpdate + 1 - m_sample))
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
	m_writesKey = m_frameKey;
	m_frameKey = k_keyBasis;
}
