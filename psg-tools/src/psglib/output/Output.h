#pragma once

#include <array>
#include <vector>
#include "processing/Processing.h"

#define DBG_PROCESSING 0

//#define DEBUG_TEST
#if defined(DEBUG_TEST)
#include "../../debug/psg-access.h"
#endif

class Stream;
class Frame;

class Output : private Processing
{
	using ProcChain = std::vector<std::unique_ptr<Processing>>;

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
	void  Reset() override;
	const Frame& operator()(const Frame& frame) override;
	float ComputeChannelLevel(int chip, int chan) const;

private:
	Chip m_dchip;
	bool m_isOpened;
	ProcChain m_procChain;

#if defined(DEBUG_TEST)
	PSG m_psg;
	Data m_output;
	uint8_t m_reg = 0xFF;
	void debug_psg_write(uint8_t data);
#endif
};
