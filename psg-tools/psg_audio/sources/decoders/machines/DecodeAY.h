#pragma once

#include <memory>
#include <vector>
#include "decoders/Decoder.h"
#include "emulation/machines/AYMachine.h"

// AY files (ZXAYEMUL): the Z80 code of the player and the music, one or more
// songs. AYMachine runs the code of a song. The file is a container: the
// tracks are its songs (see TrackPath).
class DecodeAY : public Decoder
{
public:
	bool Open(Stream& stream) override;
	bool Decode(Frame& frame) override;
	void Close(Stream& stream) override;

	// The number of the songs in an AY file, 0 when the file is not one.
	static int CountTracks(const std::filesystem::path& path);

private:
	std::vector<uint8_t> m_data;
	std::unique_ptr<AYMachine> m_machine;

	size_t m_length = 0; // of the song, in frames of 50 Hz
	int m_maxTime = 0;   // of the decoding, in samples of 44.1 kHz
};
