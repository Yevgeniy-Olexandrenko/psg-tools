#include "ConvertFiles.h"

ConvertFiles::ConvertFiles(Chip& chip, Filelist& filelist, Termination& termination)
    : m_chip(chip)
    , m_filelist(filelist)
    , m_termination(termination)
{
}

void ConvertFiles::Convert()
{
}

void ConvertFiles::OnFrameDecoded(Stream& stream, FrameId frameId)
{
}

void ConvertFiles::OnFrameEncoded(Stream& stream, FrameId frameId)
{
}
