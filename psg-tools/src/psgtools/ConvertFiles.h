#pragma once

#include "PSGLib.h"
#include "Filelist.h"

using Termination = std::atomic<bool>;

class ConvertFiles : public FileDecoder, public FileEncoder
{
public:
    ConvertFiles(Chip& chip, Filelist& filelist, Filelist::FSPath& output, Termination& termination);

    void Convert();

protected:
    void ExportPlaylist(const Filelist::FSPath& output);
    void ConvertFile(const Filelist::FSPath& input, const Filelist::FSPath& output);

    void OnFrameDecoded(Stream& stream, FrameId frameId) override;
    void OnFrameEncoded(Stream& stream, FrameId frameId) override;

private:
    Chip& m_chip;
    Filelist& m_filelist;
    Filelist::FSPath& m_output;
    Termination& m_termination;
};