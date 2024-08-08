#pragma once

#include <iostream>

class BitOutputStream
{
public:
    BitOutputStream(std::ostream& stream);
    ~BitOutputStream();

    template<size_t width, typename T> BitOutputStream& Write(T value)
    {
        return Write(uint32_t(value), width);
    }

    BitOutputStream& Write(uint32_t value, size_t width = 8);
    BitOutputStream& Write(const char* data, size_t size);
    BitOutputStream& Flush();

protected:
    std::ostream& m_stream;
    uint8_t m_buffer;
    int m_count;
};

class BitInputStream
{
public:
    BitInputStream(std::istream& stream);
    ~BitInputStream();

    // TODO

protected:
    std::istream& m_stream;
    uint8_t m_buffer;
    int m_count;
};