#pragma once

#include "PSGAudio.h"
#include "Filelist.h"

using Termination = std::atomic<bool>;

class FilelistPlayer : public FileDecoder
{
    enum class Action { Nothing, GoToNextFile, Termination };

public:
    FilelistPlayer(Chip& chip, Output& output, Filelist& filelist, Filelist& favorites, Termination& termination);
    ~FilelistPlayer();

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
