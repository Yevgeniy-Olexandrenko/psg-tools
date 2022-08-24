#pragma once

#include "stream/Frame.h"

class ChipSim
{
public:
	enum class Type { AY8910, RP2A03, SN76489 };

	ChipSim(Type type) : m_type(type) {}
	Type type() const { return m_type; }

public:
	virtual void Reset() = 0;
	virtual void Write(int chip, Register reg, uint8_t data) = 0;
	virtual void Simulate(int samples) = 0;
	virtual void Convert(Frame& frame) = 0;

private:
	Type m_type;
};
