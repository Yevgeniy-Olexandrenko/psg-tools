#pragma once

#include <set>
#include <glob.hpp>

class Filelist
{
	using FSPath = std::filesystem::path;
	using FSHash = std::size_t;

public:
	static FSPath ConvertToAbsolute(const FSPath& base, const FSPath& path);
	static std::string GetExtension(const FSPath& path);
	static bool IsPlaylistPath(const FSPath& path);

public:
	Filelist(const std::string& exts);
	Filelist(const std::string& exts, const FSPath& path);
	void Append(const FSPath& path);

public:
	bool IsEmpty() const;
	int  GetNumberOfFiles() const;
	int  GetCurrFileIndex() const;

	bool GetPrevFile(FSPath& path) const;
	bool GetNextFile(FSPath& path) const;
	void RandomShuffle();

	bool EraseFile(const FSPath& path);
	bool InsertFile(const FSPath& path);
	bool ContainsFile(const FSPath& path);

	FSPath GetPlaylistPath() const;
	bool ExportPlaylist(const FSPath& path);
	bool ExportPlaylist();

private:
	void ImportDirectory(const FSPath& path);
	void ImportPlaylistM3U(const FSPath& path);
	void ImportPlaylistAYL(const FSPath& path);

	bool ExportPlaylistM3U(const FSPath& path);
	bool ExportPlaylistAYL(const FSPath& path);

private:
	FSPath m_playlistPath;
	std::vector<FSPath> m_exts;
	std::vector<FSPath> m_files;
	std::set<FSHash> m_hashes;
	mutable int32_t m_index;
};
