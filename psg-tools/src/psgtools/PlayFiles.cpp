#include "PlayFiles.h"
#include "interface/ConsoleGUI.h"
#include <chrono>

PlayFiles::PlayFiles(Chip& chip, Output& output, Filelist& filelist, Filelist& favorites, Termination& termination)
    : m_chip(chip)
    , m_output(output)
    , m_filelist(filelist)
    , m_favorites(favorites)
    , m_termination(termination)
    , m_player(output)
    , m_pause(false)
    , m_step(1.f)
{
    for (auto& enable : m_enables) enable = true;
}

void PlayFiles::Play()
{
    Action result{ Action::GoToNext };

    while (true)
    {
        std::filesystem::path path;
        bool newFileAvailable = false;

        if (result == Action::GoToPrevious)
        {
            newFileAvailable = m_filelist.GetPrevFile(path);
        }
        else if (result == Action::GoToNext)
        {
            newFileAvailable = m_filelist.GetNextFile(path);
        }
        if (!newFileAvailable) break;

        Stream stream;
        stream.dchip = m_chip;

        m_sHeight = 0;
        m_dHeight = 0;
        m_sPrint = true;

        auto t1 = std::chrono::high_resolution_clock::now();
        if (Decode(path, stream))
        {
            auto t2 = std::chrono::high_resolution_clock::now();
            m_dbgDecodeTime = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

            result = PlayStream(stream);
        }
        else
        {
            std::cout << "Could not decode file: " << path << std::endl;
        }
    }
}

void PlayFiles::OnFrameDecoded(Stream& stream, FrameId frameId)
{
    PrintStreamDecoding(stream);
}

void PlayFiles::PrintStreamDecoding(const Stream& stream)
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

        m_sHeight += gui::PrintInputFile(stream, index, amount, favorite);
        m_sHeight += gui::PrintBriefStreamInfo(stream);
        m_sPrint = false;
    }

    m_dHeight += gui::PrintDecodingProgress(stream);
}

void PlayFiles::PrintStreamPlayback(const Stream& stream, FrameId frameId)
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

        m_sHeight += gui::PrintInputFile(stream, index, amount, favorite);
        m_sHeight += gui::PrintFullStreamInfo(stream, m_output);
        m_sPrint = false;
#if 0
        if (m_dbgDecodeTime)
        {
            std::cout << " DECODE TIME: " << m_dbgDecodeTime << " ms\n";
            m_sHeight++;
        }
#endif
    }

    m_dHeight += gui::PrintStreamFrames(stream, frameId, m_enables);
    m_dHeight += gui::PrintPlaybackProgress(stream, frameId);
}

PlayFiles::Action PlayFiles::HandleUserInput(const Stream& stream)
{
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
            else if (gui::GetKeyState(VK_LSHIFT).held)
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
                m_player.Step(-rewindStep);
            }
            else if (gui::GetKeyState(VK_RIGHT).held)
            {
                m_player.Step(+rewindStep);
            }
            else
            {
                m_player.Step(+m_step);
            }
        }

        if (gui::GetKeyState(VK_UP).pressed)
        {
            return Action::GoToPrevious;
        }
        else if (gui::GetKeyState(VK_DOWN).pressed)
        {
            return Action::GoToNext;
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

    return Action::Nothing;
}

PlayFiles::Action PlayFiles::PlayStream(const Stream& stream)
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
            m_output.SetEnables(m_enables);
            gui::Update();

            if (m_sPrint || m_player.GetFrameId() != frameId)
            {
                frameId = m_player.GetFrameId();
                PrintStreamPlayback(stream, frameId);
            }

            if (!m_player.IsPlaying())
                result = Action::GoToNext;
            else if (m_termination)
                result = Action::Termination;
            else
                result = HandleUserInput(stream);

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