#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

#include "UITest.h"
#include "Console.h"
#include "KeyboardInput.h"
#include "Screen.h"

namespace
{
    const int k_frameRate = 50;
    const int k_trackFrames = 8 * k_frameRate;
    const int k_maxInfoRows = 5;
    const int k_maxDumpRows = 17;
    const std::string k_dumpHeader = "|R7|R1R0 R8|R3R2 R9|R5R4 RA|RCRB RD|R6|";

    struct Track
    {
        std::string file;
        std::string title;
        std::string artist;
    };

    const Track k_tracks[] =
    {
        { "short.pt3", "Short", "Someone" },
        { "a_rather_long_file_name_to_check_how_the_header_line_is_cut_on_narrow_windows.pt3",
          "A very long title that goes far beyond the right edge of a narrow console window", "Somebody Else" },
        { "medium_name.stc", "Medium", "Unknown Artist" },
        { "last.asc", "The Last One", "Nobody" },
    };
    const int k_trackCount = int(sizeof(k_tracks) / sizeof(k_tracks[0]));

    // " ------[ label ]--" stretched to the canvas width
    void DrawBar(PrintBuffer& canvas, int y, const std::string& label, SHORT labelColor, float position)
    {
        int range = std::max(0, canvas.w - int(label.length()) - 9);
        int left = int(position * range + 0.5f);

        canvas.position(0, y).draw(' ');
        canvas.color(FG_BLUE).draw(std::string(2 + left, '-'));
        canvas.color(FG_CYAN).draw("[ ");
        canvas.color(labelColor).draw(label);
        canvas.color(FG_CYAN).draw(" ]");
        canvas.color(FG_BLUE).draw(std::string(2 + range - left, '-'));
    }

    void DrawProperty(PrintBuffer& canvas, int y, const std::string& label, const std::string& value)
    {
        canvas.position(1, y).color(FG_YELLOW).draw(label);
        canvas.color(FG_DARK_GREY).draw(std::string(9 - label.length(), '.'));
        canvas.color(FG_MAGENTA).draw(": ");
        canvas.color(FG_WHITE).draw(value);
    }

    void DrawDump(PrintBuffer& canvas, int y, int rows, int frame)
    {
        int x = std::max(0, (canvas.w - 6 - int(k_dumpHeader.length())) / 2);
        canvas.position(x, y).color(FG_DARK_CYAN).draw("FRAME ");
        canvas.color(FG_CYAN).draw(k_dumpHeader);

        int first = frame - (rows - 1) / 2;
        for (int i = 1; i < rows; ++i)
        {
            int f = (first + i - 1);
            SHORT background = (f == frame ? BG_DARK_MAGENTA : BG_BLACK);
            canvas.position(x, y + i).color((f == frame ? FG_WHITE : FG_DARK_GREY) | background);

            if (f < 0)
                canvas.draw(".....");
            else
            {
                std::stringstream ss;
                ss << std::setfill('0') << std::setw(5) << f;
                canvas.draw(ss.str());
            }
            canvas.draw(' ');

            // fake register values: two hex digits per register name in the header
            for (size_t c = 0; c < k_dumpHeader.length();)
            {
                if (k_dumpHeader[c] == '|' || k_dumpHeader[c] == ' ')
                {
                    canvas.color(FG_CYAN | background).draw(k_dumpHeader[c++]);
                    continue;
                }

                uint32_t hash = (uint32_t(f) * 2654435761u) ^ (uint32_t(c) * 40503u);
                if (f < 0 || ((hash >> 11) & 3) != 0)
                {
                    canvas.color(FG_DARK_GREY | background).draw("..");
                }
                else
                {
                    static const char k_hex[] = "0123456789ABCDEF";
                    canvas.color(FG_GREEN | background);
                    canvas.draw(k_hex[(hash >> 4) & 15]).draw(k_hex[hash & 15]);
                }
                c += 2;
            }
        }
    }

    std::string FormatTime(int frame)
    {
        int seconds = (frame / k_frameRate);
        std::stringstream ss;
        ss << std::setfill('0') << std::setw(2) << seconds / 60 << ':' << std::setw(2) << seconds % 60;
        return ss.str();
    }
}

void RunUITest(std::atomic<bool>& termination)
{
    console::SetupInput();
    console::SetTitle(L"PSG Tools UI test");

    Screen screen;
    KeyboardInput keyboard;
    int resizes = 0;

    auto ruler = [](PrintBuffer& canvas, int y)
    {
        canvas.position(0, y).color(FG_BLUE).draw(' ' + std::string(std::max(0, canvas.w - 1), '-'));
    };
    screen.Print(ruler);
    screen.Print(" PSG Tools UI test", FG_RED);
    screen.Print(" Draws a fake playlist without playback. Resize the window and scroll the history while it runs.");
    screen.Print(ruler);

    auto next = std::chrono::steady_clock::now();
    for (int index = 0; index < k_trackCount && !termination; ++index)
    {
        const Track& track = k_tracks[index];
        std::string number = std::to_string(index + 1) + '/' + std::to_string(k_trackCount);
        if (index == 2)
        {
            screen.Print("Could not decode file: \"chiptunes\\Broken\\missing.pt3\"", FG_RED);
        }

        const std::pair<std::string, std::string> info[k_maxInfoRows] =
        {
            { "Title", track.title },
            { "Artist", track.artist },
            { "Type", "Fake module" },
            { "Frames", std::to_string(k_trackFrames) + " @ " + std::to_string(k_frameRate) + " Hz" },
            { "Output", "Nothing, this is a UI test" },
        };

        for (int frame = 0; frame < k_trackFrames && !termination; ++frame)
        {
            keyboard.Update();
            screen.Update(keyboard);
            if (screen.Sync()) ++resizes;
            if (index == 1 && frame == k_trackFrames / 2)
            {
                screen.Print(" [test] A long line printed into the history while the live view is on "
                    "screen. It is cut at the edge of a narrow window, and the view has to stay right below it.", FG_CYAN);
            }

            // layout: header, properties, register dump, progress
            int maxHeight = screen.GetMaxHeight();
            int infoRows = std::clamp(maxHeight - 2, 0, k_maxInfoRows);
            int dumpRows = std::min(k_maxDumpRows, maxHeight - 2 - infoRows);
            if (dumpRows < 4) dumpRows = 0; // the header and three frames at least

            PrintBuffer& canvas = screen.Begin(2 + infoRows + dumpRows);
            DrawBar(canvas, 0, number + ' ' + track.file, FG_WHITE, 1.f);
            for (int i = 0; i < infoRows; ++i)
            {
                DrawProperty(canvas, 1 + i, info[i].first, info[i].second);
            }
            if (dumpRows)
            {
                DrawDump(canvas, 1 + infoRows, dumpRows, frame);
            }

            static const char k_spinner[] = R"(_\|/)";
            std::string progress = std::string(1, k_spinner[(frame / 8) & 3]) + ' ' + FormatTime(k_trackFrames - frame)
                + " | " + std::to_string(screen.GetWidth()) + 'x' + std::to_string(maxHeight)
                + " | resizes " + std::to_string(resizes);
            DrawBar(canvas, canvas.h - 1, progress, FG_WHITE, float(frame) / k_trackFrames);
            screen.Render();

            next += std::chrono::milliseconds(1000 / k_frameRate);
            std::this_thread::sleep_until(next);
        }

        // the header and all the properties of the track stay in the history
        screen.Erase();
        screen.Print([number, track](PrintBuffer& canvas, int y) { DrawBar(canvas, y, number + ' ' + track.file, FG_WHITE, 1.f); });
        for (const auto& property : info)
        {
            screen.Print([property](PrintBuffer& canvas, int y) { DrawProperty(canvas, y, property.first, property.second); });
        }
    }

    screen.Print(termination ? " UI test interrupted" : " UI test finished", termination ? FG_YELLOW : FG_GREEN);
    screen.Close();
    console::RestoreInput();
    console::RestoreTitle();
}
