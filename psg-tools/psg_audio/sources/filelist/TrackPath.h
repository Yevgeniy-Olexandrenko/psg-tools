#pragma once

#include <string>
#include <filesystem>

class Stream;

// A container file (AY, NSF) holds several tracks. The file list keeps each
// track as the path of the file with "#N" after its name, N from 1, and the
// playlists keep it so too: "music/Some File.ay#3".
namespace TrackPath
{
	using Path = std::filesystem::path;

	// The mark of the track number after the file name.
	const char k_numberMark = '#';

	// The output file of a track: <container name><separator><track name>, for
	// example "Some File_#Ingame". A track without a name takes its number:
	// "Some File_#03", with the leading zeros of the largest number (two digits at least).
	const std::string k_nameSeparator = "_#";

	Path Make(const Path& file, int number);

	// Splits the path of a track into the file and the number. Returns false
	// for other paths, also for a real file with "#N" at the end of its name.
	bool Split(const Path& path, Path& file, int& number);

	// The stem of an output file for the input path: the container name and the
	// track name (from the decoded stream) for a track, else the stem of the path.
	std::string OutputStem(const Path& path, const Stream& stream);
}
