#pragma once

#include <set>
#include <glob.hpp>

class Filelist
{
	using FilePath = std::filesystem::path;
	using PathHash = std::size_t;

public:
	Filelist(const std::string& exts);
	Filelist(const std::string& exts, const FilePath& path);
	void Append(const FilePath& path);

public:
	bool IsEmpty() const;
	uint32_t GetNumberOfFiles() const;
	int32_t  GetCurrFileIndex() const;

	bool GetPrevFile(FilePath& path) const;
	bool GetNextFile(FilePath& path) const;
	void RandomShuffle();

	bool EraseFile(const FilePath& path);
	bool InsertFile(const FilePath& path);
	bool ContainsFile(const FilePath& path);

	FilePath GetPlaylistPath() const;
	bool ExportPlaylist(const FilePath& path);
	bool ExportPlaylist();

private:
	void ImportFolder(const FilePath& path);
	void ImportPlaylistM3U(const FilePath& path);
	void ImportPlaylistAYL(const FilePath& path);

	bool ExportPlaylistM3U(const FilePath& path);
	bool ExportPlaylistAYL(const FilePath& path);

private:
	FilePath m_playlistPath;
	std::vector<FilePath> m_exts;
	std::vector<FilePath> m_files;
	std::set<PathHash> m_hashes;
	mutable int32_t m_index;
};
