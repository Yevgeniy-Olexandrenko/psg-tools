#pragma once

#include "Machine.h"
#include <algorithm>
#include <iostream> // debug

template<data_t clear>
class Memory : public Machine::Component
{
public:
    // Fill memory with specific clear value
    void Clear()
    {
        assert(!m_data.empty());
        std::fill(m_data.begin(), m_data.end(), clear);
    }

    // Direct access to the memory buffer
    data_t* Data() { return m_data.data(); }
    size_t  Size() const { return m_data.size(); }

    // Attach mirror in another address space
    virtual Machine::Bus& AttachMirror(addr_t from_, addr_t to_) = 0;

protected:
    std::vector<data_t> m_data;
};

// ----------------------
// ROM (Read Only Memory)
// ----------------------
template<addr_t from, addr_t to, data_t clear = 0xFF>
class ROM : public Memory<clear>
{
public:
    Machine::Bus& AttachToBus(Machine::Bus& bus) override final
    {
        assert(from <= to && this->m_data.empty());
        this->m_data.resize(to - from + 1, clear);

        return Memory<clear>::AttachToBus(bus)
            .AttachRDHandler(from, to, &ROM::OnRead, this);
    }

    Machine::Bus& AttachMirror(addr_t from_, addr_t to_) override final
    {
        assert(from_ <= to_ && to_ - from_ + 1 == this->m_data.size());
        return Memory<clear>::GetBus()
            .AttachRDHandler(from_, to_, &ROM::OnRead, this);
    }

protected:
    void OnRead(addr_t addr, data_t& data)
    {
        data = this->m_data[addr];
        std::cout << "rom_read (" << int(from + addr) << ", " << int(data) << ")\n";
    }
};

// --------------------------
// RAM (Random Access Memory)
// --------------------------
template<addr_t from, addr_t to, data_t clear = 0x00>
class RAM : public Memory<clear>
{
public:
    Machine::Bus& AttachToBus(Machine::Bus& bus) override final
    {
        assert(from <= to && this->m_data.empty());
        this->m_data.resize(to - from + 1, clear);

        return Memory<clear>::AttachToBus(bus)
            .AttachWRHandler(from, to, &RAM::OnWrite, this)
            .AttachRDHandler(from, to, &RAM::OnRead, this);
    }

    Machine::Bus& AttachMirror(addr_t from_, addr_t to_) override final
    {
        assert(from_ <= to_ && to_ - from_ + 1 == this->m_data.size());
        return Memory<clear>::GetBus()
            .AttachWRHandler(from_, to_, &RAM::OnWrite, this)
            .AttachRDHandler(from_, to_, &RAM::OnRead, this);
    }

protected:
    void OnWrite(addr_t addr, data_t& data)
    {
        this->m_data[addr] = data;
        std::cout << "ram_write(" << int(from + addr) << ", " << int(data) << ")\n";
    }

    void OnRead(addr_t addr, data_t& data)
    {
        data = this->m_data[addr];
        std::cout << "ram_read (" << int(from + addr) << ", " << int(data) << ")\n";
    }
};
