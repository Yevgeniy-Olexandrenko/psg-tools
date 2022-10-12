#pragma once

#include "PSGLib.h"
#include "Filelist.h"

using Termination = std::atomic<bool>;

class PSGPlayer : public PSGHandler
{
    enum class PlayStreamResult { Nothing, GoToNext, GoToPrevious, Termination };

public:
    PSGPlayer(Chip& chip, Output& output, Filelist& filelist, Filelist& favorites, Termination& termination);
    void Play();

protected:
    void OnFrameDecoded(Stream& stream, FrameId frameId) override;

private:
    void PrintStreamDecoding(const Stream& stream);
    void PrintStreamPlayback(const Stream& stream, FrameId frameId);
    PlayStreamResult HandleUserInput(const Stream& stream);
    PlayStreamResult PlayStream(const Stream& stream);

private:
    Chip& m_chip;
    Output& m_output;
    Filelist& m_filelist;
    Filelist& m_favorites;
    Termination& m_termination;

    Player m_player;
    Output::Enables m_enables;

    bool m_sPrint;
    size_t m_sHeight;
    size_t m_dHeight;

    int m_dbgDecodeTime;
};
