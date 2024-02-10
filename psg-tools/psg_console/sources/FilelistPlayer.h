#pragma once

#include "PSGAudio.h"
#include "Filelist.h"

using Termination = std::atomic<bool>;

class BackgroundDecoder : public FileDecoder
{
public:
    BackgroundDecoder(Chip& chip);
    ~BackgroundDecoder();

    void Decode(const Filelist::FSPath& path);
    bool IsReady(const Filelist::FSPath& path) const;

    std::shared_ptr<Stream> GetStream() const;

protected:
    bool IsAbortRequested() const override;

private:
    Chip& m_chip;
    std::thread m_thread;
    std::atomic<bool> m_abort;
    std::shared_ptr<Stream> m_stream;
};

class FilelistPlayer : public FileDecoder
{
    enum class Action { Nothing, GoToNextFile, GoToPrevFile, Termination };

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

    Player m_player;
    Output::Enables m_enables;
    bool  m_pause;
    float m_step;

    bool m_sPrint;
    size_t m_sHeight;
    size_t m_dHeight;
    bool m_hideStream;
};
