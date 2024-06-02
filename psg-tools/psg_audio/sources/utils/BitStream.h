#pragma once

#include <iostream>

class BitOutputStream
{
public:
    BitOutputStream(std::ostream& stream);
    ~BitOutputStream();

    template<size_t width, typename T> BitOutputStream& Write(T value)
    {
        static_assert(width > 0 && width <= 8 * sizeof(uint32_t) && sizeof(T) <= sizeof(uint32_t));
        return Write(value, width);
    }

    BitOutputStream& Write(uint32_t value, size_t width);
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