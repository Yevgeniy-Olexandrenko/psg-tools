#pragma once

#include "PSGAudio.h"
#include "filelist/FilelistTraversal.h"

using Termination = std::atomic<bool>;

class FilelistPlayer : public FileDecoder
{
    enum class Action { Nothing, GoToNextFile, GoToPrevFile, Termination };

public:
    FilelistPlayer(Chip& chip, Output& output, FilelistTraversal& filelist, Filelist& favorites, Termination& termination);
    ~FilelistPlayer();

    void Play();

private:
    Action PlayStream(const Stream& stream);
    Action HandleUserInput(const Stream& stream);
    
protected:
    void OnFrameDecoded(const Stream& stream, FrameId frameId) override;
    void OnFramePlaying(const Stream& stream, FrameId frameId);


private:
    Chip& m_chip;
    Output& m_output;
    FilelistTraversal& m_filelist;
    Filelist& m_favorites;
    Termination& m_termination;

    Player m_player;
    Output::Enables m_enables;
    bool  m_pause;
    float m_step;

    bool m_sPrint;
    size_t m_sHeight;
    size_t m_dHeight;
    bool m_hideStream;
};
