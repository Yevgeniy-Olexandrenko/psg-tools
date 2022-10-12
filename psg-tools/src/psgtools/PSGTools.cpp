//#include <iostream>
//#include <chrono>
//#include <Windows.h>
//#include <sstream>
//#include <fstream>
//#include <array>
//
//#undef max

#include "PSGTools.h"
#include "PlayFiles.h"
#include "ConvertFiles.h"
#include "interface/ConsoleGUI.h"

static const std::filesystem::path c_favoritesPath = "favorites.m3u";
static Termination m_termination = false;

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    m_termination = true;
    Sleep(10000);
    return TRUE;
}

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
