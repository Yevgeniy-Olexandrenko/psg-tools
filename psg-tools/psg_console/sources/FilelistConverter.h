#pragma once

#include "PSGAudio.h"
#include "filelist/FilelistTraversal.h"

using Termination = std::atomic<bool>;

class FilelistConverter : public FileDecoder, public FileEncoder
{
public:
    FilelistConverter(Chip& chip, FilelistTraversal& filelist, Filelist::Path& output, Termination& termination);

    void Convert();

protected:
    void ExportPlaylist();
    void ConvertFile();

    void OnFrameDecoded(const Stream& stream, FrameId frameId) override;
    void OnFrameEncoded(const Stream& stream, FrameId frameId) override;

private:
    Chip m_chip;
    FilelistTraversal& m_filelist;
    Filelist::Path& m_output;
    Termination& m_termination;

    Filelist::Path m_inputPath;
    Filelist::Path m_outputPath;

    bool m_sPrint;
    size_t m_sHeight;
    size_t m_dHeight;
};