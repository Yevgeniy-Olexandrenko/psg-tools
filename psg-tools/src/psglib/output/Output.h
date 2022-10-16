#pragma once

#include <array>
#include <vector>
#include "processing/Processing.h"

#define DBG_PROCESSING 0

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
	void SetEnables(const Enables& enables);

	const Frame& GetFrame() const;
	std::string toString() const;

protected:
	using Data = std::vector<std::pair<uint8_t, uint8_t>>;
	virtual const std::string GetDeviceName() const = 0;

	virtual bool OpenDevice() = 0;
	virtual bool ConfigureChip(const Chip& schip, Chip& dchip) = 0;
	virtual bool WriteToChip(int chip, const Data& data) = 0;
	virtual void CloseDevice() = 0;

private:
	void Reset() override;
	const Frame& operator()(const Frame& frame) override;

private:
	bool m_isOpened;
	Chip m_schip;
	Chip m_dchip;
	ProcChain m_procChain;
};
