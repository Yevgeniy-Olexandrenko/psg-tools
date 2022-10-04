#include <iostream>
#include <chrono>
#include <Windows.h>
#include <sstream>
#include <fstream>
#include <array>

#undef max

#include "PSGTools.h"
#include "interface/ConsoleGUI.h"

////////////////////////////////////////////////////////////////////////////////

#if 0
struct FrameRefs
{
    const Frame* prev = nullptr;
    const Frame* next = nullptr;
};

std::vector<FrameRefs> m_framesRefs;

bool IsSameFrames(const Frame& f1, const Frame& f2)
{
    for (int chip = 0; chip < 2; ++chip)
    {
        for (Register reg = BankA_Fst; reg <= BankA_Lst; ++reg)
        {
            if (f1[chip].Read(reg) != f2[chip].Read(reg)) return false;
        }

        for (Register reg = BankB_Fst; reg <= BankB_Lst; ++reg)
        {
            if (f1[chip].Read(reg) != f2[chip].Read(reg)) return false;
        }
    }
    return true;
}

void ComputeFrameRefs(const Stream& stream)
{
    int count = stream.framesCount();
    int depth = count;// 32;

    m_framesRefs.resize(count);
    for (int frameId = 1; frameId < count; ++frameId)
    {
        for (int otherId = frameId - 1, lastId = std::max(frameId - depth, 0); otherId >= lastId; --otherId)
        {
            const Frame& frame = stream.GetFrame(frameId);
            const Frame& other = stream.GetFrame(otherId);

            if (IsSameFrames(frame, other))
            {
                m_framesRefs[frameId].prev = &other;
                m_framesRefs[otherId].next = &frame;
                break;
            }
        }
    }

    std::ofstream debug_out;
    debug_out.open("test.txt");
    if (debug_out)
    {
        int unique = 0;
        int count = m_framesRefs.size();

        for (int frameId = 1; frameId < count; ++frameId)
        {
            if (m_framesRefs[frameId].next)
                debug_out << std::setw(5) << std::setfill('0') << int(m_framesRefs[frameId].next->GetId());
            else
                debug_out << "-----";

            debug_out << " -> " << std::setw(5) << std::setfill('0') << frameId << " -> ";

            if (m_framesRefs[frameId].prev)
                debug_out << std::setw(5) << std::setfill('0') << int(m_framesRefs[frameId].prev->GetId());
            else
                debug_out << "-----";
            debug_out << "\n";

            if (!m_framesRefs[frameId].prev) unique++;
        }
        debug_out << "duplicates: " << int(100.f * (count - unique) / count + 0.5f) << "%\n";
        debug_out.close();
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////

void PrintWellcome()
{
    gui::Init(L"PSG Tools");

    using namespace terminal;
    std::cout << ' ' << color::bright_blue << std::string(gui::k_consoleWidth - 2, '-') << std::endl;
    std::cout << ' ' << color::bright_red << "PSG Tools v1.0" << std::endl;
    std::cout << ' ' << color::bright_red << "by Yevgeniy Olexandrenko" << std::endl;
    std::cout << ' ' << color::bright_blue << std::string(gui::k_consoleWidth - 2, '-') << std::endl;
    std::cout << color::reset << std::endl;
    std::cout << " ENTER\t- Pause/Resume current song playback" << std::endl;
    std::cout << " DOWN\t- Go to next song in playlist" << std::endl;
    std::cout << " UP\t- Go to previous song in playlist" << std::endl;
    std::cout << " F\t- Add/Remove current song to/from favorites" << std::endl;
    std::cout << color::reset << std::endl;
}

//void PlayInputFiles()
//{
//    ////////////////////////////////////////////////////////////////////////////
//#if 0
//    const int k_comPortIndex = 4;
//    m_output.reset(new Streamer(k_comPortIndex));
//#else
//    m_output.reset(new Emulator());
//#endif
//    m_player.reset(new Player(*m_output));
//#if 0
//    m_filelist->RandomShuffle();
//#endif
//    ////////////////////////////////////////////////////////////////////////////
//
//    bool goToPrev = false;
//    std::filesystem::path path;
//    for (auto& enable : m_enables) enable = true;
//
//    while (goToPrev ? m_filelist->GetPrevFile(path) : m_filelist->GetNextFile(path))
//    {
//        Stream stream;
//#if 1
//        stream.dchip.first.model(Chip::Model::AY8930);
//        //stream.dchip.second.model(Chip::Model::AY8910);
//        //stream.dchip.output(Chip::Output::Stereo);
//        //stream.dchip.stereo(Chip::Stereo::CAB);
//        //stream.dchip.clockValue(1000000);
//#endif
//        if (PSG::Decode(path, stream))
//        {
//            //ComputeFrameRefs(stream);
//
//
//            goToPrev = false; // if decoding OK, move to next by default
//
//            size_t staticHeight  = 0;
//            size_t dynamicHeight = 0;
//
//            if (m_player->Init(stream))
//            {
//                bool printStatic = true;
//                FrameId frameId  = -1;
//
//                m_player->Play();
//                while (m_player->IsPlaying())
//                {
//                    m_output->SetEnables(m_enables);
//                    gui::Update();
//
//                    if (m_player->GetFrameId() != frameId)
//                    {
//                        frameId = m_player->GetFrameId();
//
//                        terminal::cursor::move_up(dynamicHeight);
//                        dynamicHeight = 0;
//
//                        if (printStatic)
//                        {
//                            gui::Clear(staticHeight);
//                            staticHeight = 0;
//
//                            auto index = m_filelist->GetCurrFileIndex();
//                            auto amount = m_filelist->GetNumberOfFiles();
//                            auto favorite = m_favorites->ContainsFile(stream.file);
//
//                            staticHeight += gui::PrintInputFile(stream, index, amount, favorite);
//                            staticHeight += gui::PrintStreamInfo(stream, *m_output);
//                            printStatic = false;
//                        }
//
//                        dynamicHeight += gui::PrintStreamFrames(stream, frameId, m_enables);
//                        dynamicHeight += gui::PrintPlaybackProgress(stream, frameId);
//                    }
//
//                    if (gui::GetKeyState(VK_UP).pressed)
//                    {
//                        if (!m_player->IsPaused())
//                        {
//                            m_player->Stop();
//                            goToPrev = true;
//                            break;
//                        }
//                    }
//
//                    if (gui::GetKeyState(VK_DOWN).pressed)
//                    {
//                        if (!m_player->IsPaused())
//                        {
//                            m_player->Stop();
//                            goToPrev = false;
//                            break;
//                        }
//                    }
//
//                    if (gui::GetKeyState(VK_RETURN).pressed)
//                    {
//                        if (m_player->IsPaused())
//                            m_player->Play();
//                        else
//                            m_player->Stop();
//                    }
//
//                    if (gui::GetKeyState('F').pressed)
//                    {
//                        if (m_favorites->ContainsFile (stream.file)
//                            ? m_favorites->EraseFile  (stream.file)
//                            : m_favorites->InsertFile (stream.file))
//                        {
//                            m_favorites->ExportPlaylist(k_favoritesPath);
//                            printStatic = true;   
//                        }
//                    }
//
//                    for (int key = '1'; key <= '5'; ++key)
//                    {
//                        if (gui::GetKeyState(key).pressed)
//                        {
//                            m_enables[key - '1'] ^= true;
//                        }
//                    }
//
//                    Sleep(1);
//                }
//
//                gui::Clear(dynamicHeight);
//            }
//            else
//            {
//                std::cout << "Could not init player with module" << std::endl;
//            }
//        }
//        else
//        {
//            std::cout << "Could not decode file: " << path << std::endl;
//        }
//        path.clear();
//    }
//}
//
//void ConvertInputFiles(const std::filesystem::path& outputPath)
//{
//    std::filesystem::path path;
//    while (m_filelist->GetNextFile(path))
//    {
//        Stream stream;
//#if 0
//        stream.chip.first.model(Chip::Model::AY8930);
//        //stream.chip.second.model(Chip::Model::YM2149);
//        //stream.chip.output(Chip::Output::Stereo);
//        //stream.chip.clockValue(1500000);
//#endif
//        if (PSG::Decode(path, stream))
//        {
//            if (PSG::Encode(outputPath, stream))
//            {
//                std::cout << "Done file encoding: " << outputPath << std::endl;
//            }
//            else
//            {
//                std::cout << "Could not encode file: " << outputPath << std::endl;
//            }
//        }
//        else
//        {
//            std::cout << "Could not decode file: " << path << std::endl;
//        }
//    }
//}

////////////////////////////////////////////////////////////////////////////////

static const std::filesystem::path c_favoritesPath = "favorites.m3u";
static Termination m_termination = false;

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    m_termination = true;
    Sleep(10000);
    return TRUE;
}

int main(int argc, char* argv[])
{
    

    bool isConverting = false;
    bool isRandomShuffle = false;
    bool isStreamer = false;

    std::filesystem::path inputPath;
    std::filesystem::path outputPath;

    // setup input path
    if (argc > 1)
        inputPath = argv[1];
    else
        inputPath = c_favoritesPath;
    
    // setup output path
    if (argc > 2)
    {
        outputPath = argv[2];
        isConverting = true;
    }

    // setup file lists
    Filelist filelist(PSGHandler::DecodeFileTypes, inputPath);
    Filelist favorites(PSGHandler::DecodeFileTypes, c_favoritesPath);

    // setup destination chip config
    Chip chip;
#if 0
    chip.first.model(Chip::Model::AY8930);
    chip.second.model(Chip::Model::AY8910);
    chip.output(Chip::Output::Stereo);
    chip.stereo(Chip::Stereo::CAB);
    chip.clockValue(1000000);
#endif

    PrintWellcome();
    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    if (!filelist.IsEmpty())
    {
        if (isConverting)
        {
            //ConvertInputFiles(outputPath);
        }
        else
        {
            // setup output device
            std::shared_ptr<Output> output;
            if (isStreamer)
            {
                const int k_comPortIndex = 4;
                output.reset(new Streamer(k_comPortIndex));
            }
            else
            {
                output.reset(new Emulator());
            }

            // setup and start playback
            if (isRandomShuffle) filelist.RandomShuffle();
            PSGPlayer psgPlayer(chip, *output, filelist, favorites, m_termination);
            psgPlayer.Play();
        }
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

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
            ? m_favorites.EraseFile (stream.file)
            : m_favorites.InsertFile(stream.file))
        {
            m_favorites.ExportPlaylist(c_favoritesPath);
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

////////////////////////////////////////////////////////////////////////////////

PSGConverter::PSGConverter(Chip& chip, Filelist& filelist, Termination& termination)
    : m_chip(chip)
    , m_filelist(filelist)
    , m_termination(termination)
{
}

void PSGConverter::Convert()
{
}

void PSGConverter::OnFrameDecoded(Stream& stream, FrameId frameId)
{
}

void PSGConverter::OnFrameEncoded(Stream& stream, FrameId frameId)
{
}

////////////////////////////////////////////////////////////////////////////////