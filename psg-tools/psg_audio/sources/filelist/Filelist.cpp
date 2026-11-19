#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include "Filelist.h"
#include "TrackPath.h"
#include "PSGAudio.h"

namespace
{
    const std::string c_extPlaylistM3U{ ".m3u" };
    const std::string c_extPlaylistAYL{ ".ayl" };
}

////////////////////////////////////////////////////////////////////////////////

Filelist::Path Filelist::ConvertToAbsolute(const Path& base, const Path& path)
{
    if (base.is_absolute() && path.is_relative())
    {
        std::filesystem::path absolute = base;
        absolute.replace_filename(path);
        return absolute.lexically_normal();
    }
    return path;
}

std::string Filelist::GetExtension(const Path& path)
{
    auto extension = path.extension().string();
    std::for_each(extension.begin(), extension.end(), ::tolower);
    return extension;
}

bool Filelist::IsPlaylistPath(const Path& path)
{
    auto extension = GetExtension(path);
    return (extension == c_extPlaylistM3U || extension == c_extPlaylistAYL);
}

////////////////////////////////////////////////////////////////////////////////

Filelist::Filelist(const std::string& exts)
//    : m_index(-1)
{
    std::string ext;
    std::stringstream ss(exts);
    
    while (getline(ss, ext, '|')) 
    {
        m_exts.push_back("." + ext);
    }
}

Filelist::Filelist(const std::string& exts, const Path& path)
    : Filelist(exts)
{
    Append(path);
    if (std::filesystem::is_regular_file(path) && IsPlaylistPath(path))
    {
        m_playlistPath = std::filesystem::absolute(path);
    }
}

////////////////////////////////////////////////////////////////////////////////

void Filelist::Append(const Path& path)
{
    // a track of a container is not a file on the disk, glob does not find it
    Path file;
    int number;
    if (TrackPath::Split(std::filesystem::absolute(path), file, number))
    {
        InsertFile(std::filesystem::absolute(path));
        return;
    }

    // A path that exists is taken as it is: its name can have the special
    // characters of glob ("[RAF World]" would be a set of characters).
    std::error_code error;
    std::vector<Path> paths;
    if (std::filesystem::exists(path, error)) paths.push_back(path);
    else paths = glob::glob(path.string());

    for (auto path : paths)
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

size_t Filelist::GetNumberOfFiles() const
{
    return m_files.size();
}

const Filelist::Path& Filelist::GetFileByIndex(size_t index) const
{
    static Path empty;
    return (index < m_files.size() ? m_files[index] : empty);
}

void Filelist::RandomShuffle()
{
    std::random_device randomDevice;
    std::mt19937 randomGenerator(randomDevice());
    std::shuffle(m_files.begin(), m_files.end(), randomGenerator);
//    m_index = -1;
}

bool Filelist::EraseFile(const Path& path)
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

bool Filelist::InsertFile(const Path& path)
{
    // a track of a container file
    Path file;
    int number;
    if (TrackPath::Split(path, file, number))
    {
        return (HasKnownExtension(file) && InsertPath(path));
    }

    if (std::filesystem::is_regular_file(path) && HasKnownExtension(path))
    {
        // a container file with more than one track gives its tracks
        int tracks = FileDecoder::CountTracks(path);
        if (tracks > 1)
        {
            bool inserted = false;
            for (int track = 1; track <= tracks; ++track)
            {
                inserted |= InsertPath(TrackPath::Make(path, track));
            }
            return inserted;
        }
        return InsertPath(path);
    }
    return false;
}

bool Filelist::HasKnownExtension(const Path& path) const
{
    return (std::find(m_exts.begin(), m_exts.end(), GetExtension(path)) != m_exts.end());
}

bool Filelist::InsertPath(const Path& path)
{
    if (m_hashes.insert(std::filesystem::hash_value(path)).second)
    {
        m_files.push_back(path);
        return true;
    }
    return false;
}

bool Filelist::ContainsFile(const Path& path)
{
    return (m_hashes.find(std::filesystem::hash_value(path)) != m_hashes.end());
}

Filelist::Path Filelist::GetPlaylistPath() const
{
    return m_playlistPath;
}

bool Filelist::ExportPlaylist(const Path& path)
{
    if (IsPlaylistPath(path))
    {
        std::filesystem::create_directories(path.parent_path());

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

void Filelist::ImportDirectory(const Path& path)
{
    for (const auto& file : std::filesystem::directory_iterator(path))
    {
        if (std::filesystem::is_regular_file(file))
        {
            InsertFile(file);
        }
    }
}

void Filelist::ImportPlaylistM3U(const Path& path)
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

void Filelist::ImportPlaylistAYL(const Path& path)
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

bool Filelist::ExportPlaylistM3U(const Path& path)
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

bool Filelist::ExportPlaylistAYL(const Path& path)
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
