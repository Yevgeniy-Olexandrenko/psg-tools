#pragma once

#include <memory>
#include <vector>
#include "decoders/Decoder.h"
#include "emulation/machines/NSFMachine.h"

// NSF files: the 6502 code of the music of a NES game, one or more songs.
// NSFMachine runs the code of a song. The file is a container: the tracks are
// its songs (see TrackPath). The APU plays on the 1st PSG, the Sunsoft 5B on
// the 2nd one; the sound of the other expansion chips is not played.
class DecodeNSF : public Decoder
{
public:
	bool Open(Stream& stream) override;
	bool Decode(Frame& frame) override;
	void Close(Stream& stream) override;

	// The number of the songs in an NSF file, 0 when the file is not one.
	static int CountTracks(const std::filesystem::path& path);

private:
	std::vector<uint8_t> m_data;
	std::shared_ptr<ChipSim> m_sim;
	std::unique_ptr<NSFMachine> m_machine;

	// The file has no length of the songs: the decoding ends after some
	// silence or at the time limit. The times are in samples of 44.1 kHz.
	size_t m_frames = 0;
	size_t m_audibleFrames = 0; // up to the last audible frame
	int  m_audibleTime = 0;     // of the end of the last audible frame
	bool m_endedInSilence = false;

	// The loop is searched by the writes of the player: the frame sequencer
	// of the APU runs on its own, so the frames of the passes can differ.
	std::vector<uint64_t> m_writesKeys;
};
