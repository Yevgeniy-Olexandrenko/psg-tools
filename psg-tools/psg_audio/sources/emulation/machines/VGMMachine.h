#pragma once

#include "emulation/Machine.h"
#include "stream/Frame.h"

class VGMMachine : public Machine
{
public:
	virtual void Init() = 0;
	virtual void Write(int chip, uint16_t reg, uint8_t data) = 0;
	virtual void DataBlock(uint16_t addr, size_t size) = 0;
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
