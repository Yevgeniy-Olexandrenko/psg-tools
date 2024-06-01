#pragma once

#include <sstream>
#include "stream/Property.h"
#include "encoders/Encoder.h"
#include "utils/BitStream.h"

#define DBG_ENCODE_AYM 1

class EncodeAYM : public Encoder
{
	struct Delta
	{
		int16_t value;
		uint8_t bits;

		Delta(uint16_t from, uint16_t to);
	};

	class DeltaList
	{
	public:
		DeltaList();
		int8_t GetIndex(const Delta& delta);

	private:
		int16_t m_list[32];
		uint8_t m_index;
	};

	class Chunk : public BitOutputStream
	{
	public:
		Chunk();
		void Finish();
		
		const uint8_t* GetData() const;
		const size_t GetSize() const;

	protected:
		std::ostringstream m_stream;
		std::string m_data;
	};

public:
	bool Open(const Stream& stream) override;
	void Encode(const Frame& frame) override;
	void Close(const Stream& stream) override;

private:
	void WriteDelta(const Delta& delta, BitOutputStream& stream);
	void WriteChipData(const Frame& frame, int chip, bool isLast, BitOutputStream& stream);
	void WriteFrameChunk(const Frame& frame);
	void WriteStepChunk();
	void WriteChunk(const Chunk& chunk);

private:
	std::ofstream m_output;
	DeltaList m_deltaList;
	uint16_t m_oldStep = 1;
	uint16_t m_newStep = 1;
	Frame m_frame;
	bool m_isTS;
};

