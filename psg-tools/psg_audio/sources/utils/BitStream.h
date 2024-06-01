#pragma once

#include <iostream>

class BitOutputStream
{
public:
    BitOutputStream(std::ostream& stream);
    ~BitOutputStream();

    template<size_t width, typename T> void Write(T value)
    {
        static_assert(width > 0 && width <= 8 * sizeof(uint32_t) && sizeof(T) <= sizeof(uint32_t));
        Write(value, width);
    }

    void Write(uint32_t value, size_t width);
    void Flush();

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

    uint32_t Read(size_t width);

protected:
    std::istream& m_stream;
    uint8_t m_buffer;
    int m_count;
};