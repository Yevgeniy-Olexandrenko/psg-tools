#include "ConvertFiles.h"
#include "interface/ConsoleGUI.h"

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
    Stream stream;
    stream.dchip = m_chip;

    m_sHeight = 0;
    m_dHeight = 0;
    m_sPrint = true;

    if (Decode(input, stream))
    {
        m_sPrint = true;
        m_sHeight += m_dHeight;
        m_dHeight = 0;

        if (Encode(output, stream))
        {
            gui::Clear(m_dHeight);
        }
        else
        {
            std::cout << "Could not encode file: " << output << std::endl;
        }
    }
    else
    {
        std::cout << "Could not decode file: " << input << std::endl;
    }
}

void ConvertFiles::OnFrameDecoded(Stream& stream, FrameId frameId)
{
    terminal::cursor::move_up(int(m_dHeight));
    m_dHeight = 0;

    if (m_sPrint)
    {
        gui::Clear(m_sHeight);
        m_sHeight = 0;

        auto index = m_filelist.GetCurrFileIndex();
        auto amount = m_filelist.GetNumberOfFiles();

        m_sHeight += gui::PrintInputFile(stream, index, amount, false);
        m_sHeight += gui::PrintBriefStreamInfo(stream);
        m_sPrint = false;
    }
    m_dHeight += gui::PrintDecodingProgress(stream);
}

void ConvertFiles::OnFrameEncoded(Stream& stream, FrameId frameId)
{
    terminal::cursor::move_up(int(m_dHeight));
    m_dHeight = 0;

    if (m_sPrint)
    {
        gui::Clear(m_sHeight);
        m_sHeight = 0;

        auto index = m_filelist.GetCurrFileIndex();
        auto amount = m_filelist.GetNumberOfFiles();

        m_sHeight += gui::PrintInputFile(stream, index, amount, false);
        m_sHeight += gui::PrintFullStreamInfo(stream, stream.file.string());
        m_sPrint = false;
    }
    m_dHeight += gui::PrintEncodingProgress(stream, frameId);
}
