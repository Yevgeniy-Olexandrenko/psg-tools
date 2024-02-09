#pragma once

#include "PSGAudio.h"
#include "Filelist.h"

using Termination = std::atomic<bool>;

class FilelistConverter : public FileDecoder, public FileEncoder
{
public:
    FilelistConverter(Chip& chip, Filelist& filelist, Filelist::FSPath& output, Termination& termination);

    void Convert();

protected:
    void ExportPlaylist();
    void ConvertFile();

    void OnFrameDecoded(const Stream& stream, FrameId frameId) override;
    void OnFrameEncoded(const Stream& stream, FrameId frameId) override;

private:
    Chip& m_chip;
    Filelist& m_filelist;
    Filelist::FSPath& m_output;
    Termination& m_termination;

    Filelist::FSPath m_inputPath;
    Filelist::FSPath m_outputPath;

    bool m_sPrint;
    size_t m_sHeight;
    size_t m_dHeight;
};