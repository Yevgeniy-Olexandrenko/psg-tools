﻿#include <iostream>
#include <chrono>
#include <Windows.h>
#include <sstream>
#include <fstream>
#include <array>

#undef max

#include "PSGLib.h"
#include "Filelist.h"
#include "interface/ConsoleGUI.h"

const std::filesystem::path k_favoritesPath = "favorites.m3u";
const std::string k_supportedFileTypes = "sqt|ym|stp|vgz|vgm|asc|stc|pt2|pt3|psg|vtx";

//std::shared_ptr<Filelist> m_filelist;
//std::shared_ptr<Filelist> m_favorites;

//std::shared_ptr<Output>   m_output;
//std::shared_ptr<Player>   m_player;

//Output::Enables m_enables;

static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType)
{
    if (m_player)
    {
        m_player->Stop();
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

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

void PlayInputFiles()
{
    ////////////////////////////////////////////////////////////////////////////
#if 0
    const int k_comPortIndex = 4;
    m_output.reset(new Streamer(k_comPortIndex));
#else
    m_output.reset(new Emulator());
#endif
    m_player.reset(new Player(*m_output));
#if 0
    m_filelist->RandomShuffle();
#endif
    ////////////////////////////////////////////////////////////////////////////

    bool goToPrev = false;
    std::filesystem::path path;
    for (auto& enable : m_enables) enable = true;

    while (goToPrev ? m_filelist->GetPrevFile(path) : m_filelist->GetNextFile(path))
    {
        Stream stream;
#if 1
        stream.dchip.first.model(Chip::Model::AY8930);
        //stream.dchip.second.model(Chip::Model::AY8910);
        //stream.dchip.output(Chip::Output::Stereo);
        //stream.dchip.stereo(Chip::Stereo::CAB);
        //stream.dchip.clockValue(1000000);
#endif
        if (PSG::Decode(path, stream))
        {
            //ComputeFrameRefs(stream);


            goToPrev = false; // if decoding OK, move to next by default

            size_t staticHeight  = 0;
            size_t dynamicHeight = 0;

            if (m_player->Init(stream))
            {
                bool printStatic = true;
                FrameId frameId  = -1;

                m_player->Play();
                while (m_player->IsPlaying())
                {
                    m_output->SetEnables(m_enables);
                    gui::Update();

                    if (m_player->GetFrameId() != frameId)
                    {
                        frameId = m_player->GetFrameId();

                        terminal::cursor::move_up(dynamicHeight);
                        dynamicHeight = 0;

                        if (printStatic)
                        {
                            gui::Clear(staticHeight);
                            staticHeight = 0;

                            auto index = m_filelist->GetCurrFileIndex();
                            auto amount = m_filelist->GetNumberOfFiles();
                            auto favorite = m_favorites->ContainsFile(stream.file);

                            staticHeight += gui::PrintInputFile(stream, index, amount, favorite);
                            staticHeight += gui::PrintStreamInfo(stream, *m_output);
                            printStatic = false;
                        }

                        dynamicHeight += gui::PrintStreamFrames(stream, frameId, m_enables);
                        dynamicHeight += gui::PrintPlaybackProgress(stream, frameId);
                    }

                    if (gui::GetKeyState(VK_UP).pressed)
                    {
                        if (!m_player->IsPaused())
                        {
                            m_player->Stop();
                            goToPrev = true;
                            break;
                        }
                    }

                    if (gui::GetKeyState(VK_DOWN).pressed)
                    {
                        if (!m_player->IsPaused())
                        {
                            m_player->Stop();
                            goToPrev = false;
                            break;
                        }
                    }

                    if (gui::GetKeyState(VK_RETURN).pressed)
                    {
                        if (m_player->IsPaused())
                            m_player->Play();
                        else
                            m_player->Stop();
                    }

                    if (gui::GetKeyState('F').pressed)
                    {
                        if (m_favorites->ContainsFile (stream.file)
                            ? m_favorites->EraseFile  (stream.file)
                            : m_favorites->InsertFile (stream.file))
                        {
                            m_favorites->ExportPlaylist(k_favoritesPath);
                            printStatic = true;   
                        }
                    }

                    for (int key = '1'; key <= '5'; ++key)
                    {
                        if (gui::GetKeyState(key).pressed)
                        {
                            m_enables[key - '1'] ^= true;
                        }
                    }

                    Sleep(1);
                }

                gui::Clear(dynamicHeight);
            }
            else
            {
                std::cout << "Could not init player with module" << std::endl;
            }
        }
        else
        {
            std::cout << "Could not decode file: " << path << std::endl;
        }
        path.clear();
    }
}

void ConvertInputFiles(const std::filesystem::path& outputPath)
{
    std::filesystem::path path;
    while (m_filelist->GetNextFile(path))
    {
        Stream stream;
#if 0
        stream.chip.first.model(Chip::Model::AY8930);
        //stream.chip.second.model(Chip::Model::YM2149);
        //stream.chip.output(Chip::Output::Stereo);
        //stream.chip.clockValue(1500000);
#endif
        if (PSG::Decode(path, stream))
        {
            if (PSG::Encode(outputPath, stream))
            {
                std::cout << "Done file encoding: " << outputPath << std::endl;
            }
            else
            {
                std::cout << "Could not encode file: " << outputPath << std::endl;
            }
        }
        else
        {
            std::cout << "Could not decode file: " << path << std::endl;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

class MyPSG : public PSG
{
public:
    MyPSG(const Chip& chip, Output& output)
        : m_chip(chip)
        , m_output(output)
        , m_player(output)
    {
        for (auto& enable : m_enables) enable = true;
    }


public:
    void PlayFileList(const Filelist& filelist, Filelist& favorites)
    {
        bool goToPrev = false;
        std::filesystem::path path;
        
        while (goToPrev ? filelist.GetPrevFile(path) : filelist.GetNextFile(path))
        {
            Stream stream;
            stream.dchip = m_chip;

            if (Decode(path, stream))
            {
                goToPrev = PlayStream(stream, filelist, favorites);
            }
            else
            {
                std::cout << "Could not decode file: " << path << std::endl;
            }
            path.clear();
        }
    }




protected:
    void OnFrameDecoded(Stream& stream, FrameId frameId) override
    {

    }

    void OnFrameEncoded(Stream& stream, FrameId frameId) override
    {

    }

    void OnFramePlaying(Stream& stream, FrameId frameId)
    {

    }

private:
    bool PlayStream(const Stream& stream, const Filelist& filelist, Filelist& favorites)
    {
        m_staticHeight = 0;
        m_dynamicHeight = 0;

        if (m_player.Init(stream))
        {
            m_printStatic = true;
            FrameId frameId = -1;

            m_player.Play();
            while (m_player.IsPlaying())
            {
                m_output.SetEnables(m_enables);
                gui::Update();

                if (m_player.GetFrameId() != frameId)
                {
                    frameId = m_player.GetFrameId();

                    terminal::cursor::move_up(m_dynamicHeight);
                    m_dynamicHeight = 0;

                    if (m_printStatic)
                    {
                        gui::Clear(m_staticHeight);
                        m_staticHeight = 0;

                        auto index = filelist.GetCurrFileIndex();
                        auto amount = filelist.GetNumberOfFiles();
                        auto favorite = favorites.ContainsFile(stream.file);

                        m_staticHeight += gui::PrintInputFile(stream, index, amount, favorite);
                        m_staticHeight += gui::PrintStreamInfo(stream, m_output);
                        m_printStatic = false;
                    }

                    m_dynamicHeight += gui::PrintStreamFrames(stream, frameId, m_enables);
                    m_dynamicHeight += gui::PrintPlaybackProgress(stream, frameId);
                }

                if (gui::GetKeyState(VK_UP).pressed)
                {
                    if (!m_player.IsPaused())
                    {
                        m_player.Stop();
                        return true;
                    }
                }

                if (gui::GetKeyState(VK_DOWN).pressed)
                {
                    if (!m_player.IsPaused())
                    {
                        m_player.Stop();
                        return false;
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
                    if (favorites.ContainsFile(stream.file)
                        ? favorites.EraseFile(stream.file)
                        : favorites.InsertFile(stream.file))
                    {
                        favorites.ExportPlaylist(k_favoritesPath);
                        m_printStatic = true;
                    }
                }

                for (int key = '1'; key <= '5'; ++key)
                {
                    if (gui::GetKeyState(key).pressed)
                    {
                        m_enables[key - '1'] ^= true;
                    }
                }

                Sleep(1);
            }

            gui::Clear(m_dynamicHeight);
        }
        else
        {
            std::cout << "Could not init player with module" << std::endl;
        }
        return false;
    }

private:
    Chip m_chip;
    Output& m_output;
    Output::Enables m_enables;

    Player m_player;
    size_t m_staticHeight;
    size_t m_dynamicHeight;
    bool m_printStatic;
};

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    bool isConverting = false;
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;

    // setup input path
    if (argc > 1)
        inputPath = argv[1];
    else
        inputPath = k_favoritesPath;
    
    // setup output path
    if (argc > 2)
    {
        outputPath = argv[2];
        isConverting = true;
    }

    // setup file lists
    m_filelist.reset(new Filelist(k_supportedFileTypes, inputPath));
    m_favorites.reset(new Filelist(k_supportedFileTypes, k_favoritesPath));

    PrintWellcome();
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    if (!m_filelist->IsEmpty())
    {
        if (isConverting)
        {
            ConvertInputFiles(outputPath);
        }
        else
        {
            PlayInputFiles();
        }
    }
    return 0;
}
