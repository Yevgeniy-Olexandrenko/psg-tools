#include <algorithm>
#include <cstring>
#include "DecodeNSF.h"
#include "filelist/TrackPath.h"
#include "simulation/SimFactory.h"

namespace
{
	const size_t k_maxFileSize = (4 << 20);
	const int k_sampleRate = 44100;

	// The file has no length of the songs. The decoding ends at the time limit
	// (as for the AY songs without a length), or after a silence that is longer
	// than the pauses in a song: then the song has ended. The stream keeps one
	// second of the silence, as the player does (Stream::Play).
	const int k_maxTime = (5 * 60 * k_sampleRate);
	const int k_silenceTime = (3 * k_sampleRate);
	const int k_keptSilenceSec = 1;

	// the names of the expansion chips, in the order of their bits
	const char* const c_expansions[] = { "VRC6", "VRC7", "FDS", "MMC5", "N163", "5B" };

	bool ReadFile(const std::filesystem::path& path, std::vector<uint8_t>& data)
	{
		std::string ext = path.extension().string();
		std::for_each(ext.begin(), ext.end(), [](char& c) { c = char(::tolower(c)); });
		if (ext != ".nsf") return false;

		std::ifstream input(path, std::fstream::binary);
		if (!input) return false;
		input.seekg(0, input.end);
		size_t size = size_t(input.tellg());
		if (size <= sizeof(NSFMachine::Header) || size > k_maxFileSize) return false;

		data.resize(size);
		input.seekg(0, input.beg);
		input.read((char*)data.data(), size);
		return (input && !memcmp(data.data(), "NESM\x1A", 5));
	}

	std::string String(const char* text, size_t size)
	{
		std::string result(text, strnlen(text, size));
		while (!result.empty() && result.back() == ' ') result.pop_back();
		while (!result.empty() && result.front() == ' ') result.erase(0, 1);
		return result;
	}
}

int DecodeNSF::CountTracks(const std::filesystem::path& path)
{
	std::vector<uint8_t> data;
	if (!ReadFile(path, data)) return 0;

	const auto& header = *reinterpret_cast<const NSFMachine::Header*>(data.data());
	return header.songs;
}

bool DecodeNSF::Open(Stream& stream)
{
	std::filesystem::path path = stream.file;
	int number = 0;
	TrackPath::Split(stream.file, path, number);
	if (!ReadFile(path, m_data)) return false;

	NSFMachine::Header header;
	memcpy(&header, m_data.data(), sizeof(header));

	// a file without a track number plays its first song
	int song = (number ? number : std::max<int>(header.startSong, 1)) - 1;
	if (song >= header.songs) return false;

	// The APU plays on the 1st PSG, the 5B on the 2nd one. Without a 2nd PSG
	// the 5B is not played, as the expansion chips without a simulator.
	const uint32_t cpuClock = NSFMachine::GetCpuClock(header);
	std::vector<SimFactory::Source> sources{ { ChipSim::Type::RP2A03, int(cpuClock) } };
	bool play5B = (header.expansion & NSFMachine::S5B);
	if (play5B)
	{
		sources.push_back({ ChipSim::Type::AY8910, int(cpuClock / NSFMachine::k_5BClockDivider), Chip::Model::YM2149 });
		m_sim = SimFactory::Create(sources, stream);
		if (!m_sim) sources.pop_back();
		play5B = bool(m_sim);
	}
	if (!play5B) m_sim = SimFactory::Create(sources, stream);
	if (!m_sim) return false;

	m_machine.reset(new NSFMachine(m_sim, header, play5B));
	if (!m_machine->Load(m_data.data() + sizeof(header), m_data.size() - sizeof(header), song))
	{
		m_machine.reset();
		return false;
	}

	// the type: the system, the expansion chips played and not played
	std::string played, muted;
	for (int bit = 0; bit < 6; ++bit)
	{
		if (!(header.expansion & (1 << bit))) continue;
		std::string& list = ((1 << bit) == NSFMachine::S5B && play5B ? played : muted);
		list += (list.empty() ? "" : ", ") + std::string(c_expansions[bit]);
	}
	std::string type = std::string("NSF machine (") + (NSFMachine::IsPAL(header) ? "PAL" : "NTSC");
	if (!played.empty()) type += ", " + played;
	if (!muted.empty()) type += ", not played: " + muted;
	stream.info.type = (type + ")");

	stream.info.title = String(header.songName, sizeof(header.songName));
	stream.info.artist = String(header.artistName, sizeof(header.artistName));
	stream.info.comment = String(header.copyright, sizeof(header.copyright));
	stream.play.frameRate = m_machine->GetPlayRate();

	m_frames = m_audibleFrames = 0;
	m_audibleTime = 0;
	m_endedInSilence = false;
	m_writesKeys.clear();
	return true;
}

bool DecodeNSF::Decode(Frame& frame)
{
	int time = m_machine->GetTime();
	if (time - m_audibleTime >= k_silenceTime)
	{
		m_endedInSilence = true;
		return false;
	}
	if (time >= k_maxTime) return false;

	m_machine->RunFrame(frame);
	m_writesKeys.push_back(m_machine->GetWritesKey());
	m_frames++;
	if (frame.IsAudible())
	{
		m_audibleFrames = m_frames;
		m_audibleTime = m_machine->GetTime();
	}
	return true;
}

void DecodeNSF::Close(Stream& stream)
{
	// the rate of the play calls, or of the updates when the frames are cut
	// without them (a play routine that never returns)
	FrameRate frameRate = stream.play.frameRate;
	if (m_machine) m_machine->GetFrameRate(frameRate);
	stream.play.frameRate = frameRate;

	// a song that ends in silence keeps a second of it
	size_t length = 0;
	if (m_endedInSilence)
	{
		length = std::max<size_t>(1, m_audibleFrames + size_t(frameRate * k_keptSilenceSec));
	}
	stream.Finalize(0, true, length, &m_writesKeys);

	m_machine.reset();
	m_sim.reset();
	m_data.clear();
	m_writesKeys.clear();
}
