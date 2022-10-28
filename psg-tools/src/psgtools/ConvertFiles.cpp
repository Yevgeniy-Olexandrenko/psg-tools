#include "ConvertFiles.h"

ConvertFiles::ConvertFiles(Chip& chip, Filelist& filelist, Filelist::FSPath& output, Termination& termination)
    : m_chip(chip)
    , m_filelist(filelist)
    , m_output(output)
    , m_termination(termination)
{
}

void ConvertFiles::Convert()
{
    Filelist::FSPath inputPath, outputPath;
    outputPath = std::filesystem::absolute(m_output);
    outputPath.remove_filename();

    if (Filelist::IsPlaylistPath(m_output))
    {
        // write input files as playlist
        outputPath.replace_filename(m_output.filename());
        ExportPlaylist(outputPath);
    }
    else
    {
        std::string outputType;
        if (!m_output.has_parent_path() && m_output.has_stem() && !m_output.has_extension())
        {
            // probably only desired output type is provided
            outputType = "." + m_output.stem().string();
            m_output.replace_filename("*" + outputType);
        }
        else if (m_output.has_extension())
        {
            // determine output type from output file extension
            outputType = m_output.extension().string();
        }
            
        if (!outputType.empty())
        {
            std::for_each(outputType.begin(), outputType.end(), ::tolower);
            while (m_filelist.GetNextFile(inputPath))
            {
                std::string inputName = inputPath.stem().string();
                std::string outputName = m_output.stem().string();

                size_t star = outputName.find('*');
                if (star != std::string::npos)
                    outputName.replace(star, 1, inputName);

                outputPath.replace_filename(outputName);
                outputPath.replace_extension(outputType);

                std::filesystem::create_directories(outputPath.parent_path());
                ConvertFile(inputPath, outputPath);

                // single file case
                if (star == std::string::npos) break;
            }
        }
    }
}

void ConvertFiles::ExportPlaylist(const Filelist::FSPath& output)
{
    std::cout << "output: " << output.string() << std::endl;
    std::cout << std::endl;

    m_filelist.ExportPlaylist(output);
}

void ConvertFiles::ConvertFile(const Filelist::FSPath& input, const Filelist::FSPath& output)
{
    std::cout << "input:  " << input.string()  << std::endl;
    std::cout << "output: " << output.string() << std::endl;
    std::cout << std::endl;

    // TODO
}

void ConvertFiles::OnFrameDecoded(Stream& stream, FrameId frameId)
{
    // TODO
}

void ConvertFiles::OnFrameEncoded(Stream& stream, FrameId frameId)
{
    // TODO
}
