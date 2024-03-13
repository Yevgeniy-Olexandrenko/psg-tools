#pragma once

#include <set>
#include "glob/glob.hpp"

class Filelist
{
public:
	using Path = std::filesystem::path;
	using Hash = std::size_t;

	static Path ConvertToAbsolute(const Path& base, const Path& path);
	static std::string GetExtension(const Path& path);
	static bool IsPlaylistPath(const Path& path);

public:
	Filelist(const std::string& exts);
	Filelist(const std::string& exts, const Path& path);
	void Append(const Path& path);

	bool IsEmpty() const;
	size_t GetNumberOfFiles() const;
	const Path& GetFileByIndex(size_t index) const;
		
	void RandomShuffle();
	bool EraseFile(const Path& path);
	bool InsertFile(const Path& path);
	bool ContainsFile(const Path& path);

	Path GetPlaylistPath() const;
	bool ExportPlaylist(const Path& path);
	bool ExportPlaylist();

private:
	void ImportDirectory(const Path& path);
	void ImportPlaylistM3U(const Path& path);
	void ImportPlaylistAYL(const Path& path);

	bool ExportPlaylistM3U(const Path& path);
	bool ExportPlaylistAYL(const Path& path);

private:
	Path m_playlistPath;
	std::vector<Path> m_exts;
	std::vector<Path> m_files;
	std::set<Hash> m_hashes;
};
