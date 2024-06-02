#include "BitStream.h"
#include <cassert>

BitOutputStream::BitOutputStream(std::ostream& stream)
	: m_stream(stream)
	, m_buffer(0)
	, m_count(0)
{
}

BitOutputStream::~BitOutputStream()
{
    Flush();
}

BitOutputStream& BitOutputStream::Write(uint32_t value, size_t bits)
{
    constexpr auto MAX_BITS = 8 * sizeof(value);
    assert(bits > 0 && bits <= MAX_BITS);

    value <<= (MAX_BITS - bits);
    for (; bits > 0; --bits, value <<= 1)
    {
        m_buffer <<= 1;
        m_buffer |= (value >> (MAX_BITS - 1) & 1);
        if (++m_count == 8) Flush();
    }
    return *this;
}

BitOutputStream& BitOutputStream::Flush()
{
    if (m_count > 0)
    {
        m_stream.put(m_buffer << (8 - m_count));
        m_buffer = m_count = 0;
    }
    return *this;
}

BitInputStream::BitInputStream(std::istream& stream)
    : m_stream(stream)
    , m_buffer(0)
    , m_count(0)
{
}

BitInputStream::~BitInputStream()
{
    // TODO
}
