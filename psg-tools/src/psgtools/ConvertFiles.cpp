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
    m_outputPath = std::filesystem::absolute(m_output);
    m_outputPath.remove_filename();

    if (Filelist::IsPlaylistPath(m_output))
    {
        // write input files as playlist
        m_outputPath.replace_filename(m_output.filename());
        ExportPlaylist();
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
            while (m_filelist.GetNextFile(m_inputPath))
            {
                std::string inputName = m_inputPath.stem().string();
                std::string outputName = m_output.stem().string();

                size_t star = outputName.find('*');
                if (star != std::string::npos)
                    outputName.replace(star, 1, inputName);

                m_outputPath.replace_filename(outputName);
                m_outputPath.replace_extension(outputType);

                std::filesystem::create_directories(m_outputPath.parent_path());
                ConvertFile();

                // single file case
                if (star == std::string::npos) break;
            }
        }
    }
}

void ConvertFiles::ExportPlaylist()
{
    std::cout << "output: " << m_outputPath.string() << std::endl;
    std::cout << std::endl;

    m_filelist.ExportPlaylist(m_outputPath);
}

void ConvertFiles::ConvertFile()
{
    Stream stream;
    stream.dchip = m_chip;

    m_sHeight = 0;
    m_dHeight = 0;
    m_sPrint = true;

    if (Decode(m_inputPath, stream))
    {
        m_sPrint = true;
        m_sHeight += m_dHeight;
        m_dHeight = 0;

        if (Encode(m_outputPath, stream))
        {
            gui::Clear(m_dHeight);
        }
        else
        {
            std::cout << "Could not encode file: " << m_outputPath << std::endl;
        }
    }
    else
    {
        std::cout << "Could not decode file: " << m_inputPath << std::endl;
    }
}

void ConvertFiles::OnFrameDecoded(const Stream& stream, FrameId frameId)
{
    terminal::cursor::move_up(int(m_dHeight));
    m_dHeight = 0;

    if (m_sPrint)
    {
        gui::Clear(m_sHeight);
        m_sHeight = 0;

        auto index = m_filelist.GetCurrFileIndex();
        auto amount = m_filelist.GetNumberOfFiles();

        m_sHeight += gui::PrintInputFile(m_inputPath, index, amount, false);
        m_sHeight += gui::PrintBriefStreamInfo(stream);
        m_sPrint = false;
    }
    m_dHeight += gui::PrintDecodingProgress(stream);
}

void ConvertFiles::OnFrameEncoded(const Stream& stream, FrameId frameId)
{
    terminal::cursor::move_up(int(m_dHeight));
    m_dHeight = 0;

    if (m_sPrint)
    {
        gui::Clear(m_sHeight);
        m_sHeight = 0;

        auto index = m_filelist.GetCurrFileIndex();
        auto amount = m_filelist.GetNumberOfFiles();

        m_sHeight += gui::PrintInputFile(m_inputPath, index, amount, false);
        m_sHeight += gui::PrintFullStreamInfo(stream, m_outputPath.string());
        m_sPrint = false;
    }
    m_dHeight += gui::PrintEncodingProgress(stream, frameId);
}
