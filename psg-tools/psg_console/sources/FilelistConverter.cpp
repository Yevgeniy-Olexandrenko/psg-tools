#include "FilelistConverter.h"
#include "ConsoleGUI.h"

FilelistConverter::FilelistConverter(Chip& chip, FilelistTraversal& filelist, Filelist::Path& output, Termination& termination)
    : m_chip(chip)
    , m_filelist(filelist)
    , m_output(output)
    , m_termination(termination)
{
}

void FilelistConverter::Convert()
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

void FilelistConverter::ExportPlaylist()
{
    gui::Print("output: " + m_outputPath.string());
    gui::Print("");

    m_filelist.ExportPlaylist(m_outputPath);
}

void FilelistConverter::ConvertFile()
{
    Stream stream;
    stream.dchip = m_chip;

    if (Decode(m_inputPath, stream))
    {
        if (Encode(m_outputPath, stream))
        {
            gui::CommitFile(GetFileView(), stream, m_outputPath.string());
        }
        else
        {
            gui::EraseFile();
            gui::Print("Could not encode file: \"" + m_outputPath.string() + '"');
        }
    }
    else
    {
        gui::EraseFile();
        gui::Print("Could not decode file: \"" + m_inputPath.string() + '"');
    }
}

gui::FileView FilelistConverter::GetFileView() const
{
    return { m_inputPath, m_filelist.GetCurrFileIndex(), int(m_filelist.GetNumberOfFiles()), false };
}

void FilelistConverter::OnFrameDecoded(const Stream& stream, FrameId frameId)
{
    gui::DrawDecoding(GetFileView(), stream);
}

void FilelistConverter::OnFrameEncoded(const Stream& stream, FrameId frameId)
{
    gui::DrawEncoding(GetFileView(), stream, frameId, m_outputPath.string());
}
