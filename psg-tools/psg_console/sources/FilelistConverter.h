#pragma once

#include "PSGAudio.h"
#include "ConsoleGUI.h"
#include "filelist/FilelistTraversal.h"

using Termination = std::atomic<bool>;

class FilelistConverter : public FileDecoder, public FileEncoder
{
public:
    FilelistConverter(Chip& chip, FilelistTraversal& filelist, Filelist::Path& output, Duration duration, Termination& termination);

    void Convert();

protected:
    void ExportPlaylist();
    void ConvertFile();
    gui::FileView GetFileView() const;

    void OnFrameDecoded(const Stream& stream, FrameId frameId) override;
    void OnFrameEncoded(const Stream& stream, FrameId frameId, size_t framesCount) override;
    bool IsAbortRequested() const override { return m_termination; }

private:
    Chip m_chip;
    FilelistTraversal& m_filelist;
    Filelist::Path& m_output;
    Duration m_duration;
    Termination& m_termination;

    Filelist::Path m_inputPath;
    Filelist::Path m_outputPath;
};