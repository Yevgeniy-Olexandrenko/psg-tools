#pragma once

#include "Filelist.h"

class FilelistTraversal : public Filelist
{
public:
	FilelistTraversal(const std::string& exts);
	FilelistTraversal(const std::string& exts, const Path& path);

	int  GetCurrFileIndex() const;

	bool GetPrevFile(Path& path) const;
	bool GetNextFile(Path& path) const;

	bool PeekPrevFile(Path& path) const;
	bool PeekNextFile(Path& path) const;

protected:
	mutable int m_index = -1;
};