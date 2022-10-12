#include "PlayFiles.h"
#include "interface/ConsoleGUI.h"

PSGPlayer::PSGPlayer(Chip& chip, Output& output, Filelist& filelist, Filelist& favorites, Termination& termination)
    : m_chip(chip)
    , m_output(output)
    , m_filelist(filelist)
    , m_favorites(favorites)
    , m_termination(termination)
    , m_player(output)
{
    for (auto& enable : m_enables) enable = true;
}

void PSGPlayer::Play()
{
    PlayStreamResult result{ PlayStreamResult::GoToNext };

    while (true)
    {
        std::filesystem::path path;
        bool newFileAvailable = false;

        if (result == PlayStreamResult::GoToPrevious)
        {
            newFileAvailable = m_filelist.GetPrevFile(path);
        }
        else if (result == PlayStreamResult::GoToNext)
        {
            newFileAvailable = m_filelist.GetNextFile(path);
        }
        if (!newFileAvailable) break;

        Stream stream;
        stream.dchip = m_chip;

        m_sHeight = 0;
        m_dHeight = 0;
        m_sPrint = true;

        if (Decode(path, stream))
        {
            result = PlayStream(stream);
        }
        else
        {
            std::cout << "Could not decode file: " << path << std::endl;
        }
    }
}

void PSGPlayer::OnFrameDecoded(Stream& stream, FrameId frameId)
{
    PrintStreamDecoding(stream);
}

void PSGPlayer::PrintStreamDecoding(const Stream& stream)
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

void PSGPlayer::PrintStreamPlayback(const Stream& stream, FrameId frameId)
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
    }

    m_dHeight += gui::PrintStreamFrames(stream, frameId, m_enables);
    m_dHeight += gui::PrintPlaybackProgress(stream, frameId);
}

PSGPlayer::PlayStreamResult PSGPlayer::HandleUserInput(const Stream& stream)
{
    if (gui::GetKeyState(VK_UP).pressed)
    {
        if (!m_player.IsPaused())
        {
            return PlayStreamResult::GoToPrevious;
        }
    }

    if (gui::GetKeyState(VK_DOWN).pressed)
    {
        if (!m_player.IsPaused())
        {
            return PlayStreamResult::GoToNext;
        }
    }

    if (gui::GetKeyState(VK_RETURN).pressed)
    {
        if (m_player.IsPaused())
            m_player.Play();
        else
            m_player.Stop();
    }

    if (gui::GetKeyState('F').pressed)
    {
        if (m_favorites.ContainsFile(stream.file)
            ? m_favorites.EraseFile(stream.file)
            : m_favorites.InsertFile(stream.file))
        {
            m_favorites.ExportPlaylist(m_favorites.GetPath());
            m_sPrint = true;
        }
    }

    for (int key = '1'; key <= '5'; ++key)
    {
        if (gui::GetKeyState(key).pressed)
        {
            m_enables[key - '1'] ^= true;
        }
    }

    return PlayStreamResult::Nothing;
}

PSGPlayer::PlayStreamResult PSGPlayer::PlayStream(const Stream& stream)
{
    auto result{ PlayStreamResult::Nothing };

    m_sPrint = true;
    m_sHeight += m_dHeight;
    m_dHeight = 0;

    if (m_player.Init(stream))
    {
        m_player.Play();
        FrameId frameId = -1;

        while (result == PlayStreamResult::Nothing)
        {
            if (m_player.GetFrameId() != frameId)
            {
                frameId = m_player.GetFrameId();
                PrintStreamPlayback(stream, frameId);
            }

            m_output.SetEnables(m_enables);
            gui::Update();

            if (!m_player.IsPlaying())
                result = PlayStreamResult::GoToNext;
            else if (m_termination)
                result = PlayStreamResult::Termination;
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