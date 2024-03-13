#include "FilelistTraversal.h"

FilelistTraversal::FilelistTraversal(const std::string& exts)
    : Filelist(exts)
{
}

FilelistTraversal::FilelistTraversal(const std::string& exts, const Path& path)
    : Filelist(exts, path)
{
}

int FilelistTraversal::GetCurrFileIndex() const
{
    return m_index;
}

bool FilelistTraversal::GetPrevFile(Path& path) const
{
    if (!IsEmpty())
    {
        if (m_index > 0)
        {
            path = Filelist::GetFileByIndex(--m_index);
            return true;
        }
    }
    return false;
}

bool FilelistTraversal::GetNextFile(Path& path) const
{
    if (!IsEmpty())
    {
        if (m_index < int(GetNumberOfFiles() - 1))
        {
            path = Filelist::GetFileByIndex(++m_index);
            return true;
        }
    }
    return false;
}

bool FilelistTraversal::PeekPrevFile(Path& path) const
{
    if (!IsEmpty())
    {
        if (m_index > 0)
        {
            path = Filelist::GetFileByIndex(m_index - 1);
            return true;
        }
    }
    return false;
}

bool FilelistTraversal::PeekNextFile(Path& path) const
{
    if (!IsEmpty())
    {
        if (m_index < int(GetNumberOfFiles() - 1))
        {
            path = Filelist::GetFileByIndex(m_index + 1);
            return true;
        }
    }
    return false;
}
