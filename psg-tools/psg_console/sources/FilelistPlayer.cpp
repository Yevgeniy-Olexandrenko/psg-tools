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
    , m_sPrint(false)
    , m_sHeight(0)
    , m_dHeight(0)
    , m_hideStream(false)
{}

FilelistPlayer::~FilelistPlayer()
{
    terminal::cursor::show(true);
}

void FilelistPlayer::Play()
{
    auto action{ Action::GoToNextFile };
    BackgroundDecoder bgDecoder(m_chip);

    while (action != Action::Termination)
    {
        Filelist::Path path;
        std::shared_ptr<Stream> stream;

        bool available = false;
        if (action == Action::GoToPrevFile) available = m_filelist.GetPrevFile(path);
        if (action == Action::GoToNextFile) available = m_filelist.GetNextFile(path);
        if (!available) break;

        m_sPrint = true;
        m_sHeight = m_dHeight = 0;
        
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
                std::cout << "Could not decode file: " << path << std::endl;
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

    m_sPrint = true;
    m_sHeight += m_dHeight;
    m_dHeight = 0;

    if (m_player.Init(stream))
    {
        m_step = 1.f;
        m_pause = false;
        m_player.Play();

        FrameId frameId = -1;
        while (action == Action::Nothing)
        {
            m_output.GetEnables() = m_enables;
            gui::Update();

            if (m_sPrint || m_player.GetFrameId() != frameId)
            {
                frameId = m_player.GetFrameId();
                OnFramePlaying(stream, frameId);
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
        gui::Clear(m_dHeight);
    }
    else
    {
        action = Action::GoToNextFile;
        std::cout << "Could not init player with module" << std::endl;
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
            m_sPrint = true;
        }
    }

    for (int key = '1'; key <= '5'; ++key)
    {
        if (gui::GetKeyState(key).pressed)
        {
            m_enables[key - '1'] ^= true;
            m_sPrint = true;
        }
    }

    if (gui::GetKeyState('H').pressed)
    {
        m_hideStream ^= true;
        gui::Clear(m_dHeight);
        m_dHeight = 0;
    }

    return Action::Nothing;
}

void FilelistPlayer::OnFrameDecoded(const Stream& stream, FrameId frameId)
{
    terminal::cursor::move_up(int(m_dHeight));
    m_dHeight = 0;

    if (m_sPrint)
    {
        gui::Clear(m_sHeight);
        m_sHeight = 0;

        auto index = m_filelist.GetCurrFileIndex();
        auto amount = m_filelist.GetNumberOfFiles();
        auto favorite = m_favorites.ContainsFile(stream.file);

        m_sHeight += gui::PrintInputFile(stream.file, index, amount, favorite);
        m_sHeight += gui::PrintBriefStreamInfo(stream);
        m_sPrint = false;
    }

    m_dHeight += gui::PrintDecodingProgress(stream);
}

void FilelistPlayer::OnFramePlaying(const Stream& stream, FrameId frameId)
{
    terminal::cursor::move_up(int(m_dHeight));
    m_dHeight = 0;

    if (m_sPrint)
    {
        gui::Clear(m_sHeight);
        m_sHeight = 0;

        auto index = m_filelist.GetCurrFileIndex();
        auto amount = m_filelist.GetNumberOfFiles();
        auto favorite = m_favorites.ContainsFile(stream.file);

        m_sHeight += gui::PrintInputFile(stream.file, index, amount, favorite);
        m_sHeight += gui::PrintFullStreamInfo(stream, m_output.toString());
        m_sPrint = false;
    }

    if (!m_hideStream)
    m_dHeight += gui::PrintStreamFrames(stream, frameId, m_output);
    m_dHeight += gui::PrintPlaybackProgress(stream, frameId);
}

