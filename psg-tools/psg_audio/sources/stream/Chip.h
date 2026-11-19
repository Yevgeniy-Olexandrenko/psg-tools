#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <array>
#include "utils/Property.h"
#include "Frame.h"

struct Chip
{
public:
	enum class Model  { Unknown, AY8910, YM2149, AY8930, Compatible };
	enum class Clock  { Unknown, F1000000, F1750000, F1773400, F1789773, F2000000 };
	enum class Output { Unknown, Mono, Stereo };
	enum class Stereo { Unknown, ABC, ACB, BAC, BCA, CAB, CBA };

public:
	Chip();
	Chip(const Chip& other);
	Chip& operator=(const Chip& other);

	struct F
	{
		F();

		Property<Model> model;

	private:
		Model m_model;
	} first;

	struct S
	{
		S();

		Property<Model> model;
		ReadOnlyProperty<bool> modelKnown;

	private:
		Model m_model;
	} second;

	ReadOnlyProperty<int>  count;
	Property<Clock>        clock;
	Property<int>          clockValue;
	Property<Output>       output;
	Property<Stereo>       stereo;
	ReadOnlyProperty<bool> clockKnown;
	ReadOnlyProperty<bool> outputKnown;
	ReadOnlyProperty<bool> stereoKnown;

	Model model(int index) const;
	bool hasExpMode(int index) const;
	std::string toString() const;

private:
	void SetClockValue(const int& val);
	int  GetClockValue() const;

private:
	Clock  m_clock;
	Output m_output;
	Stereo m_stereo;
};

// A frame keeps bank B of the AY8930 flat (0x10-0x1A). The chip has 16
// addresses and selects the bank through R13. Each write to R13 also
// restarts envelope A.

struct ChipRecord  { uint8_t addr, data; };
using  ChipPacket  = std::vector<ChipRecord>;
using  ChipPackets = std::array<ChipPacket, 2>;

// Frame -> the writes that give a chip the changed registers (a new packet).
class ChipEncoder
{
public:
	void Init(const Chip& chip);
	void Encode(int chip, const Frame::Registers& regs, ChipPacket& packet);

private:
	enum class Mode { Unknown, Compatible, Expanded };

	bool m_hasExpMode[2]{ false, false };
	Mode m_mode[2]{ Mode::Unknown, Mode::Unknown };
};

// The writes to a chip (addresses 0x00-0x0F) -> frame.
class ChipDecoder
{
public:
	void Init();
	void Decode(int chip, const ChipRecord& record, Frame::Registers& regs);

	// Reads a chip address (0x00-0x0F) back from the registers, as the chip
	// gives it: through the selected bank, with the unused bits zero. The
	// I/O ports (0x0E, 0x0F) and the addresses above give 0xFF.
	uint8_t Read(int chip, uint8_t addr, const Frame::Registers& regs) const;

private:
	bool m_bankB[2]{ false, false };
};
