#include "FilelistPlayer.h"
#include "ConsoleGUI.h"
#include <chrono>

FilelistPlayer::FilelistPlayer(Chip& chip, Output& output, Filelist& filelist, Filelist& favorites, Termination& termination)
    : m_chip(chip)
    , m_output(output)
    , m_filelist(filelist)
    , m_favorites(favorites)
    , m_termination(termination)
    , m_gotoBackward(false)
    , m_player(output)
    , m_pause(false)
    , m_step(1.f)
    , m_hideStream(false)
{
    for (auto& enable : m_enables) enable = true;
}

FilelistPlayer::~FilelistPlayer()
{
    terminal::cursor::show(true);
}

void FilelistPlayer::Play()
{
    m_gotoBackward = false;
    auto result{ Action::GoToNextFile };

    BackgroundDecoder bgDecoder(m_chip);

    while (result == Action::GoToNextFile)
    {
        Filelist::FSPath path;
        std::shared_ptr<Stream> stream;

        if (!(m_gotoBackward ? m_filelist.GetPrevFile(path) : m_filelist.GetNextFile(path))) break;

        m_sHeight = 0;
        m_dHeight = 0;
        m_sPrint = true;
#if 1
        if (bgDecoder.IsReady(path))
        {
            stream = bgDecoder.GetStream();
            Sleep(1000);
        }
        else
#endif
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
            if (m_gotoBackward ? m_filelist.PeekPrevFile(path) : m_filelist.PeekNextFile(path))
            {
                bgDecoder.Decode(path);
            }

            m_gotoBackward = false;
            result = PlayStream(*stream);
        }
    }
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
#if 0
        if (m_dbgDecodeTime)
        {
            std::cout << " DECODE TIME: " << m_dbgDecodeTime << " ms\n";
            m_sHeight++;
        }
#endif
    }

    if (!m_hideStream)
    m_dHeight += gui::PrintStreamFrames(stream, frameId, m_output);
    m_dHeight += gui::PrintPlaybackProgress(stream, frameId);
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
            float rewindStep = 0.4f * stream.play.frameRate();
            if (m_step < 1.f) rewindStep = 1.f;

            if (gui::GetKeyState(VK_LEFT).held)
            {
                m_gotoBackward = true;
                m_player.Step(-rewindStep);
            }
            else if (gui::GetKeyState(VK_RIGHT).held)
            {
                m_gotoBackward = false;
                m_player.Step(+rewindStep);
            }
            else
            {
                m_player.Step(+m_step);
            }
        }

        if (gui::GetKeyState(VK_UP).pressed)
        {
            m_gotoBackward = true;
            return Action::GoToNextFile;
        }
        else if (gui::GetKeyState(VK_DOWN).pressed)
        {
            m_gotoBackward = false;
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

FilelistPlayer::Action FilelistPlayer::PlayStream(const Stream& stream)
{
    auto result{ Action::Nothing };

    m_sPrint = true;
    m_sHeight += m_dHeight;
    m_dHeight = 0;

    if (m_player.Init(stream))
    {
        m_step = 1.f;
        m_pause = false;
        m_player.Play();

        FrameId frameId = -1;
        while (result == Action::Nothing)
        {
            m_output.GetEnables() = m_enables;
            gui::Update();

            if (m_sPrint || m_player.GetFrameId() != frameId)
            {
                frameId = m_player.GetFrameId();
                OnFramePlaying(stream, frameId);
            }

            if (!m_player.IsPlaying())
            {
                result = Action::GoToNextFile;
            }
            else if (m_termination)
            {
                result = Action::Termination;
            }
            else
            {
                result = HandleUserInput(stream);
            }
            Sleep(1);
        }

        m_player.Stop();
        gui::Clear(m_dHeight);
    }
    else
    {
        std::cout << "Could not init player with module" << std::endl;
    }
    return result;
}

BackgroundDecoder::BackgroundDecoder(Chip& chip)
    : m_chip(chip)
    , m_abort(false)
{
}

BackgroundDecoder::~BackgroundDecoder()
{
    if (m_thread.joinable())
    {
        m_abort = true;
        m_thread.join();
    }
}

void BackgroundDecoder::Decode(const Filelist::FSPath& path)
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

bool BackgroundDecoder::IsReady(const Filelist::FSPath& path) const
{
    return (m_stream && m_stream->file == path);
}

std::shared_ptr<Stream> BackgroundDecoder::GetStream() const
{
    return m_stream;
}

bool BackgroundDecoder::IsAbortRequested() const
{
    return m_abort;
}
