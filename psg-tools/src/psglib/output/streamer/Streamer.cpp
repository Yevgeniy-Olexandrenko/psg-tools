#include <thread>
#include "Streamer.h"
#include "stream/Stream.h"

Streamer::Streamer(int comPortIndex)
	: m_portIndex(comPortIndex)
{
}

Streamer::~Streamer()
{
	CloseDevice();
}

const std::string& Streamer::GetDebugInfo() const
{
	return m_debugInfo;
}

const std::string Streamer::GetDeviceName() const
{
	return "Streamer";
}

bool Streamer::OpenDevice()
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

bool Streamer::ConfigureChip(const Chip& schip, Chip& dchip)
{
	dchip.second.model(Chip::Model::Unknown);
	dchip.clock(Chip::Clock::F1773400);
	dchip.output(Chip::Output::Stereo);

	if (!dchip.stereoKnown())
	{
		dchip.stereo(schip.stereoKnown() ? schip.stereo() : Chip::Stereo::ABC);
	}

	return true;
}

bool Streamer::WriteToChip(int chip, const Data& data)
{
	// prepare packet
	std::vector<uint8_t> binary;
	for (const auto& pair : data)
	{
		const uint8_t& reg = pair.first;
		const uint8_t& val = pair.second;
		binary.push_back(reg);
		binary.push_back(val);
	}
	binary.push_back(0xFF);
	
	// send packet
	auto dataSize = int(binary.size());
	auto dataBuff = reinterpret_cast<const char*>(binary.data());
	auto sentSize = m_port.SendBinary(dataBuff, dataSize);
	return (sentSize == dataSize);
}

void Streamer::CloseDevice()
{
	Write(!Frame());
	m_port.Close();
}
