#include "SimHybrid.h"

SimHybrid::SimHybrid(std::shared_ptr<ChipSim> sim0, std::shared_ptr<ChipSim> sim1)
	: ChipSim(Type::Hybrid)
	, m_sims{ sim0, sim1 }
{
}

ChipSim* SimHybrid::Find(Type type)
{
	for (auto& sim : m_sims)
	{
		if (ChipSim* found = sim->Find(type)) return found;
	}
	return ChipSim::Find(type);
}

void SimHybrid::Reset()
{
	for (int i = 0; i < 2; ++i)
	{
		m_sims[i]->Reset();
		m_frames[i].ResetData();
		m_frames[i].ResetChanges();
	}
}

void SimHybrid::Write(int chip, Register reg, uint8_t data)
{
	// The decoder writes to the simulator that Find returns.
}

void SimHybrid::Simulate(int samples)
{
	for (auto& sim : m_sims) sim->Simulate(samples);
}

void SimHybrid::Convert(Frame& frame)
{
	// Find the changes of each simulator chip against its PSG.
	for (int i = 0; i < 2; ++i)
	{
		m_sims[i]->Convert(m_frames[i]);
		frame[i] += m_frames[i][0];
		m_frames[i].ResetChanges();
	}
}
