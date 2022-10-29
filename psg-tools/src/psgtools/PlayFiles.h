#pragma once

#include "PSGLib.h"
#include "Filelist.h"

using Termination = std::atomic<bool>;

class PlayFiles : public FileDecoder
{
    enum class Action { Nothing, GoToNextFile, Termination };

public:
    PlayFiles(Chip& chip, Output& output, Filelist& filelist, Filelist& favorites, Termination& termination);
    ~PlayFiles();

    void Play();

protected:
    void OnFrameDecoded(const Stream& stream, FrameId frameId) override;
    void OnFramePlaying(const Stream& stream, FrameId frameId);

private:
    Action HandleUserInput(const Stream& stream);
    Action PlayStream(const Stream& stream);

private:
    Chip& m_chip;
    Output& m_output;
    Filelist& m_filelist;
    Filelist& m_favorites;
    Termination& m_termination;
    bool m_gotoBackward;

    Player m_player;
    Output::Enables m_enables;
    bool  m_pause;
    float m_step;

    bool m_sPrint;
    size_t m_sHeight;
    size_t m_dHeight;
    bool m_hideStream;

    int m_dbgDecodeTime;
};
