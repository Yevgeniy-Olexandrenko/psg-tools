#pragma once

#include "PSGAudio.h"
#include "ConsoleGUI.h"
#include "filelist/FilelistTraversal.h"

using Termination = std::atomic<bool>;

class FilelistPlayer : public FileDecoder
{
    enum class Action { Nothing, GoToNextFile, GoToPrevFile, Termination };

public:
    FilelistPlayer(Chip& chip, Output& output, FilelistTraversal& filelist, Filelist& favorites, Termination& termination);

    void Play();

private:
    Action PlayStream(const Stream& stream);
    Action HandleUserInput(const Stream& stream);
    gui::FileView GetFileView(const Stream& stream) const;

protected:
    void OnFrameDecoded(const Stream& stream, FrameId frameId) override;
    bool IsAbortRequested() const override { return m_termination; }


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
    bool  m_hideStream;
};
