#include <algorithm>
#include <cmath>
#include <cstring>
#include "DecodeAY.h"
#include "filelist/TrackPath.h"

// The format: the description of Project AY (by Patrik Rak). All words are big-endian. A pointer is a signed word, relative to
// its own position in the file. The header:
//   0 "ZXAY", 4 "EMUL", 8 file version, 9 player version, 10 special player,
//   12 author, 14 misc, 16 number of songs - 1, 17 first song, 18 songs.
// A song: name, data. The data: 0 the channels of the Amiga (4 bytes),
//   4 length, 6 fade length (frames of 50 Hz), 8 hiReg, 9 loReg, 10 points, 12 blocks.
// The points: stack, init, interrupt. The blocks: address, length, data; address 0 ends them.
namespace
{
	const size_t k_maxFileSize = (4 << 20);

	// The lengths are in frames of 50 Hz. A song without a length plays 5 minutes.
	// To find the loop, the machine plays the song two times as long, but not
	// more than 10 minutes.
	const int k_lengthRate = 50;
	const size_t k_defaultLength = (5 * 60 * k_lengthRate);
	const size_t k_maxLength = (10 * 60 * k_lengthRate);
	const int k_sampleRate = 44100;

	class File
	{
	public:
		explicit File(const std::vector<uint8_t>& data) : m_data(data) {}

		bool IsAY() const
		{
			return (m_data.size() >= 20 && !memcmp(m_data.data(), "ZXAYEMUL", 8));
		}

		int Songs() const { return m_data[16] + 1; }
		int FirstSong() const { return m_data[17]; }

		uint16_t Word(size_t at) const
		{
			return (at + 2 <= m_data.size() ? uint16_t(m_data[at] << 8 | m_data[at + 1]) : 0);
		}

		// -1 when the pointer is outside the file
		long Pointer(size_t at) const
		{
			if (at + 2 > m_data.size()) return -1;
			long target = long(at) + int16_t(Word(at));
			return (target >= 0 && size_t(target) < m_data.size() ? target : -1);
		}

		std::string String(long at) const
		{
			std::string text;
			for (size_t i = size_t(at); at >= 0 && i < m_data.size() && m_data[i]; ++i) text += char(m_data[i]);
			while (!text.empty() && text.back() == ' ') text.pop_back();
			while (!text.empty() && text.front() == ' ') text.erase(0, 1);
			return text;
		}

		// the entry of the song in the song table, -1 when it is outside the file
		long SongEntry(int song) const
		{
			long songs = Pointer(18);
			if (songs < 0 || size_t(songs) + 4 * (song + 1) > m_data.size()) return -1;
			return songs + 4 * song;
		}

		bool ReadSong(int song, AYMachine::Song& result, std::string& name, size_t& length) const
		{
			long entry = SongEntry(song);
			if (entry < 0) return false;
			long data = Pointer(entry + 2);
			if (data < 0 || size_t(data) + 14 > m_data.size()) return false;
			long points = Pointer(data + 10);
			if (points < 0 || size_t(points) + 6 > m_data.size()) return false;

			name = String(Pointer(entry));
			length = Word(data + 4);
			result.hiReg = m_data[data + 8];
			result.loReg = m_data[data + 9];
			result.stack = Word(points + 0);
			result.init = Word(points + 2);
			result.interrupt = Word(points + 4);

			// the blocks: the parts outside the memory or the file are cut
			result.blocks.clear();
			for (long at = Pointer(data + 12); at >= 0 && size_t(at) + 6 <= m_data.size(); at += 6)
			{
				uint16_t addr = Word(at);
				if (!addr) break;
				long offset = Pointer(at + 4);
				if (offset < 0) continue;
				uint32_t size = std::min<uint32_t>(Word(at + 2), uint32_t(m_data.size() - offset));
				result.blocks.push_back({ addr, m_data.data() + offset, size });
			}
			return !result.blocks.empty();
		}

	private:
		const std::vector<uint8_t>& m_data;
	};

	bool ReadFile(const std::filesystem::path& path, std::vector<uint8_t>& data)
	{
		std::string ext = path.extension().string();
		std::for_each(ext.begin(), ext.end(), [](char& c) { c = char(::tolower(c)); });
		if (ext != ".ay") return false;

		std::ifstream input(path, std::fstream::binary);
		if (!input) return false;
		input.seekg(0, input.end);
		size_t size = size_t(input.tellg());
		if (size < 20 || size > k_maxFileSize) return false;

		data.resize(size);
		input.seekg(0, input.beg);
		input.read((char*)data.data(), size);
		return bool(input);
	}
}

int DecodeAY::CountTracks(const std::filesystem::path& path)
{
	std::vector<uint8_t> data;
	if (!ReadFile(path, data)) return 0;

	File file(data);
	return (file.IsAY() ? file.Songs() : 0);
}

bool DecodeAY::Open(Stream& stream)
{
	std::filesystem::path path = stream.file;
	int number = 0;
	TrackPath::Split(stream.file, path, number);
	if (!ReadFile(path, m_data)) return false;

	File file(m_data);
	if (!file.IsAY()) return false;

	// a file without a track number plays its first song
	int song = (number ? number - 1 : file.FirstSong());
	if (song >= file.Songs()) return false;

	AYMachine::Song code;
	std::string name;
	if (!file.ReadSong(song, code, name, m_length)) return false;

	// The decoding is one pass: the play time is known before it, the rate
	// of the frames only after it (see Machine).
	m_machine.reset(new AYMachine());
	m_machine->Load(code);
	if (!m_length) m_length = k_defaultLength;
	m_maxTime = int(std::min(2 * m_length, k_maxLength) * (k_sampleRate / k_lengthRate));

	stream.info.title = name;
	stream.info.artist = file.String(file.Pointer(12));
	stream.info.comment = file.String(file.Pointer(14));
	stream.trackNumber = song + 1;
	stream.tracksCount = file.Songs();
	stream.play.frameRate = k_lengthRate;
	return true;
}

bool DecodeAY::Decode(Frame& frame)
{
	if (m_machine->GetTime() >= m_maxTime) return false;

	m_machine->RunFrame(frame);
	return true;
}

void DecodeAY::Close(Stream& stream)
{
	// the type and the clock of the AY are known only after the first writes
	bool cpc = (m_machine && m_machine->GetSystem() == AYMachine::System::CPC);
	stream.schip.clock = (cpc ? Chip::Clock::F1000000 : Chip::Clock::F1773400);

	std::string type = (cpc ? "AY file (Amstrad CPC)" : "AY file (ZX Spectrum)");
	if (stream.tracksCount > 1)
	{
		type += ", song " + std::to_string(stream.trackNumber) + " of " + std::to_string(stream.tracksCount);
	}
	stream.info.type = type;

	// A rate within 1% of the interrupts is a player in time with them:
	// its updates jitter by a few T-states.
	FrameRate frameRate = k_lengthRate;
	if (m_machine && m_machine->GetFrameRate(frameRate) && std::abs(frameRate - k_lengthRate) <= 0.5)
	{
		frameRate = k_lengthRate;
	}
	stream.play.frameRate = frameRate;
	stream.Finalize(0, true, size_t(m_length * frameRate / k_lengthRate + 0.5));
	m_machine.reset();
	m_data.clear();
}
