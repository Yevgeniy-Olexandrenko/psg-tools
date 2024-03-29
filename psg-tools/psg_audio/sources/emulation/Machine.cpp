#include "Machine.h"
#include <cassert>

void Machine::Bus::Handler::OnBusWrite(int tag, Addr addr, Data data)
{
	// DO NOTHING
}

Machine::Data Machine::Bus::Handler::OnBusRead(int tag, Addr addr) const
{
	return Bus::OPEN;
}

void Machine::Bus::AddWriteReadHandler(Handler* handler, Addr from, Addr to, int tag)
{
	AddWriteHandler(handler, from, to, tag);
	AddReadHandler(handler, from, to, tag);
}

void Machine::Bus::AddWriteHandler(Handler* handler, Addr from, Addr to, int tag)
{
	assert(handler != nullptr && from <= to);
	m_writeHandlers.push_back({ handler, from, to, tag });
}

void Machine::Bus::AddReadHandler(Handler* handler, Addr from, Addr to, int tag)
{
	assert(handler != nullptr && from <= to);
	m_readHandlers.push_back({ handler, from, to, tag });
}

void Machine::Bus::Write(Addr addr, Data data)
{
	for (auto& range : m_writeHandlers)
	{
		if (addr >= range.from && addr <= range.to)
		{
			range.handler->OnBusWrite(range.tag, addr, data);
			break;
		}
	}
}

Machine::Data Machine::Bus::Read(Addr addr)
{
	Data data = Bus::OPEN;
	for (auto& range : m_readHandlers)
	{
		if (addr >= range.from && addr <= range.to)
		{
			data = range.handler->OnBusRead(range.tag, addr);
			break;
		}
	}
	return data;
}

Machine::Bus& Machine::GetBus()
{
	return m_bus;
}
