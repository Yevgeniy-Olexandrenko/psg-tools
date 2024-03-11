#include <thread>
#include "Streamer.h"
#include "stream/Stream.h"

Streamer::Streamer(int comPortIndex)
	: m_portIndex(comPortIndex)
{
}

Streamer::~Streamer()
{
	DeviceClose();
}

const std::string& Streamer::GetDebugInfo() const
{
	return m_debugInfo;
}

const std::string Streamer::GetDeviceName() const
{
	return "Streamer";
}

bool Streamer::DeviceOpen()
{
	if (m_port.Open(m_portIndex))
	{
		// read debug info from streamer
		if (m_port.GetRTSControl() != SerialPort::RTSControl::DISABLE)
		{
			char buffer[1024] = { 0 };
			m_port.SetBaudRate(SerialPort::BaudRate::_9600);
			m_port.ReciveText(buffer, sizeof(buffer));
			m_debugInfo.assign(buffer);
		}

		// configure port for data steaming
		bool isOK = true;
		isOK &= m_port.SetBaudRate(SerialPort::BaudRate::_57600);
		isOK &= m_port.SetDataBits(SerialPort::DataBits::_8);
		isOK &= m_port.SetStopBits(SerialPort::StopBits::ONE);
		isOK &= m_port.SetParity(SerialPort::Parity::NO);
		isOK &= m_port.SetDTRControl(SerialPort::DTRControl::DISABLE);
		isOK &= m_port.SetRTSControl(SerialPort::RTSControl::DISABLE);
		return isOK;
	}
	return false;
}

bool Streamer::DeviceInit(const Stream& stream, Chip& dchip)
{
	dchip.second.model(Chip::Model::Unknown);
	dchip.clock(Chip::Clock::F1773400);
	if (!dchip.outputKnown() || dchip.output() == Chip::Output::Mono)
	{
		dchip.output(Chip::Output::Stereo);
		dchip.stereo(Chip::Stereo::ABC);
	}
	return true;
}

bool Streamer::DeviceWrite(int chip, const Data& data)
{
	// prepare packet
	std::vector<uint8_t> packet;
	packet.push_back(chip ? 0xFE : 0xFF);
	for (const auto& pair : data)
	{
		const uint8_t& reg = pair.first;
		const uint8_t& val = pair.second;
		packet.push_back(reg);
		packet.push_back(val);
	}
	
	// send packet
	auto dataSize = int(packet.size());
	auto dataBuff = reinterpret_cast<const char*>(packet.data());
	auto sentSize = m_port.SendBinary(dataBuff, dataSize);
	return (sentSize == dataSize);
}

void Streamer::DeviceClose()
{
	Write(!Frame());
	m_port.Close();
}
