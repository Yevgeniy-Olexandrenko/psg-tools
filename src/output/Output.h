#pragma once

#include "Chip.h"

class Module;
class Frame;

class Output
{
public:
	Chip chip;
	virtual std::string name() const = 0;

public:
	virtual bool Open() = 0;
	virtual bool Init(const Module& module) = 0;
	virtual bool OutFrame(const Frame& frame, bool force) = 0;
	virtual void Close() = 0;

protected:
	bool m_isOpened = false;
};
