#pragma once

#include <Windows.h>

class SerialPort
{
public:
	enum class BaudRate
	{
		_9600   = CBR_9600,
		_14400  = CBR_14400,
		_19200  = CBR_19200,
		_38400  = CBR_38400,
		_56000  = CBR_56000,
		_57600  = CBR_57600,
		_115200 = CBR_115200,
		_128000 = CBR_128000,
		_256000 = CBR_256000,
		UNKNOWN = -1
	};

	enum class DataBits
	{
		_5 = 5,
		_6 = 6,
		_7 = 7,
		_8 = 8,
		UNKNOWN = -1
	};

	enum class StopBits
	{
		ONE = ONESTOPBIT,
		TWO = TWOSTOPBITS,
		ONE_HALF = ONE5STOPBITS,
		UNKNOWN = -1
	};

	enum class Parity
	{
		NO = NOPARITY,
		ODD = ODDPARITY,
		EVEN = EVENPARITY,
		MARK = MARKPARITY,
		SPACE = SPACEPARITY,
		UNKNOWN = -1
	};

	enum class DTRControl
	{
		ENABLE = DTR_CONTROL_ENABLE,
		DISABLE = DTR_CONTROL_DISABLE,
		HANDSHAKE = DTR_CONTROL_HANDSHAKE,
		UNKNOWN = -1
	};

	enum class RTSControl
	{
		ENABLE = RTS_CONTROL_ENABLE,
		DISABLE = RTS_CONTROL_DISABLE,
		HANDSHAKE = RTS_CONTROL_HANDSHAKE,
		TOGGLE = RTS_CONTROL_TOGGLE,
		UNKNOWN = -1
	};

	SerialPort();
	~SerialPort();

public:
	void Open(int index);
	void Close();

	bool SetBaudRate(BaudRate baudRate);
	bool SetDataBits(DataBits dataBits);
	bool SetStopBits(StopBits stopBits);
	bool SetParity(Parity parity);
	bool SetDTRControl(DTRControl dtrControl);
	bool SetRTSControl(RTSControl rtsControl);

	BaudRate GetBaudRate() const;
	DataBits GetDataBits() const;
	StopBits GetStopBits() const;
	Parity GetParity() const;
	DTRControl GetDTRControl() const;
	RTSControl GetRTSControl() const;

	int SendText(const char* data);
	int SendBinary(const char* data, int size);
	int ReciveText(char* buffer, int size);

private:
	bool GetSerialParams(DCB& serialParams) const;

private:
	HANDLE m_port;
};
