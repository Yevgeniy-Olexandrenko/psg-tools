#include "Machine.h"

void Machine::Bus::Write(addr_t addr, data_t data)
{
    HandleWrite(m_writeHandlers, addr, data);
}

data_t Machine::Bus::Read(addr_t addr)
{
    return HandleRead(m_readHandlers, addr);
}

void Machine::Bus::IOWrite(addr_t port, data_t data)
{
    HandleWrite(m_ioWriteHandlers, port, data);
}

data_t Machine::Bus::IORead(addr_t port)
{
    return HandleRead(m_ioReadHandlers, port);
}

void Machine::Bus::HandleWrite(const Handlers& handlers, addr_t addr, data_t data)
{
    for (const auto& handler : handlers)
    {
        if (addr >= handler.from && addr <= handler.to)
        {
            data_t wrdata = data;
            handler.call(addr - handler.from, wrdata);
        }
    }
}

data_t Machine::Bus::HandleRead(const Handlers& handlers, addr_t addr)
{
    data_t data = 0xFF;
    for (const auto& handler : handlers)
    {
        if (addr >= handler.from && addr <= handler.to)
        {
            data_t rddata = data;
            handler.call(addr - handler.from, rddata);
            data &= rddata;
        }
    }
    return data;
}
