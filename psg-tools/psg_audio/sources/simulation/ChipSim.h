#pragma once

#include "stream/Frame.h"

class ChipSim
{
public:
	enum class Type { AY8910, RP2A03, SN76489 };

	ChipSim(Type type) 
		: m_type(type)
		, m_srcClock(1)
		, m_dstClock(1)
	{}

	Type type() const 
	{
		return m_type; 
	}

	virtual void ConfigureClock(int srcClock, int dstClock)
	{
		m_srcClock = srcClock;
		m_dstClock = dstClock;
	}

public:
	virtual void Reset() = 0;
	virtual void Write(int chip, Register reg, uint8_t data) = 0;
	virtual void Simulate(int samples) = 0;
	virtual void Convert(Frame& frame) = 0;

protected:
	// converts period by clock ratio and divides it by the given divider,
	// integer math gives exact rounding, done once for both operations
	uint16_t ConvertPeriod(uint32_t period, uint32_t divider = 1) const
	{
		uint64_t num = uint64_t(period) * m_dstClock;
		uint64_t den = uint64_t(m_srcClock) * divider;
		uint64_t converted = (num + den / 2) / den;
		return uint16_t(converted > 0xFFFF ? 0xFFFF : converted);
	}

	void EnableTone(uint8_t& mixer, int chan) const
	{
		mixer &= ~(1 << chan);
	}

	void EnableNoise(uint8_t& mixer, int chan) const
	{
		mixer &= ~(1 << (3 + chan));
	}

protected:
	Type     m_type;
	uint32_t m_srcClock;
	uint32_t m_dstClock;
};
