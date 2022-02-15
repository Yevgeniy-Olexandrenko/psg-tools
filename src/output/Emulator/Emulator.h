#pragma once

#include <ayumi/ayumi.h>
#include "output/Output.h"
#include "WaveAudio.h"

class Emulator : public Output, public WaveAudio
{
public:
	Emulator();
	virtual ~Emulator();
	std::string name() const;

public:
	bool Open() override;
	bool Init(const Module& module) override;
	bool OutFrame(const Frame& frame, bool force) override;
	void Close() override;

protected:
	void FillBuffer(unsigned char* buffer, unsigned long size) override;

private:
	bool InitChip(uint8_t chipIndex);
	void WriteToChip(uint8_t chipIndex, const Frame& frame, bool force);

private:
	ayumi m_ay[2];
};
