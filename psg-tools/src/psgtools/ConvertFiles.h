#pragma once

#include "PSGLib.h"
#include "Filelist.h"

using Termination = std::atomic<bool>;

class ConvertFiles : public PSGHandler
{
public:
    ConvertFiles(Chip& chip, Filelist& filelist, Termination& termination);
    void Convert();

protected:
    void OnFrameDecoded(Stream& stream, FrameId frameId) override;
    void OnFrameEncoded(Stream& stream, FrameId frameId) override;

private:
    Chip& m_chip;
    Filelist& m_filelist;
    Termination& m_termination;
};