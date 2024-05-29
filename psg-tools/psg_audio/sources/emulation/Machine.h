#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <vector>

using data_t = uint8_t;
using addr_t = uint16_t;

class Machine
{
public:
    // --------------------------------------
    // Machine bus used to connect components
    // --------------------------------------
    class Bus
    {
        struct Handler  { addr_t from, to; std::function<void(addr_t, data_t&)> call; };
        using  Handlers = std::vector<Handler>;

    public:
        // Memory address space
        template<typename T> 
        Bus& AttachWRHandler(addr_t from, addr_t to, void (T::* method)(addr_t, data_t&), T* object)
        {
            assert(from <= to);
            m_writeHandlers.push_back({ from, to, std::bind(method, object, std::placeholders::_1, std::placeholders::_2) });
            return *this;
        }

        template<typename T>
        Bus& AttachRDHandler(addr_t from, addr_t to, void (T::* method)(addr_t, data_t&), T* object)
        {
            assert(from <= to);
            m_readHandlers.push_back({ from, to, std::bind(method, object, std::placeholders::_1, std::placeholders::_2) });
            return *this;
        }

        // I/O address space
        template<typename T>
        Bus& AttachIOWRHandler(addr_t from, addr_t to, void (T::* method)(addr_t, data_t&), T* object)
        {
            assert(from <= to);
            m_ioWriteHandlers.push_back({ from, to, std::bind(method, object, std::placeholders::_1, std::placeholders::_2) });
            return *this;
        }

        template<typename T>
        Bus& AttachIOWRHandler(addr_t addr, void (T::* method)(addr_t, data_t&), T* object)
        {
            return AttachIOWRHandler(addr, addr, object, method);
        }

        template<typename T>
        Bus& AttachIORDHandler(addr_t from, addr_t to, void (T::* method)(addr_t, data_t&), T* object)
        {
            assert(from <= to);
            m_ioReadHandlers.push_back({ from, to, std::bind(method, object, std::placeholders::_1, std::placeholders::_2) });
            return *this;
        }

        template<typename T>
        Bus& AttachIORDHandler(addr_t addr, void (T::* method)(addr_t, data_t&), T* object)
        {
            return AttachIORDHandler(addr, addr, object, method);
        }

    private:
        Handlers m_writeHandlers;
        Handlers m_readHandlers;
        Handlers m_ioWriteHandlers;
        Handlers m_ioReadHandlers;

    public:
        void Write(addr_t addr, data_t data);
        data_t Read(addr_t addr);

        void IOWrite(addr_t port, data_t data);
        data_t IORead(addr_t port);

    private:
        void HandleWrite(const Handlers& handlers, addr_t addr, data_t data);
        data_t HandleRead(const Handlers& handlers, addr_t addr);
    };

    // -----------------------------------
    // Component to be attached to the bus
    // -----------------------------------
    class Component
    {
    public:
        virtual Bus& AttachToBus(Bus& bus) { m_bus = &bus; return *m_bus; }
        virtual Bus& GetBus() const final  { assert(m_bus != nullptr); return *m_bus; }

    private:
        Bus* m_bus = nullptr;
    };

    // ---------------------------
    // Access to the machine's bus
    // ---------------------------
    Bus& GetBus() { return m_bus; }

private:
    Bus m_bus;
};
