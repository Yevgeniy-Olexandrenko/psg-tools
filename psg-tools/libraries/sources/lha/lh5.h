#pragma once

// LH5 (LHA -lh5-) compressor and decompressor,
// based on ar002 by Haruhiko Okumura (public domain).

#include <stdint.h>
#include <stddef.h>
#include <vector>

namespace lh5
{
	enum class Result
	{
		Ok,             // success
		BadData,        // corrupt compressed stream
		InputOverrun,   // compressed stream ends too early
		OutputOverflow, // output does not fit into the limit
		BadHeader,      // corrupt LZH archive header
		BadCrc,         // CRC of unpacked data does not match
		Unsupported,    // unsupported LZH method or header level
		OutOfMemory     // memory allocation failed
	};

	const char* ToString(Result result);

	// Both functions clear 'out' and append the result to it. If 'out' has
	// reserved capacity, it is the maximum size of the output: decompression
	// stops when the output reaches it, compression fails with OutputOverflow.
	// With zero capacity the output size is not limited, and decompression
	// stops at the end of the compressed stream.
	Result Decompress(const uint8_t* in, size_t size, std::vector<uint8_t>& out);
	Result Compress(const uint8_t* in, size_t size, std::vector<uint8_t>& out);

	// CRC-16 (polynomial 0xA001) used in LZH archive headers
	uint16_t Crc16(const uint8_t* data, size_t size, uint16_t crc = 0);
}
