#pragma once

#include <array>
#include <vector>
#include "processing/ProcessingChain.h"

class Stream;
class Frame;

class Output
{
public:
	Output();
	virtual ~Output();
	
	bool Open();
	bool Init(const Stream& stream);
	bool Write(const Frame& frame);
	void Close();

	using Enables = std::array<bool, 5>;
	const Enables& GetEnables() const;
	Enables& GetEnables();

	void GetLevels(float& L, float& C, float& R) const;
	void GetLevels(float& L, float& R) const;
	std::string toString() const;

protected:
	using Data = std::vector<std::pair<uint8_t, uint8_t>>;
	virtual const std::string GetDeviceName() const = 0;

	virtual bool DeviceOpen() = 0;
	virtual bool DeviceInit(const Stream& stream, Chip& dchip) = 0;
	virtual bool DeviceWrite(int chip, const Data& data) = 0;
	virtual void DeviceClose() = 0;

private:
	float ComputeChannelLevel(int chip, int chan) const;

private:
	Chip m_dchip;
	bool m_alive;
	ProcessingChain m_processing;
};
