#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include "Filelist.h"

namespace
{
    const std::string c_extPlaylistM3U{ ".m3u" };
    const std::string c_extPlaylistAYL{ ".ayl" };
}

////////////////////////////////////////////////////////////////////////////////

Filelist::FSPath Filelist::ConvertToAbsolute(const FSPath& base, const FSPath& path)
{
    if (base.is_absolute() && path.is_relative())
    {
        std::filesystem::path absolute = base;
        absolute.replace_filename(path);
        return absolute.lexically_normal();
    }
    return path;
}

std::string Filelist::GetExtension(const FSPath& path)
{
    auto extension = path.extension().string();
    std::for_each(extension.begin(), extension.end(), [](char& c) { c = ::tolower(c); });
    return extension;
}

bool Filelist::IsPlaylistPath(const FSPath& path)
{
    if (std::filesystem::is_regular_file(path))
    {
        auto extension = GetExtension(path);
        return (extension == c_extPlaylistM3U || extension == c_extPlaylistAYL);
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////

Filelist::Filelist(const std::string& exts)
    : m_index(-1)
{
    std::string ext;
    std::stringstream ss(exts);
    
    while (getline(ss, ext, '|')) 
    {
        m_exts.push_back("." + ext);
    }
}

Filelist::Filelist(const std::string& exts, const FSPath& path)
    : Filelist(exts)
{
    Append(path);
    if (IsPlaylistPath(path))
    {
        m_playlistPath = std::filesystem::absolute(path);
    }
}

////////////////////////////////////////////////////////////////////////////////

void Filelist::Append(const FSPath& path)
{
    for (auto path : glob::glob(path.string()))
    {
        if (path.is_relative())
        {
            path = std::filesystem::absolute(path);
        }

        if (std::filesystem::is_regular_file(path))
        {
            auto extension = GetExtension(path);
            if (extension == c_extPlaylistM3U)
            {
                ImportPlaylistM3U(path);
            }
            else if (extension == c_extPlaylistAYL)
            {
                ImportPlaylistAYL(path);
            }
            else
            {
                InsertFile(path);
            }
        }
        else if (std::filesystem::is_directory(path))
        {
            ImportDirectory(path);
        }
    }
}

bool Filelist::IsEmpty() const
{
    return m_files.empty();
}

int Filelist::GetNumberOfFiles() const
{
    return (int)m_files.size();
}

int Filelist::GetCurrFileIndex() const
{
    return m_index;
}

bool Filelist::GetPrevFile(FSPath& path) const
{
    if (!IsEmpty())
    {
        if (m_index > 0)
        {
            path = m_files[--m_index];
            return true;
        }
    }
    return false;
}

bool Filelist::GetNextFile(FSPath& path) const
{
    if (!IsEmpty())
    {
        if (m_index < int(m_files.size() - 1))
        {
            path = m_files[++m_index];
            return true;
        }
    }
    return false;
}

void Filelist::RandomShuffle()
{
    std::random_device randomDevice;
    std::mt19937 randomGenerator(randomDevice());
    std::shuffle(m_files.begin(), m_files.end(), randomGenerator);
    m_index = -1;
}

bool Filelist::EraseFile(const FSPath& path)
{
    auto hashIt = m_hashes.find(std::filesystem::hash_value(path));
    auto fileIt = std::find(m_files.begin(), m_files.end(), path);

    if (hashIt != m_hashes.end() && fileIt != m_files.end())
    {
        m_hashes.erase(hashIt);
        m_files.erase(fileIt);
        return true;
    }
    return false;
}

bool Filelist::InsertFile(const FSPath& path)
{
    if (std::filesystem::is_regular_file(path))
    {
        auto extension = GetExtension(path);
        if (std::find(m_exts.begin(), m_exts.end(), extension) != m_exts.end())
        {
            if (std::filesystem::exists(path))
            {
                if (m_hashes.insert(std::filesystem::hash_value(path)).second)
                {
                    m_files.push_back(path);
                    return true;
                }
            }
        }
    }
    return false;
}

bool Filelist::ContainsFile(const FSPath& path)
{
    return (m_hashes.find(std::filesystem::hash_value(path)) != m_hashes.end());
}

Filelist::FSPath Filelist::GetPlaylistPath() const
{
    return m_playlistPath;
}

bool Filelist::ExportPlaylist(const FSPath& path)
{
    if (std::filesystem::is_regular_file(path))
    {
        auto extension = GetExtension(path);
        if (extension == c_extPlaylistM3U)
        {
            return ExportPlaylistM3U(std::filesystem::absolute(path));
        }
        else if (extension == c_extPlaylistAYL)
        {
            return ExportPlaylistAYL(std::filesystem::absolute(path));
        }
    }
    return false;
}

bool Filelist::ExportPlaylist()
{
    return ExportPlaylist(m_playlistPath);
}

////////////////////////////////////////////////////////////////////////////////

void Filelist::ImportDirectory(const FSPath& path)
{
    for (const auto& file : std::filesystem::directory_iterator(path))
    {
        if (std::filesystem::is_regular_file(file))
        {
            InsertFile(file);
        }
    }
}

void Filelist::ImportPlaylistM3U(const FSPath& path)
{
    std::ifstream fileStream;
    fileStream.open(path);

    if (fileStream)
    {
        std::string entity;
        while (getline(fileStream, entity))
        {
            if (!entity.empty() && entity[0] != '#')
            {
                auto entityPath = ConvertToAbsolute(path, entity);
                InsertFile(entityPath);
            }
        }
        fileStream.close();
    }
}

void Filelist::ImportPlaylistAYL(const FSPath& path)
{
    std::ifstream fileStream;
    fileStream.open(path);

    if (fileStream)
    {
        std::string entity;
        bool skipLine = false;
        while (getline(fileStream, entity))
        {
            if (!entity.empty())
            {
                if (entity == "<") skipLine = true;
                else if (entity == ">") skipLine = false;
                else if (!skipLine)
                {
                    auto entityPath = ConvertToAbsolute(path, entity);
                    InsertFile(entityPath);
                }
            }
        }
        fileStream.close();
    }
}

bool Filelist::ExportPlaylistM3U(const FSPath& path)
{
    std::ofstream stream;
    stream.open(path);

    if (stream)
    {
        auto basePath = path.parent_path();
        for (const auto& filePath : m_files)
        {
            auto proximateFilePath = std::filesystem::proximate(filePath, basePath);
            stream << proximateFilePath.string() << std::endl;
        }

        stream.close();
        return true;
    }
    return false;
}

bool Filelist::ExportPlaylistAYL(const FSPath& path)
{
    std::ofstream stream;
    stream.open(path);

    if (stream)
    {
        stream << "ZX Spectrum Sound Chip Emulator Play List File v1.0" << std::endl;

        auto basePath = path.parent_path();
        for (const auto& filePath : m_files)
        {
            auto proximateFilePath = std::filesystem::proximate(filePath, basePath);
            stream << proximateFilePath.string() << std::endl;
        }

        stream.close();
        return true;
    }
    return false;
}
