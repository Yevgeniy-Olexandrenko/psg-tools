#include "SimFactory.h"
#include "SimAY8910.h"
#include "SimHybrid.h"
#include "SimLR35902.h"
#include "SimRP2A03.h"
#include "SimSN76489.h"

namespace
{
	using Source = SimFactory::Source;

	bool IsSelected(Chip::Model model)
	{
		return (model != Chip::Model::Compatible && model != Chip::Model::Unknown);
	}

	Chip::Model DefaultModel(const Source& source)
	{
		switch (source.type)
		{
		case ChipSim::Type::SN76489: return Chip::Model::AY8910;
		case ChipSim::Type::AY8910:  return (IsSelected(source.model) ? source.model : Chip::Model::AY8910);
		default: return Chip::Model::YM2149;
		}
	}

	std::shared_ptr<ChipSim> CreateSim(ChipSim::Type type)
	{
		switch (type)
		{
		case ChipSim::Type::RP2A03:  return std::make_shared<SimRP2A03>();
		case ChipSim::Type::LR35902: return std::make_shared<SimLR35902>();
		case ChipSim::Type::SN76489: return std::make_shared<SimSN76489>();
		case ChipSim::Type::AY8910:  return std::make_shared<SimAY8910>();
		default: return nullptr;
		}
	}

	// The output of a simulator on one PSG, or on two when it can use them.
	void ConfigureOutput(ChipSim& sim, Chip::Model model, bool twoPSGs, Chip::Model secondModel, Chip& schip)
	{
		const bool ay8930 = (model == Chip::Model::AY8930);
		if (sim.type() == ChipSim::Type::RP2A03)
		{
			auto outputType = (ay8930 ? SimRP2A03::OutputType::AY8930Chip : SimRP2A03::OutputType::SingleChip);
			if (twoPSGs && ay8930 && secondModel == Chip::Model::AY8930)
			{
				// the 2nd AY8930 plays the DMC
				outputType = SimRP2A03::OutputType::AY8930DoubleChip;
				schip.second.model = Chip::Model::AY8930;
			}
			else if (twoPSGs && !ay8930)
			{
				outputType = SimRP2A03::OutputType::DoubleChip;
				schip.second.model = model;
			}
			static_cast<SimRP2A03&>(sim).ConfigureOutput(outputType, model);
		}
		else if (sim.type() == ChipSim::Type::LR35902)
		{
			auto outputType = (ay8930 ? SimLR35902::OutputType::AY8930Chip : SimLR35902::OutputType::SingleChip);
			if (twoPSGs && !ay8930)
			{
				outputType = SimLR35902::OutputType::DoubleChip;
				schip.second.model = model;
			}
			static_cast<SimLR35902&>(sim).ConfigureOutput(outputType, model);
		}
	}
}

std::shared_ptr<ChipSim> SimFactory::Create(const std::vector<Source>& sources, Stream& stream)
{
	const Chip& dchip = stream.dchip;
	Chip& schip = stream.schip;
	if (sources.empty() || sources.size() > 2) return nullptr;

	// two chips need two PSGs
	if (sources.size() == 2 && IsSelected(dchip.first.model) && !dchip.second.modelKnown) return nullptr;

	const Source& first = sources[0];
	if (sources.size() == 1 && (first.type == ChipSim::Type::AY8910 || first.type == ChipSim::Type::SN76489))
	{
		auto sim = CreateSim(first.type);
		int clock = (first.type == ChipSim::Type::SN76489 ? first.clock / 2 : first.clock);
		schip.first.model = DefaultModel(first);
		if (first.dual) schip.second.model = DefaultModel(first);
		// the chip takes the closest clock it supports, the simulator converts to it
		schip.clockValue = clock;
		sim->ConfigureClock(first.clock, schip.clockValue);
		return sim;
	}

	schip.clock = (dchip.clockKnown ? dchip.clock : Chip::Clock::F1773400);
	schip.output = (dchip.outputKnown ? dchip.output : Chip::Output::Mono);

	if (sources.size() == 1)
	{
		auto sim = CreateSim(sources[0].type);
		if (!sim) return nullptr;

		Chip::Model model = (IsSelected(dchip.first.model) ? Chip::Model(dchip.first.model) : DefaultModel(sources[0]));
		schip.first.model = model;
		ConfigureOutput(*sim, model, dchip.count == 2, dchip.second.model, schip);
		sim->ConfigureClock(sources[0].clock, schip.clockValue);
		return sim;
	}

	std::shared_ptr<ChipSim> sims[2];
	for (int psg = 0; psg < 2; ++psg)
	{
		sims[psg] = CreateSim(sources[psg].type);
		if (!sims[psg]) return nullptr;

		Chip::Model model = dchip.model(psg);
		if (!IsSelected(model)) model = DefaultModel(sources[psg]);
		if (psg) schip.second.model = model;
		else schip.first.model = model;

		ConfigureOutput(*sims[psg], model, false, Chip::Model::Unknown, schip);
		sims[psg]->ConfigureClock(sources[psg].clock, schip.clockValue);
	}
	return std::make_shared<SimHybrid>(sims[0], sims[1]);
}
