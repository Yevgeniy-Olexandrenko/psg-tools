#include "SerialPort.h"

SerialPort::SerialPort()
	: m_port(NULL)
{
}

SerialPort::~SerialPort()
{
	Close();
}

void SerialPort::Open(int index)
{
	TCHAR comName[100];
	wsprintf(comName, TEXT("\\\\.\\COM%d"), index);

	HANDLE hComm = CreateFile(
		comName,      //port name 
		GENERIC_READ | GENERIC_WRITE,		 
		0,            // No Sharing                               
		NULL,         // No Security                              
		OPEN_EXISTING,// Open existing port only                     
		0,            // Non Overlapped I/O                           
		NULL);        // Null for Comm Devices

	if (hComm == INVALID_HANDLE_VALUE) return;

	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 50;
	timeouts.ReadTotalTimeoutConstant = 50;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.WriteTotalTimeoutConstant = 50;
	timeouts.WriteTotalTimeoutMultiplier = 10;

	if (SetCommTimeouts(hComm, &timeouts) == FALSE) return;
	if (SetCommMask(hComm, EV_RXCHAR) == FALSE) return;

	Close();
	m_port = hComm;
}

void SerialPort::Close()
{
	if (m_port)
	{
		CloseHandle(m_port);
		m_port = NULL;
	}
}

bool SerialPort::SetBaudRate(BaudRate baudRate)
{
	DCB serialParams;
	if (GetSerialParams(serialParams))
	{
		serialParams.BaudRate = DWORD(baudRate);
		return bool(SetCommState(m_port, &serialParams));
	}
	return false;
}

bool SerialPort::SetDataBits(DataBits dataBits)
{
	DCB serialParams;
	if (GetSerialParams(serialParams))
	{
		serialParams.ByteSize = BYTE(dataBits);
		return bool(SetCommState(m_port, &serialParams));
	}
	return false;
}

bool SerialPort::SetStopBits(StopBits stopBits)
{
	DCB serialParams;
	if (GetSerialParams(serialParams))
	{
		serialParams.StopBits = BYTE(stopBits);
		return bool(SetCommState(m_port, &serialParams));
	}
	return false;
}

bool SerialPort::SetParity(Parity parity)
{
	DCB serialParams;
	if (GetSerialParams(serialParams))
	{
		serialParams.Parity = BYTE(parity);
		return bool(SetCommState(m_port, &serialParams));
	}
	return false;
}

bool SerialPort::SetDTRControl(DTRControl dtrControl)
{
	DCB serialParams;
	if (GetSerialParams(serialParams))
	{
		serialParams.fDtrControl = BYTE(dtrControl);
		return bool(SetCommState(m_port, &serialParams));
	}
	return false;
}

bool SerialPort::SetRTSControl(RTSControl rtsControl)
{
	DCB serialParams;
	if (GetSerialParams(serialParams))
	{
		serialParams.fRtsControl = BYTE(rtsControl);
		return bool(SetCommState(m_port, &serialParams));
	}
	return false;
}

SerialPort::BaudRate SerialPort::GetBaudRate() const
{
	DCB serialParams;
	if (GetSerialParams(serialParams))
	{
		return SerialPort::BaudRate(serialParams.BaudRate);
	}
	return SerialPort::BaudRate::UNKNOWN;
}

SerialPort::DataBits SerialPort::GetDataBits() const
{
	DCB serialParams;
	if (GetSerialParams(serialParams))
	{
		return SerialPort::DataBits(serialParams.ByteSize);
	}
	return SerialPort::DataBits::UNKNOWN;
}

SerialPort::StopBits SerialPort::GetStopBits() const
{
	DCB serialParams;
	if (GetSerialParams(serialParams))
	{
		return SerialPort::StopBits(serialParams.StopBits);
	}
	return SerialPort::StopBits::UNKNOWN;
}

SerialPort::Parity SerialPort::GetParity() const
{
	DCB serialParams;
	if (GetSerialParams(serialParams))
	{
		return SerialPort::Parity(serialParams.Parity);
	}
	return SerialPort::Parity::UNKNOWN;
}

SerialPort::DTRControl SerialPort::GetDTRControl() const
{
	DCB serialParams;
	if (GetSerialParams(serialParams))
	{
		return SerialPort::DTRControl(serialParams.fDtrControl);
	}
	return SerialPort::DTRControl::UNKNOWN;
}

SerialPort::RTSControl SerialPort::GetRTSControl() const
{
	DCB serialParams;
	if (GetSerialParams(serialParams))
	{
		return SerialPort::RTSControl(serialParams.fRtsControl);
	}
	return SerialPort::RTSControl::UNKNOWN;
}

int SerialPort::SendText(const char* data)
{
	int size = int(strlen(data));
	return SendBinary(data, size);
}

int SerialPort::SendBinary(const char* data, int size)
{
	DWORD bytesToWrite = size;
	DWORD bytesWritten;

	BOOL status = WriteFile(m_port, data, bytesToWrite, &bytesWritten, NULL);
	if (status == FALSE) return -1;

	return bytesWritten;
}

int SerialPort::ReciveText(char* buffer, int size)
{
	DWORD eventMask;
	DWORD bytesRead;

	BOOL status = WaitCommEvent(m_port, &eventMask, NULL);
	if (status == FALSE) return FALSE;
	
	status = ReadFile(m_port, buffer, size, &bytesRead, NULL);
	if (status == FALSE) return -1;
	
	buffer[bytesRead] = 0;
	return bytesRead;
}

bool SerialPort::GetSerialParams(DCB& serialParams) const
{
	serialParams = { 0 };
	serialParams.DCBlength = sizeof(serialParams);
	return bool(GetCommState(m_port, &serialParams));
}
