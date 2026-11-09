#include <thread>
#include "Streamer.h"
#include "stream/Stream.h"

Streamer::Streamer(int comPortIndex)
	: m_portIndex(comPortIndex)
	, m_chips(1)
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
	dchip.second.model = Chip::Model::Unknown;
	dchip.clock = Chip::Clock::F1773400;
	if (!dchip.outputKnown || dchip.output == Chip::Output::Mono)
	{
		dchip.output = Chip::Output::Stereo;
		dchip.stereo = Chip::Stereo::ABC;
	}
	m_chips = dchip.count;
	m_timer.Start(stream.play.frameRate);
	return true;
}

bool Streamer::DeviceWrite(const FrameData& data)
{
	// prepare packet
	m_packet.clear();
	for (int chip = 0; chip < m_chips; ++chip)
	{
		m_packet.push_back(chip ? 0xFE : 0xFF);
		for (const auto& [reg, val] : data[chip])
		{
			m_packet.push_back(reg);
			m_packet.push_back(val);
		}
	}

	// wait for next frame start
	m_timer.Wait();

	// send packet
	auto dataSize = int(m_packet.size());
	auto sentSize = m_port.SendBinary(reinterpret_cast<const char*>(m_packet.data()), dataSize);
	return (sentSize == dataSize);
}

void Streamer::DeviceClose()
{
	Write(!Frame());
	m_port.Close();
}
