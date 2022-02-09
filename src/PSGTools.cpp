﻿#include <iostream>
#include <iomanip>
#include <chrono>

#include <indicators/cursor_control.hpp>
#include <indicators/termcolor.hpp>
#include <indicators/progress_bar.hpp>

#include "module/Module.h"
#include "decoders/DecodeVTX.h"
#include "decoders/DecodePT3.h"
#include "decoders/DecodePSG.h"

#include "output/AYMStreamer/AYMStreamer.h"
#include "output/AY38910/AY38910.h"
#include "module/Player.h"

const std::string k_folder = "../../chiptunes/";
const std::string k_file = "rElaTed_.vtx";
const std::string k_output = "output.txt";
const int k_comPortIndex = 4;

static Player * g_player = nullptr;

static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType)
{
    if (g_player) g_player->Stop();
    return TRUE;
}


bool DecodeFileToModule(const std::string& filePath, Module& module)
{
    std::shared_ptr<Decoder> decoders[]{
        std::shared_ptr<Decoder>(new DecodeVTX()),
        std::shared_ptr<Decoder>(new DecodePT3()),
        std::shared_ptr<Decoder>(new DecodePSG()),
    };

    module.SetFilePath(filePath);
    for (std::shared_ptr<Decoder> decoder : decoders)
    {
        if (decoder->Open(module))
        {
            Frame frame;
            while (decoder->Decode(frame))
            {
                frame.FixValues();
                module.AddFrame(frame);
                frame.SetUnchanged();
            }

            decoder->Close(module);
            return true;
        }
    }
    return false;
}

void SaveModuleDebugOutput(const Module& module)
{
    std::ofstream file;
    file.open(k_output);

    if (file)
    {
        auto delimiter = [&file]()
        {
            for (int i = 0; i < 48; ++i) file << '-';
            file << std::endl;
        };

        delimiter();
        file << module;
        delimiter();

        file << "frame  [ r7|r1r0 r8|r3r2 r9|r5r4 rA|rCrB rD|r6 ]" << std::endl;
        delimiter();

        for (FrameId i = 0; i < module.GetFrameCount(); ++i)
        {
            if (i && module.HasLoop() && i == module.GetLoopFrameId()) delimiter();
            file << std::setfill('0') << std::setw(5) << i;
            file << "  [ " << module.GetFrame(i) << " ]" << std::endl;
            
        }
        delimiter();
        file.close();
    }
}

int main()
{
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    Module module;
    DecodeFileToModule(k_folder + k_file, module);
    //SaveModuleDebugOutput(module);

    ////////////////////////////////////////////////////////////////////////////

    indicators::show_console_cursor(false);
    size_t w = indicators::terminal_width() - 1;

#if 0
    AYMStreamer output(module, k_comPortIndex);
#else
    AY38910 output(module);
#endif
    Player player(output);

    std::cout << termcolor::bright_cyan << std::string(w, '-') << termcolor::reset << std::endl;
    std::cout << termcolor::bright_red << "PSG Tools v1.0" << termcolor::reset << std::endl;
    std::cout << termcolor::bright_red << "by Yevgeniy Olexandrenko" << termcolor::reset << std::endl;
    std::cout << termcolor::bright_cyan << std::string(w, '-') << termcolor::reset << std::endl;
    std::cout << module;
    std::cout << termcolor::bright_cyan << std::string(w, '-') << termcolor::reset << std::endl;

    indicators::ProgressBar bar{
        indicators::option::BarWidth{50},
        indicators::option::Start{"["},
        indicators::option::Fill{"|"},
        indicators::option::Lead{"|"},
        indicators::option::Remainder{"-"},
        indicators::option::End{"]"},
    //    indicators::option::PostfixText{"Loading dependency 1/4"},
    //    indicators::option::ForegroundColor{indicators::Color::cyan},
        indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}
    };

    g_player = &player;
    if (player.Init(module))
    {
        FrameId oldFrame = -1;
        auto start = std::chrono::steady_clock::now();
        player.Play();

        while (player.IsPlaying())
        {
//#if 0
//            
//            bool left  = (GetAsyncKeyState(VK_LEFT ) & 0x1) != 0;
//            bool right = (GetAsyncKeyState(VK_RIGHT) & 0x1) != 0;
//            bool up    = (GetAsyncKeyState(VK_UP   ) & 0x1) != 0;
//            bool down  = (GetAsyncKeyState(VK_DOWN ) & 0x1) != 0;
//#else
//            bool left  = (GetKeyState(VK_LEFT  ) & 0x80) != 0;
//            bool right = (GetKeyState(VK_RIGHT ) & 0x80) != 0;
//            bool up    = (GetKeyState(VK_UP    ) & 0x80) != 0;
//            bool down  = (GetKeyState(VK_DOWN  ) & 0x80) != 0;
//            bool enter = (GetAsyncKeyState(VK_RETURN) & 0x1) != 0;
//#endif
//
//            if (right)
//            {
//                player.Step(+10);
//            }
//            else if (left)
//            {
//                player.Step(-10);
//            }
//            else
//            {
//                player.Step(+1);
//            }
//
//            if (enter)
//            {
//                if (player.IsPaused())
//                    player.Play();
//                else 
//                    player.Stop();
//            }

            FrameId newFrame = player.GetFrameId();
            if (newFrame != oldFrame)
            {
                oldFrame = newFrame;
                bar.set_progress(newFrame * 100 / module.GetPlaybackFrameCount());
            }
            Sleep(1000);
        }

        uint32_t duration = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        int ms = duration % 1000; duration /= 1000;
        int ss = duration % 60;   duration /= 60;
        int mm = duration % 60;   duration /= 60;
        int hh = duration;

        std::cout << std::endl;
        std::cout << "Duration: " <<
            std::setfill('0') << std::setw(2) << hh << ':' <<
            std::setfill('0') << std::setw(2) << mm << ':' <<
            std::setfill('0') << std::setw(2) << ss << '.' <<
            std::setfill('0') << std::setw(3) << ms << std::endl;
    }
    
    indicators::show_console_cursor(true);
}
