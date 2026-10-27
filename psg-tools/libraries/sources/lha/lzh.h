#pragma once

// LZH archive container for a single file (used by the YM format).

#include <string>
#include "lh5.h"

namespace lzh
{
	using lh5::Result;

	// Unpacks the first file of an LZH archive (header levels 0, 1 and 2,
	// methods -lh0- and -lh5-) and checks its CRC. If 'out' has reserved
	// capacity, it is the maximum size of the unpacked file.
	Result Unpack(const uint8_t* in, size_t size, std::vector<uint8_t>& out, std::string* name = nullptr);

	// Packs data into an LZH archive with a single file (header level 0,
	// method -lh5-). If 'out' has reserved capacity, it is the maximum
	// size of the archive.
	Result Pack(const uint8_t* in, size_t size, const std::string& name, std::vector<uint8_t>& out);
}
