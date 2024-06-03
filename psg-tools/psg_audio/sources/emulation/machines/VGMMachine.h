#pragma once

#include "emulation/Machine.h"
#include "stream/Frame.h"

class VGMMachine : public Machine
{
public:
	virtual void Init() = 0;
	virtual void Write(int chip, addr_t reg, data_t data) = 0;
	virtual void DataBlock(addr_t addr, size_t size) = 0;
	virtual void Simulate(int samples) = 0;
	virtual void Convert(Frame& frame) = 0;
};

class AY89XXPsgMachine : public VGMMachine
{

};

class RP2A03ApuMachine : public VGMMachine
{

};

class SN76489PsgMachine : public VGMMachine
{

};

class GBDMGApuMachine : public VGMMachine
{

};
