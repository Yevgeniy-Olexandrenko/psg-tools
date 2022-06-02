﻿#include <iostream>
#include <chrono>
#include <Windows.h>

#include "PSGLib.h"
#include "Filelist.h"
#include "Interface.h"


const std::string k_supportedFileTypes = "sqt|ym|stp|vgz|vgm|asc|stc|pt2|pt3|psg|vtx";
const int k_comPortIndex = 4;

////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<Filelist> m_filelist;
std::shared_ptr<Stream>   m_stream;
std::shared_ptr<Output>   m_output;
std::shared_ptr<Player>   m_player;

static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType)
{
    if (m_player)
    {
        m_player->Stop();
    }
    return TRUE;
}

void PrintDelimiter()
{
    using namespace terminal;
    size_t term_w = terminal_width() - 2;
    std::cout << ' ' << color::bright_cyan;
    std::cout << std::string(term_w, '-');
    std::cout << color::reset << std::endl;
}

////////////////////////////////////////////////////////////////////////////////

bool SetConsoleWindowSize(SHORT x, SHORT y)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

    if (h == INVALID_HANDLE_VALUE)
        return false;

    // If either dimension is greater than the largest console window we can have,
    // there is no point in attempting the change.
    {
        COORD largestSize = GetLargestConsoleWindowSize(h);
        if (x > largestSize.X)
            return false;
        if (y > largestSize.Y)
            return false;
    }


    CONSOLE_SCREEN_BUFFER_INFO bufferInfo;
    if (!GetConsoleScreenBufferInfo(h, &bufferInfo))
        return false;

    SMALL_RECT& winInfo = bufferInfo.srWindow;
    COORD windowSize = { winInfo.Right - winInfo.Left + 1, winInfo.Bottom - winInfo.Top + 1 };

    if (windowSize.X > x || windowSize.Y > y)
    {
        // window size needs to be adjusted before the buffer size can be reduced.
        SMALL_RECT info =
        {
            0,
            0,
            x < windowSize.X ? x - 1 : windowSize.X - 1,
            y < windowSize.Y ? y - 1 : windowSize.Y - 1
        };

        if (!SetConsoleWindowInfo(h, TRUE, &info))
            return false;
    }

    COORD size = { x, y };
    if (!SetConsoleScreenBufferSize(h, size))
        return false;


    SMALL_RECT info = { 0, 0, x - 1, y - 1 };
    if (!SetConsoleWindowInfo(h, TRUE, &info))
        return false;

    return true;
}

void PlayInputFiles()
{
    using namespace terminal;

    m_filelist->shuffle();
    bool goToPrev = false;

    std::filesystem::path path;
    while (true)
    {
        bool isDone = goToPrev ? m_filelist->prev(path) : m_filelist->next(path);
        if (!isDone) break;

        m_stream.reset(new Stream());
        if (PSG::Decode(path, *m_stream))
        {
            goToPrev = false; // if decoding OK, move to next by default
            Interface::PrintInputFile(*m_stream, m_filelist->index(), m_filelist->count());

            if (m_player->Init(*m_stream))
            {
                Interface::PrintModuleInfo(*m_stream, *m_output);
                std::cout << std::endl;

                m_player->Play();
                auto start = std::chrono::steady_clock::now();

                FrameId oldFrame = -1;
                while (m_player->IsPlaying())
                {
                    SetConsoleWindowSize(87, 32);
                    Interface::HandleKeyboardInput();

                    FrameId newFrame = m_player->GetFrameId();
                    if (newFrame != oldFrame)
                    {
                        oldFrame = newFrame;
                        Interface::PrintModuleFrames2(*m_stream, newFrame, 12);
                        cursor::move_down(12);
                        Interface::PrintPlaybackProgress();
                        cursor::move_up(12);
                    }

                    if (Interface::GetKey(VK_UP).pressed)
                    {
                        if (!m_player->IsPaused())
                        {
                            m_player->Stop();
                            goToPrev = true;
                            break;
                        }
                    }

                    if (Interface::GetKey(VK_DOWN).pressed)
                    {
                        if (!m_player->IsPaused())
                        {
                            m_player->Stop();
                            goToPrev = false;
                            break;
                        }
                    }

                    if (Interface::GetKey(VK_RETURN).pressed)
                    {
                        if (m_player->IsPaused())
                            m_player->Play();
                        else
                            m_player->Stop();
                    }

                    Sleep(1);
                }
                Interface::PrintBlankArea(0, 13);


                //uint32_t duration = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                //        int ms = duration % 1000; duration /= 1000;
                //        int ss = duration % 60;   duration /= 60;
                //        int mm = duration % 60;   duration /= 60;
                //        int hh = duration;
                //
                //        std::cout << std::endl;
                //        std::cout << " Duration: " <<
                //            std::setfill('0') << std::setw(2) << hh << ':' <<
                //            std::setfill('0') << std::setw(2) << mm << ':' <<
                //            std::setfill('0') << std::setw(2) << ss << '.' <<
                //            std::setfill('0') << std::setw(3) << ms << std::endl;

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
    while (true)
    {
        bool isDone = m_filelist->next(path);
        if (!isDone) break;

        m_stream.reset(new Stream());
        if (PSG::Decode(path, *m_stream))
        {
            if (PSG::Encode(outputPath, *m_stream))
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

int main(int argc, char* argv[])
{
    if (argc < 2) return 1;

    bool isConverting = false;
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;

    inputPath = argv[1];
    if (argc > 2)
    {
        outputPath = argv[2];
        isConverting = true;
    }

#if 1
    SetConsoleWindowSize(87, 32);

    HWND consoleWindow = GetConsoleWindow();
    SetWindowLong(consoleWindow, GWL_STYLE, GetWindowLong(consoleWindow, GWL_STYLE) & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX);


    HANDLE hInput;
    DWORD prev_mode;
    hInput = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hInput, &prev_mode);
    SetConsoleMode(hInput, prev_mode & ~ENABLE_QUICK_EDIT_MODE);
#endif    
 
    using namespace terminal;
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
    size_t w = terminal_width() - 1;

    PrintDelimiter();
    std::cout << ' ' << color::bright_red << "PSG Tools v1.0" << color::reset << std::endl;
    std::cout << ' ' << color::bright_red << "by Yevgeniy Olexandrenko" << color::reset << std::endl;
    PrintDelimiter();
    std::cout << std::endl;

#if 0
    m_output.reset(new Streamer(k_comPortIndex));
#else
    m_output.reset(new Emulator());
#endif
    m_player.reset(new Player(*m_output));
    m_filelist.reset(new Filelist(k_supportedFileTypes, inputPath));

    if (!m_filelist->empty())
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
    cursor::show(true);

    return 0;
}