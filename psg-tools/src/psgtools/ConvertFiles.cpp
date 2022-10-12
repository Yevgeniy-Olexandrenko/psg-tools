#include "ConvertFiles.h"

PSGConverter::PSGConverter(Chip& chip, Filelist& filelist, Termination& termination)
    : m_chip(chip)
    , m_filelist(filelist)
    , m_termination(termination)
{
}

void PSGConverter::Convert()
{
}

void PSGConverter::OnFrameDecoded(Stream& stream, FrameId frameId)
{
}

void PSGConverter::OnFrameEncoded(Stream& stream, FrameId frameId)
{
}
