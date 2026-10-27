#include "lzh.h"
#include <string.h>
#include <time.h>
#include <new>

namespace
{
	using lh5::Result;

	uint16_t GetU16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
	uint32_t GetU32(const uint8_t* p) { return uint32_t(p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24); }

	void PutU16(std::vector<uint8_t>& v, uint32_t x) { v.push_back(uint8_t(x)); v.push_back(uint8_t(x >> 8)); }
	void PutU32(std::vector<uint8_t>& v, uint32_t x) { PutU16(v, x); PutU16(v, x >> 16); }

	uint32_t GetDosDateTime()
	{
		time_t now = time(nullptr);
		struct tm t;
		if (localtime_s(&t, &now) || t.tm_year < 80) return 0x00210000; // 1980-01-01

		return uint32_t(t.tm_year - 80) << 25 | uint32_t(t.tm_mon + 1) << 21 | uint32_t(t.tm_mday) << 16
			| uint32_t(t.tm_hour) << 11 | uint32_t(t.tm_min) << 5 | uint32_t(t.tm_sec / 2);
	}

	// Walks the chain of extended headers that starts at 'pos' with the size
	// 'extSize'. Each one is: type (1), data, size of the next one (2).
	bool ReadExtHeaders(const uint8_t* in, size_t size, size_t pos, size_t extSize, size_t& extTotal, std::string& name)
	{
		extTotal = 0;
		while (extSize)
		{
			if (extSize < 3 || pos + extSize > size) return false;

			if (in[pos] == 0x01) // file name
				name.assign((const char*)&in[pos + 1], extSize - 3);

			extTotal += extSize;
			pos += extSize;
			extSize = GetU16(&in[pos - 2]);
		}
		return true;
	}
}

namespace lzh
{
	Result Unpack(const uint8_t* in, size_t size, std::vector<uint8_t>& out, std::string* name)
	{
		size_t limit = out.capacity();
		out.clear();

		// common part of the header for all levels
		if (size < 22) return Result::BadHeader;

		int level = in[20];
		uint32_t compSize = GetU32(&in[7]);
		uint32_t origSize = GetU32(&in[11]);
		bool isStored = !memcmp(&in[2], "-lh0-", 5);
		bool isPacked = !memcmp(&in[2], "-lh5-", 5);

		std::string fileName;
		size_t dataPos = 0;
		uint16_t crc = 0;

		if (level == 0 || level == 1)
		{
			size_t headerSize = size_t(in[0]) + 2;
			size_t nameSize = in[21];
			if (in[0] == 0) return Result::BadHeader; // end of archive
			if (headerSize > size || headerSize < 24 + nameSize) return Result::BadHeader;

			uint8_t sum = 0;
			for (size_t i = 2; i < headerSize; i++) sum += in[i];
			if (sum != in[1]) return Result::BadHeader;

			fileName.assign((const char*)&in[22], nameSize);
			crc = GetU16(&in[22 + nameSize]);
			dataPos = headerSize;

			if (level == 1)
			{
				// base header ends with the OS id and the size of the
				// first extended header; compSize includes ext headers
				if (headerSize < 27 + nameSize) return Result::BadHeader;

				size_t extTotal = 0;
				size_t extSize = GetU16(&in[headerSize - 2]);
				if (!ReadExtHeaders(in, size, headerSize, extSize, extTotal, fileName)) return Result::BadHeader;
				if (extTotal > compSize) return Result::BadHeader;

				dataPos += extTotal;
				compSize -= uint32_t(extTotal);
			}
		}
		else if (level == 2)
		{
			size_t headerSize = GetU16(&in[0]);
			if (size < 26 || headerSize < 26 || headerSize > size) return Result::BadHeader;

			crc = GetU16(&in[21]);
			size_t extTotal = 0;
			size_t extSize = GetU16(&in[24]);
			if (!ReadExtHeaders(in, headerSize, 26, extSize, extTotal, fileName)) return Result::BadHeader;
			dataPos = headerSize;
		}
		else return Result::Unsupported;

		if (!isStored && !isPacked) return Result::Unsupported;
		if (compSize > size - dataPos) return Result::InputOverrun;
		if (limit && origSize > limit) return Result::OutputOverflow;

		const uint8_t* data = &in[dataPos];
		if (isStored)
		{
			if (compSize != origSize) return Result::BadHeader;
			out.assign(data, data + origSize);
		}
		else
		{
			// a fresh vector, so that its capacity is exactly the original size
			std::vector<uint8_t> unpacked;
			try { unpacked.reserve(origSize); }
			catch (const std::bad_alloc&) { return Result::OutOfMemory; }

			Result result = lh5::Decompress(data, compSize, unpacked);
			if (result != Result::Ok) return result;
			if (unpacked.size() != origSize) return Result::InputOverrun;
			out.swap(unpacked);
		}

		if (lh5::Crc16(out.data(), out.size()) != crc)
		{
			out.clear();
			return Result::BadCrc;
		}
		if (name) *name = fileName;
		return Result::Ok;
	}

	Result Pack(const uint8_t* in, size_t size, const std::string& name, std::vector<uint8_t>& out)
	{
		size_t limit = out.capacity();
		out.clear();
		if (size > UINT32_MAX) return Result::OutputOverflow;

		std::vector<uint8_t> packed;
		Result result = lh5::Compress(in, size, packed);
		if (result != Result::Ok) return result;
		if (packed.size() > UINT32_MAX) return Result::OutputOverflow;

		// the header size is one byte, so the name is limited
		size_t nameSize = (name.size() < 233 ? name.size() : 233);
		size_t headerSize = 24 + nameSize;
		size_t archiveSize = headerSize + packed.size() + 1;
		if (limit && archiveSize > limit) return Result::OutputOverflow;

		std::vector<uint8_t> archive;
		archive.reserve(archiveSize);
		archive.push_back(uint8_t(headerSize - 2));     // header size
		archive.push_back(0);                           // header checksum
		archive.insert(archive.end(), { '-', 'l', 'h', '5', '-' });
		PutU32(archive, uint32_t(packed.size()));       // compressed size
		PutU32(archive, uint32_t(size));                // original size
		PutU32(archive, GetDosDateTime());              // MS-DOS time stamp
		archive.push_back(0x20);                        // MS-DOS attribute: archive
		archive.push_back(0x00);                        // header level
		archive.push_back(uint8_t(nameSize));           // file name
		archive.insert(archive.end(), name.begin(), name.begin() + nameSize);
		PutU16(archive, lh5::Crc16(in, size));          // CRC-16 of the file

		uint8_t sum = 0;
		for (size_t i = 2; i < headerSize; i++) sum += archive[i];
		archive[1] = sum;

		archive.insert(archive.end(), packed.begin(), packed.end());
		archive.push_back(0x00);                        // end of archive

		out.swap(archive);
		return Result::Ok;
	}
}
