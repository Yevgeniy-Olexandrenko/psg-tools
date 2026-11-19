#include <sstream>
#include <iterator>
#include "Chip.h"

namespace
{
	const int k_clocks[] =
	{
		0,       // unknown
		1000000, // Amstrad CPC, Oric
		1750000, // Pentagon and other ZX Spectrum clones
		1773400, // ZX Spectrum 128/+2/+3
		1789773, // MSX (3.579545 MHz / 2), also NES CPU clock
		2000000, // Atari ST
	};
}

Chip::Chip()
	: m_clock(Clock::Unknown)
	, m_output(Output::Unknown)
	, m_stereo(Stereo::Unknown)
	, count([this]() { return (second.modelKnown ? 2 : 1); })
	, clock(m_clock)
	, clockValue([this]() { return this->GetClockValue(); }, [this](const int& val) { this->SetClockValue(val); })
	, output(m_output)
	, stereo(m_stereo)
	, clockKnown([this]()  { return (m_clock  != Clock::Unknown);  })
	, outputKnown([this]() { return (m_output != Output::Unknown); })
	, stereoKnown([this]() { return (m_stereo != Stereo::Unknown); })
{
}

Chip::Chip(const Chip& other)
	: Chip()
{
	*this = other;
}

Chip& Chip::operator=(const Chip& other)
{
	if (this != &other)
	{
		first.model  = other.first.model;
		second.model = other.second.model;
		clock  = other.clock;
		output = other.output;
		stereo = other.stereo;
	}
	return *this;
}

Chip::F::F()
	: m_model(Model::Compatible)
	, model(m_model, [this](const Model& val) { m_model = (val == Model::Unknown ? Model::Compatible : val); })
{
}

Chip::S::S()
	: m_model(Model::Unknown)
	, model(m_model)
	, modelKnown([this]() { return (m_model != Model::Unknown); })
{
}

Chip::Model Chip::model(int index) const
{
	return (index ? second.model : first.model);
}

bool Chip::hasExpMode(int index) const
{
	return (model(index) == Model::AY8930);
}

std::string Chip::toString() const
{
	const auto OutputModel = [](std::ostream& stream, Model type)
	{
		switch (type)
		{
		case Model::AY8910:     stream << "AY-3-8910";        break;
		case Model::YM2149:     stream << "YM2149F";          break;
		case Model::AY8930:     stream << "AY8930";           break;
		case Model::Compatible: stream << "AY/YM Compatible"; break;
		}
	};

	std::stringstream stream;
	if (count == 2)
	{
		if (first.model == second.model)
		{
			stream << "2 x ";
			OutputModel(stream, first.model);
		}
		else
		{
			OutputModel(stream, first.model);
			stream << " + ";
			OutputModel(stream, second.model);
		}
		stream << ' ';
	}
	else
	{
		OutputModel(stream, first.model);
		stream << ' ';
	}

	if (clockKnown)
	{
		stream << double(clockValue) / 1000000 << " MHz" << ' ';
	}

	if (outputKnown)
	{
		if (output == Output::Mono)
		{
			stream << "Mono";
		}
		else if (stereoKnown)
		{
			switch (stereo)
			{
			case Stereo::ABC: stream << "ABC"; break;
			case Stereo::ACB: stream << "ACB"; break;
			case Stereo::BAC: stream << "BAC"; break;
			case Stereo::BCA: stream << "BCA"; break;
			case Stereo::CAB: stream << "CAB"; break;
			case Stereo::CBA: stream << "CBA"; break;
			}
		}
		else
		{
			stream << "Stereo";
		}
	}

	return stream.str();
}

void Chip::SetClockValue(const int& val)
{
	auto dist = [&](int i)
	{
		return std::abs(val - k_clocks[i]);
	};

	int f = 0;
	for (int i = 1; i < int(std::size(k_clocks)); ++i)
	{
		if (dist(i) < dist(f)) f = i;
	}
	m_clock = Clock(f);
}

int Chip::GetClockValue() const
{
	return k_clocks[size_t(m_clock)];
}

void ChipEncoder::Init(const Chip& chip)
{
	for (int index = 0; index < 2; ++index)
	{
		m_hasExpMode[index] = chip.hasExpMode(index);
		m_mode[index] = Mode::Unknown;
	}
}

void ChipEncoder::Encode(int chip, const Frame::Registers& regs, ChipPacket& packet)
{
	const auto modeBank = regs.GetData(Register::Mode_Bank);
	const auto isExpMode = (m_hasExpMode[chip] && regs.IsExpMode());
	const auto mode = (isExpMode ? Mode::Expanded : Mode::Compatible);
	bool bankB = false;
	packet.clear();

	// The mode switch clears the registers of the chip: R13 goes first.
	// The frame has all the registers changed after a switch.
	if (m_hasExpMode[chip] && mode != m_mode[chip])
	{
		packet.push_back({ Register::Mode_Bank, modeBank });
		m_mode[chip] = mode;
	}

	for (Register reg = Register::A_Fine; reg <= Register::EA_Coarse; ++reg)
	{
		if (regs.IsChanged(reg))
			packet.push_back({ uint8_t(reg & 0x0F), regs.GetData(reg) });
	}
	if (isExpMode)
	{
		for (Register reg = Register::EB_Fine; reg <= Register::N_OrMask; ++reg)
		{
			if (regs.IsChanged(reg))
			{
				if (!bankB)
				{
					packet.push_back({ Register::Mode_Bank, uint8_t(modeBank | 0x10) });
					bankB = true;
				}
				packet.push_back({ uint8_t(reg & 0x0F), regs.GetData(reg) });
			}
		}
	}

	// R13 goes last: envelope A restarts after its period is set, and bank A is back
	if (bankB || regs.IsChanged(Register::Mode_Bank))
	{
		packet.push_back({ Register::Mode_Bank, modeBank });
	}
}

void ChipDecoder::Init()
{
	m_bankB[0] = m_bankB[1] = false;
}

uint8_t ChipDecoder::Read(int chip, uint8_t addr, const Frame::Registers& regs) const
{
	if (addr == Register::Mode_Bank) return regs.GetData(Register::Mode_Bank);
	if (addr < Register::Mode_Bank) return regs.GetData(m_bankB[chip] ? Register::BankB_Fst + addr : addr);
	return 0xFF;
}

void ChipDecoder::Decode(int chip, const ChipRecord& record, Frame::Registers& regs)
{
	if (record.addr == Register::Mode_Bank)
	{
		m_bankB[chip] = ((record.data & 0xF0) == 0xB0);
		regs.Update(Register::Mode_Bank, record.data);
	}
	else if (record.addr < Register::Mode_Bank)
	{
		regs.Update(m_bankB[chip] ? Register::BankB_Fst + record.addr : record.addr, record.data);
	}
}
