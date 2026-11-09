#pragma once

#include "player/Output.h"
#include "player/FrameTimer.h"
#include "SerialPort.h"

class Streamer : public Output
{
public:
	Streamer(int comPortIndex);
	virtual ~Streamer();

	const std::string& GetDebugInfo() const;

protected:
	const std::string GetDeviceName() const override;

	bool DeviceOpen() override;
	bool DeviceInit(const Stream& stream, Chip& dchip) override;
	bool DeviceWrite(const FrameData& data) override;
	void DeviceClose() override;

private:
	int m_portIndex;
	SerialPort m_port;
	FrameTimer m_timer;
	std::string m_debugInfo;
	std::vector<uint8_t> m_packet;
	int m_chips;
};
