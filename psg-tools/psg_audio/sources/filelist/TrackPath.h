#pragma once

#include <string>
#include <filesystem>

// A container file (AY, NSF) holds several tracks. The file list keeps each
// track as the path of the file with the track number after its name, and
// the playlists keep it so too: "music/Some File.ay#03".
namespace TrackPath
{
	using Path = std::filesystem::path;

	// The mark of the track number after the file name.
	const char k_numberMark = '#';

	// The output file of a track: <container name><separator><track number>,
	// for example "Some File_#03".
	const std::string k_nameSeparator = "_";

	// The track number as the paths and the names show it: "#01" to "#99",
	// more digits for the numbers above.
	std::string Number(int number);

	Path Make(const Path& file, int number);

	// Splits the path of a track into the file and the number. Returns false
	// for other paths, also for a real file with "#N" at the end of its name.
	bool Split(const Path& path, Path& file, int& number);

	// The stem of an output file for the input path: the container name and
	// the track number for a track, else the stem of the path.
	std::string OutputStem(const Path& path);
}
