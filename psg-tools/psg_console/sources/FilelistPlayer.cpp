#include "FilelistPlayer.h"
#include "ConsoleGUI.h"
#include <chrono>

class BackgroundDecoder : public FileDecoder
{
public:
    BackgroundDecoder(Chip& chip) 
        : m_chip(chip)
        , m_abort(false)
    {}

    ~BackgroundDecoder()
    {
        if (m_thread.joinable())
        {
            m_abort = true;
            m_thread.join();
        }
    }

    void Decode(const Filelist::Path& path)
    {
        if (m_thread.joinable())
        {
            m_abort = true;
            m_thread.join();
        }

        m_abort = false;
        m_stream.reset();

        m_thread = std::thread([&]
            {
                auto stream = new Stream();
                stream->dchip = m_chip;

                if (FileDecoder::Decode(path, *stream))
                    m_stream.reset(stream);
                else
                    delete stream;
            });
    }

    bool IsReady(const Filelist::Path& path) const
    {
        return (m_stream && m_stream->file == path);
    }

    std::shared_ptr<Stream> GetStream() const { return m_stream; }

protected:
    bool IsAbortRequested() const override { return m_abort; }

private:
    Chip& m_chip;
    std::thread m_thread;
    std::atomic<bool> m_abort;
    std::shared_ptr<Stream> m_stream;
};

FilelistPlayer::FilelistPlayer(Chip& chip, Output& output, FilelistTraversal& filelist, Filelist& favorites, Termination& termination)
    : m_chip(chip)
    , m_output(output)
    , m_filelist(filelist)
    , m_favorites(favorites)
    , m_termination(termination)
    , m_player(output)
    , m_enables{ true, true, true, true, true }
    , m_pause(false)
    , m_step(1.f)
    , m_hideStream(false)
{}

void FilelistPlayer::Play()
{
    auto action{ Action::GoToNextFile };
    BackgroundDecoder bgDecoder(m_chip);

    while (action != Action::Termination && !m_termination)
    {
        Filelist::Path path;
        std::shared_ptr<Stream> stream;

        bool available = false;
        if (action == Action::GoToPrevFile) available = m_filelist.GetPrevFile(path);
        if (action == Action::GoToNextFile) available = m_filelist.GetNextFile(path);
        if (!available) break;

        auto tpBefore = std::chrono::steady_clock::now();
        if (bgDecoder.IsReady(path))
        {
            stream = bgDecoder.GetStream();
        }
        else
        {
            stream.reset(new Stream());
            stream->dchip = m_chip;

            if (!FileDecoder::Decode(path, *stream))
            {
                stream.reset();
                gui::EraseFile();
                if (!m_termination) gui::Print("Could not decode file: \"" + path.string() + '"');
            }
        }

        if (stream)
        {
            available = false;
            if (action == Action::GoToPrevFile) available = m_filelist.PeekPrevFile(path);
            if (action == Action::GoToNextFile) available = m_filelist.PeekNextFile(path);
            if (available) bgDecoder.Decode(path);

            auto tpAfter = std::chrono::steady_clock::now();
            auto decodeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(tpAfter - tpBefore).count();
            if (decodeDuration < 1000) 
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 - decodeDuration));

            action = PlayStream(*stream);
        }
    }
}

FilelistPlayer::Action FilelistPlayer::PlayStream(const Stream& stream)
{
    auto action{ Action::Nothing };

    if (m_player.Init(stream))
    {
        m_step = 1.f;
        m_pause = false;
        m_player.Play();

        FrameId frameId = -1;
        auto drawTime = std::chrono::steady_clock::now();
        while (action == Action::Nothing)
        {
            m_output.GetEnables() = m_enables;
            gui::Update();

            // redraw every new frame, and from time to time while the playback
            // is paused, to show changes of settings and the window size
            auto now = std::chrono::steady_clock::now();
            if (m_player.GetFrameId() != frameId || now - drawTime >= std::chrono::milliseconds(40))
            {
                frameId = m_player.GetFrameId();
                drawTime = now;
                gui::DrawPlayback(GetFileView(stream), stream, frameId, m_output, !m_hideStream);
            }

            if (m_termination)
            {
                action = Action::Termination;
            }
            else
            {
                action = HandleUserInput(stream);
                if (action == Action::Nothing && !m_player.IsPlaying())
                {
                    action = Action::GoToNextFile;
                }
            }

            // TODO
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        m_player.Stop();
        gui::CommitFile(GetFileView(stream), stream, m_output.toString());
    }
    else
    {
        action = Action::GoToNextFile;
        gui::EraseFile();
        gui::Print("Could not init player with module");
    }
    return action;
}

FilelistPlayer::Action FilelistPlayer::HandleUserInput(const Stream& stream)
{
    // playback paused mode
    if (m_pause)
    {
        if (gui::GetKeyState(VK_SPACE).pressed)
        {
            m_pause = false;
            m_player.Play();
        }
        else
        {
            if (gui::GetKeyState(VK_LEFT).held)
            {
                m_player.Step(-m_step);
                m_player.Play();
            }
            else if (gui::GetKeyState(VK_RIGHT).held)
            {
                m_player.Step(+m_step);
                m_player.Play();
            }
            else if (gui::GetKeyState(VK_RETURN).held)
            {
                m_player.Step(0);
                m_player.Play();
            }
            else
            {
                m_player.Stop();
            }
        }
    }

    // playback active mode
    else
    {
        if (gui::GetKeyState(VK_SPACE).pressed)
        {
            m_pause = true;
            m_player.Stop();
        }
        else
        {
            float rewindStep = 0.4f * stream.play.frameRate;
            if (m_step < 1.f) rewindStep = 1.f;

            if (gui::GetKeyState(VK_LEFT).held)
            {
                if (m_player.IsPlaying())
                    m_player.Step(-rewindStep);
                else
                    return Action::GoToPrevFile;
            }
            else if (gui::GetKeyState(VK_RIGHT).held)
            {
                if (m_player.IsPlaying())
                    m_player.Step(+rewindStep);
                else
                    return Action::GoToNextFile;
            }
            else
            {
                m_player.Step(+m_step);
            }
        }

        if (gui::GetKeyState(VK_UP).pressed)
        {
            return Action::GoToPrevFile;
        }
        else if (gui::GetKeyState(VK_DOWN).pressed)
        {
            return Action::GoToNextFile;
        }
    }

    if (gui::GetKeyState('A').pressed)
    {
        if (m_step > 0.0625f) m_step *= 0.5f;
    }
    else if (gui::GetKeyState('Q').pressed)
    {
        if (m_step < 1.0) m_step *= 2.0f;
        else m_step = 1.f;
    }

    if (gui::GetKeyState('F').pressed)
    {
        if (m_favorites.ContainsFile(stream.file)
            ? m_favorites.EraseFile(stream.file)
            : m_favorites.InsertFile(stream.file))
        {
            m_favorites.ExportPlaylist();
        }
    }

    for (int key = '1'; key <= '5'; ++key)
    {
        if (gui::GetKeyState(key).pressed)
        {
            m_enables[key - '1'] ^= true;
        }
    }

    if (gui::GetKeyState('H').pressed)
    {
        m_hideStream ^= true;
    }

    return Action::Nothing;
}

gui::FileView FilelistPlayer::GetFileView(const Stream& stream) const
{
    return
    {
        stream.file,
        m_filelist.GetCurrFileIndex(),
        int(m_filelist.GetNumberOfFiles()),
        m_favorites.ContainsFile(stream.file)
    };
}

void FilelistPlayer::OnFrameDecoded(const Stream& stream, FrameId frameId)
{
    gui::DrawDecoding(GetFileView(stream), stream);
}
