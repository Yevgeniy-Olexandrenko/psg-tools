#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iomanip>
#include <sstream>
#include <vector>

#include "ConsoleGUI.h"
#include "Console.h"
#include "Screen.h"

namespace gui
{
    // the header row and 15 frames around the playing one
    const int k_maxFramesRows = 16;
    // the header row and the frames before, at and after the playing one
    const int k_minFramesRows = 4;
    // the levels of the last frames for the trails of the meters, more than
    // the widest window shows
    const size_t k_maxLevels = 1024;

    // The bars are left half blocks, so they stand apart. The ends of the
    // bars of the meters and the peaks of the analyzer are quarter blocks:
    // two levels in a cell. The raster fonts have no quarter blocks.
    const wchar_t k_halfBlock = wchar_t(0x258C);
    const wchar_t k_lowerQuarter = wchar_t(0x2596);
    const wchar_t k_upperQuarter = wchar_t(0x2598);

    // the colors of the registers dump: tone, noise, envelope
    const SHORT k_soundColors[LevelMeter::k_sounds] = { FG_GREEN, FG_CYAN, FG_YELLOW };

    // a bar of the analyzer rises at once and falls by this number of levels
    // (of 255) in a frame; its peak holds, then falls more slowly
    const int k_barFall = 6;
    const int k_peakHold = 25;
    const int k_peakFall = 3;

    // A column of the analyzer shows the loudest of 1 to 4 semitones, the
    // smallest number that fits the width and gives whole columns in an
    // octave. A narrower window shows no analyzer.
    const int k_maxBandsPerColumn = 4;

    // the frequencies on the axis of the analyzer, in Hz: the octaves from
    // 1 kHz, with the usual names of the octave bands
    const std::pair<double, const char*> k_analyzerLabels[] =
    {
        { 31.25, "31" }, { 62.5, "63" }, { 125, "125" }, { 250, "250" }, { 500, "500" },
        { 1000, "1k" }, { 2000, "2k" }, { 4000, "4k" }, { 8000, "8k" }, { 16000, "16k" },
    };

    // decoding and encoding report every single frame, which is way more
    // often than the console needs to be redrawn
    const auto k_progressInterval = std::chrono::milliseconds(20);

    struct Property
    {
        std::string label;
        std::string value;
        SHORT color;
    };

    struct FramesLayout
    {
        int rows;     // zero if there is no room for the frames
        int chips;    // chips shown side by side
        bool stacked; // the second chip is shown below the first one
        bool meters;  // output level meters at both sides
        int width;    // width of the registers dump
    };

    PrintBuffer   m_framesBuffer(0, 0);
    KeyboardInput m_keyboardInput;

    // the levels of the frames written to the output, the newest last
    std::deque<Output::Levels> m_levels;

    // the bands of the analyzer as it shows them
    struct AnalyzerBand
    {
        int level;
        int peak;
        int hold;
        LevelMeter::Sound sound;
    };
    AnalyzerBand m_analyzer[LevelMeter::k_bands] {};

    Screen& screen()
    {
        static Screen s_screen;
        return s_screen;
    }

    void onExit() { Shutdown(); }
    void onAbort(int) { Shutdown(); }
    LONG WINAPI onCrash(EXCEPTION_POINTERS*) { Shutdown(); return EXCEPTION_CONTINUE_SEARCH; }

    void Init(const std::wstring& title)
    {
        // the console window size is never changed: under ConPTY (Windows Terminal)
        // that only resizes conhost's own buffer and desyncs it from the terminal
        console::SetupInput();
        console::SetTitle(title);
        screen();

        // the console gets its state back even when the program ends abnormally:
        // on exit() from a library, on std::terminate() in any thread, on a crash
        std::atexit(onExit);
        std::signal(SIGABRT, onAbort);
        SetUnhandledExceptionFilter(onCrash);
    }

    void Shutdown()
    {
        // the history goes to the standard output buffer, even on an abnormal exit
        screen().Close();
        console::RestoreInput();
        console::RestoreTitle();
    }

	void Update()
	{
		m_keyboardInput.Update();
		screen().Update(m_keyboardInput);
	}

    void SetLivePinned(bool pinned)
    {
        screen().SetLivePinned(pinned);
    }

    void UpdateBusy()
    {
        // called for every frame decoded or encoded, the input is read only
        // as often as the progress is drawn
        static auto s_time = std::chrono::steady_clock::time_point();
        auto now = std::chrono::steady_clock::now();
        if (now - s_time < k_progressInterval) return;

        s_time = now;
        Update();
    }

	const KeyState& GetKeyState(int key)
	{
		return m_keyboardInput.GetKeyState(key);
	}

    void Print(const std::string& text, WORD color)
    {
        screen().Print(text, color);
    }

    void PrintRuler()
    {
        screen().Print([](PrintBuffer& canvas, int y)
        {
            canvas.position(0, y).color(FG_BLUE).draw(' ' + std::string(std::max(0, canvas.w - 1), '-'));
        });
    }

    ////////////////////////////////////////////////////////////////////////////

    void trim(std::string& str)
    {
        std::for_each(str.begin(), str.end(), [](char& c) { if (c < 0) c = '?'; });
        str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](int c) { return !std::isspace(c); }));
        str.erase(std::find_if(str.rbegin(), str.rend(), [](int c) { return !std::isspace(c); }).base(), str.end());
    };

    // cuts out the middle of the string to make it fit the given length
    template<typename String> void limit(String& str, int length)
    {
        int size = int(str.length());
        if (size > length)
        {
            if (length < 5)
            {
                str = str.substr(0, std::max(0, length));
            }
            else
            {
                String separator(3, ' ');
                separator[1] = '~';

                int head = (length - 3) / 2;
                int tail = (length - 3 - head);
                str = str.substr(0, head) + separator + str.substr(size - tail);
            }
        }
    }

    bool isTimeToDraw(const FileView& file)
    {
        static std::filesystem::path s_path;
        static auto s_time = std::chrono::steady_clock::time_point();

        auto now = std::chrono::steady_clock::now();
        if (file.path == s_path && now - s_time < k_progressInterval) return false;

        s_path = file.path;
        s_time = now;
        return true;
    }

    ////////////////////////////////////////////////////////////////////////////

    void drawFileHeader(PrintBuffer& canvas, int y, const FileView& file)
    {
        std::string number = std::to_string(file.index + 1);
        std::string amount = std::to_string(file.amount);
        std::wstring name = file.path.stem().wstring();
        std::wstring ext = file.path.extension().wstring();

        // " ---[ 1/10 name.ext ]--" stretched to the canvas width
        int fixed = int(9 + number.length() + amount.length() + ext.length());
        limit(name, canvas.w - fixed - 2);
        int dashes = std::max(0, canvas.w - fixed - int(name.length()));

        canvas.position(0, y).draw(' ');
        canvas.color(FG_BLUE).draw(std::string(dashes, '-'));
        canvas.color(FG_CYAN).draw("[ ");
        canvas.color(FG_YELLOW).draw(number);
        canvas.color(FG_CYAN).draw('/');
        canvas.color(FG_YELLOW).draw(amount);
        canvas.draw(' ');
        canvas.color(file.favorite ? FG_MAGENTA : FG_WHITE).draw(name);
        canvas.color(FG_DARK_GREY).draw(ext);
        canvas.color(FG_CYAN).draw(" ]");
        canvas.color(FG_BLUE).draw("--");
    }

    void addProperty(std::vector<Property>& properties, const std::string& label, std::string value, SHORT color)
    {
        trim(value);
        if (!value.empty())
        {
            properties.push_back({ label, value, color });
        }
    }

    // the brief properties are known while the stream is decoding
    std::vector<Property> getProperties(const Stream& stream, const std::string* output)
    {
        std::vector<Property> properties;
        addProperty(properties, "Title", stream.ToString(Stream::Property::Title), FG_GREEN);
        addProperty(properties, "Artist", stream.ToString(Stream::Property::Artist), FG_WHITE);
        addProperty(properties, "Comment", stream.ToString(Stream::Property::Comment), FG_WHITE);
        addProperty(properties, "Type", stream.ToString(Stream::Property::Type), FG_WHITE);
        addProperty(properties, "Chip", stream.ToString(Stream::Property::Chip), FG_WHITE);
        if (output)
        {
            addProperty(properties, "Frames", stream.ToString(Stream::Property::Frames), FG_WHITE);
            addProperty(properties, "Duration", stream.ToString(Stream::Property::Duration), FG_WHITE);
            addProperty(properties, "Output", *output, FG_CYAN);
        }
        return properties;
    }

    void drawProperty(PrintBuffer& canvas, int y, const Property& property)
    {
        std::string value = property.value;
        limit(value, canvas.w - 12);

        canvas.position(1, y).color(FG_YELLOW).draw(property.label);
        canvas.color(FG_DARK_GREY).draw(std::string(9 - property.label.length(), '.'));
        canvas.color(FG_MAGENTA).draw(": ");
        canvas.color(property.color).draw(value);
    }

    // " --[ / value ]-----" with the dashes on the left growing with the position
    void drawProgress(PrintBuffer& canvas, int y, float position, int spinner, const std::string& value)
    {
        static const char k_spinner[] = R"(_\|/)";
        int range = std::max(0, canvas.w - int(value.length()) - 11);
        int left = int(std::clamp(position, 0.f, 1.f) * range + 0.5f);

        canvas.position(0, y).draw(' ');
        canvas.color(FG_BLUE).draw(std::string(2 + left, '-'));
        canvas.color(FG_CYAN).draw("[ ");
        canvas.color(FG_MAGENTA).draw(k_spinner[spinner & 3]).draw(' ');
        canvas.color(FG_WHITE).draw(value);
        canvas.color(FG_CYAN).draw(" ]");
        canvas.color(FG_BLUE).draw(std::string(2 + range - left, '-'));
    }

    // the header, the properties that fit the window and the progress bar
    template<typename DrawProgress>
    void drawFileView(const FileView& file, const std::vector<Property>& properties, DrawProgress drawProgress)
    {
        screen().Sync();
        int rows = std::clamp(screen().GetMaxHeight() - 2, 0, int(properties.size()));

        PrintBuffer& canvas = screen().Begin(2 + rows);
        drawFileHeader(canvas, 0, file);
        for (int i = 0; i < rows; ++i)
        {
            drawProperty(canvas, 1 + i, properties[i]);
        }
        drawProgress(canvas, canvas.h - 1);
        screen().Render();
    }

	////////////////////////////////////////////////////////////////////////////

    const std::string k_headerForExpMode = "|07|0100 0816 0C0B 0D|0302 0917 1110 14|0504 0A18 1312 15|191A06|";
    const std::string k_headerForComMode = "|R7|R1R0 R8|R3R2 R9|R5R4 RA|RCRB RD|R6|";

    class Coloring
    {
        SHORT m_color0;
        SHORT m_color1;
        bool m_playing;

    public:
        Coloring(bool playing, bool enabled, bool withTone, bool withNoise, bool withEnvelope)
            : m_color0(FG_DARK_GREY)
            , m_color1(FG_DARK_GREY)
            , m_playing(playing)
        {
            if (playing)
            {
                if (enabled) m_color1 = FG_WHITE;
                m_color1 |= BG_DARK_MAGENTA;
            }
            else if (enabled)
            {
                m_color1 = (withEnvelope ? FG_YELLOW : (withNoise ? FG_CYAN : (withTone ? FG_GREEN : FG_RED)));
            }
        }

        bool IsPlaying() const { return m_playing; }
        SHORT GetColor(bool isChanged) const { return (isChanged ? m_color1 : m_color0); }
    };

    void printNibble(uint8_t nibble)
    {
        nibble &= 0x0F;
        m_framesBuffer.draw(char((nibble >= 0x0A ? 'A' - 0x0A : '0') + nibble));
    };

    void printRegisterValue(int chip, const Frame& frame, Register reg, const Coloring& coloring)
    {
        if (frame[chip].IsChanged(reg) || coloring.IsPlaying())
        {
            m_framesBuffer.color(coloring.GetColor(true));
            uint8_t data = frame[chip].GetData(reg);
            printNibble(data >> 4);
            printNibble(data);
        }
        else
        {
            m_framesBuffer.color(coloring.GetColor(false));
            m_framesBuffer.draw("..");
        }
    }

    void printRegistersValuesForCompatibleMode(int chip, const Frame& frame, bool playing, const Output::Enables& enables)
    {
        const bool enableT[]{ frame[chip].IsToneEnabled(0), frame[chip].IsToneEnabled(1), frame[chip].IsToneEnabled(2) };
        const bool enableN[]{ frame[chip].IsNoiseEnabled(0), frame[chip].IsNoiseEnabled(1), frame[chip].IsNoiseEnabled(2) };
        const bool enableE[]{ frame[chip].IsEnvelopeEnabled(0), frame[chip].IsEnvelopeEnabled(1), frame[chip].IsEnvelopeEnabled(2) };
        const bool periodE = frame[chip].IsPeriodicEnvelope(0);

        Coloring coloringM
        {
            playing,
            (enables[0] || enables[1] || enables[2] || enables[4]),
            (
                (enables[0] && enableT[Frame::Channel::A]) ||
                (enables[1] && enableT[Frame::Channel::B]) ||
                (enables[2] && enableT[Frame::Channel::C])
            ),
            enables[4] && (
                enableN[Frame::Channel::A] ||
                enableN[Frame::Channel::B] ||
                enableN[Frame::Channel::C]
            ),
            false
        };
        Coloring coloringA
        {
            playing,
            enables[0],
            enables[0] && enableT[Frame::Channel::A],
            enables[4] && enableN[Frame::Channel::A],
            enables[3] && enableE[Frame::Channel::A] && periodE,

        };
        Coloring coloringB
        {
            playing,
            enables[1],
            enables[1] && enableT[Frame::Channel::B],
            enables[4] && enableN[Frame::Channel::B],
            enables[3] && enableE[Frame::Channel::B] && periodE,
        };
        Coloring coloringC
        {
            playing,
            enables[2],
            enables[2] && enableT[Frame::Channel::C],
            enables[4] && enableN[Frame::Channel::C],
            enables[3] && enableE[Frame::Channel::C] && periodE,
        };
        Coloring coloringE
        {
            playing,
            enables[3],
            false,
            false,
            (
                enableE[Frame::Channel::A] ||
                enableE[Frame::Channel::B] ||
                enableE[Frame::Channel::C]
            )
        };
        Coloring coloringN
        {
            playing,
            enables[4],
            false,
            (
                enableN[Frame::Channel::A] ||
                enableN[Frame::Channel::B] ||
                enableN[Frame::Channel::C]
            ),
            false
        };

        uint16_t color = (playing ? BG_DARK_MAGENTA | FG_CYAN : FG_CYAN);
        m_framesBuffer.color(color).draw('|');
        printRegisterValue(chip, frame, Register::Mixer, coloringM);

        m_framesBuffer.color(color).draw('|');
        printRegisterValue(chip, frame, Register::A_Coarse, coloringA);
        printRegisterValue(chip, frame, Register::A_Fine,   coloringA);
        m_framesBuffer.draw(' ');
        printRegisterValue(chip, frame, Register::A_Volume, coloringA);

        m_framesBuffer.color(color).draw('|');
        printRegisterValue(chip, frame, Register::B_Coarse, coloringB);
        printRegisterValue(chip, frame, Register::B_Fine,   coloringB);
        m_framesBuffer.draw(' ');
        printRegisterValue(chip, frame, Register::B_Volume, coloringB);

        m_framesBuffer.color(color).draw('|');
        printRegisterValue(chip, frame, Register::C_Coarse, coloringC);
        printRegisterValue(chip, frame, Register::C_Fine,   coloringC);
        m_framesBuffer.draw(' ');
        printRegisterValue(chip, frame, Register::C_Volume, coloringC);

        m_framesBuffer.color(color).draw('|');
        printRegisterValue(chip, frame, Register::E_Coarse, coloringE);
        printRegisterValue(chip, frame, Register::E_Fine,   coloringE);
        m_framesBuffer.draw(' ');
        printRegisterValue(chip, frame, Register::E_Shape,  coloringE);

        m_framesBuffer.color(color).draw('|');
        printRegisterValue(chip, frame, Register::N_Period, coloringN);
        m_framesBuffer.color(color).draw('|');
    }

    void printRegistersValuesForExpandedMode(int chip, const Frame& frame, bool playing, const Output::Enables& enables)
    {
        const bool enableT[]{ frame[chip].IsToneEnabled(0), frame[chip].IsToneEnabled(1), frame[chip].IsToneEnabled(2) };
        const bool enableN[]{ frame[chip].IsNoiseEnabled(0), frame[chip].IsNoiseEnabled(1), frame[chip].IsNoiseEnabled(2) };
        const bool enableE[]{ frame[chip].IsEnvelopeEnabled(0), frame[chip].IsEnvelopeEnabled(1), frame[chip].IsEnvelopeEnabled(2) };
        const bool periodE[]{ frame[chip].IsPeriodicEnvelope(0), frame[chip].IsPeriodicEnvelope(1), frame[chip].IsPeriodicEnvelope(2) };

        Coloring coloringM
        {
            playing,
            (enables[0] || enables[1] || enables[2] || enables[4]),
            (
                (enables[0] && enableT[Frame::Channel::A]) ||
                (enables[1] && enableT[Frame::Channel::B]) ||
                (enables[2] && enableT[Frame::Channel::C])
            ),
            enables[4] && (
                enableN[Frame::Channel::A] ||
                enableN[Frame::Channel::B] ||
                enableN[Frame::Channel::C]
            ),
            false
        };
        Coloring coloringA
        {
            playing,
            enables[0],
            enables[0] && enableT[Frame::Channel::A],
            enables[4] && enableN[Frame::Channel::A],
            enables[3] && enableE[Frame::Channel::A] && periodE[Frame::Channel::A],

        };
        Coloring coloringB
        {
            playing,
            enables[1],
            enables[1] && enableT[Frame::Channel::B],
            enables[4] && enableN[Frame::Channel::B],
            enables[3] && enableE[Frame::Channel::B] && periodE[Frame::Channel::B],
        };
        Coloring coloringC
        {
            playing,
            enables[2],
            enables[2] && enableT[Frame::Channel::C],
            enables[4] && enableN[Frame::Channel::C],
            enables[3] && enableE[Frame::Channel::C] && periodE[Frame::Channel::C],
        };
        Coloring coloringEA
        {
            playing,
            enables[3],
            false,
            false,
            enableE[Frame::Channel::A]
        };
        Coloring coloringEB
        {
            playing,
            enables[3],
            false,
            false,
            enableE[Frame::Channel::B]
        };
        Coloring coloringEC
        {
            playing,
            enables[3],
            false,
            false,
            enableE[Frame::Channel::C]
        };
        Coloring coloringN
        {
            playing,
            enables[4],
            false,
            (
                enableN[Frame::Channel::A] ||
                enableN[Frame::Channel::B] ||
                enableN[Frame::Channel::C]
            ),
            false
        };

        uint16_t color = (playing ? BG_DARK_MAGENTA | FG_CYAN : FG_CYAN);
        m_framesBuffer.color(color).draw('|');
        printRegisterValue(chip, frame, Register::Mixer, coloringM);

        m_framesBuffer.color(color).draw('|');
        printRegisterValue(chip, frame, Register::A_Coarse,  coloringA);
        printRegisterValue(chip, frame, Register::A_Fine,    coloringA);
        m_framesBuffer.draw(' ');
        printRegisterValue(chip, frame, Register::A_Volume,  coloringA);
        printRegisterValue(chip, frame, Register::A_Duty,    coloringA);
        m_framesBuffer.draw(' ');
        printRegisterValue(chip, frame, Register::EA_Coarse, coloringEA);
        printRegisterValue(chip, frame, Register::EA_Fine,   coloringEA);
        m_framesBuffer.draw(' ');
        printRegisterValue(chip, frame, Register::EA_Shape,  coloringEA);

        m_framesBuffer.color(color).draw('|');
        printRegisterValue(chip, frame, Register::B_Coarse,  coloringB);
        printRegisterValue(chip, frame, Register::B_Fine,    coloringB);
        m_framesBuffer.draw(' ');
        printRegisterValue(chip, frame, Register::B_Volume,  coloringB);
        printRegisterValue(chip, frame, Register::B_Duty,    coloringB);
        m_framesBuffer.draw(' ');
        printRegisterValue(chip, frame, Register::EB_Coarse, coloringEB);
        printRegisterValue(chip, frame, Register::EB_Fine,   coloringEB);
        m_framesBuffer.draw(' ');
        printRegisterValue(chip, frame, Register::EB_Shape,  coloringEB);

        m_framesBuffer.color(color).draw('|');
        printRegisterValue(chip, frame, Register::C_Coarse,  coloringC);
        printRegisterValue(chip, frame, Register::C_Fine,    coloringC);
        m_framesBuffer.draw(' ');
        printRegisterValue(chip, frame, Register::C_Volume,  coloringC);
        printRegisterValue(chip, frame, Register::C_Duty,    coloringC);
        m_framesBuffer.draw(' ');
        printRegisterValue(chip, frame, Register::EC_Coarse, coloringEC);
        printRegisterValue(chip, frame, Register::EC_Fine,   coloringEC);
        m_framesBuffer.draw(' ');
        printRegisterValue(chip, frame, Register::EC_Shape,  coloringEC);
        m_framesBuffer.color(color).draw('|');

        printRegisterValue(chip, frame, Register::N_AndMask, coloringN);
        printRegisterValue(chip, frame, Register::N_OrMask,  coloringN);
        printRegisterValue(chip, frame, Register::N_Period,  coloringN);
        m_framesBuffer.color(color).draw('|');
    }

    void printRegistersValues(int chip, const Frame& frame, bool playing, const Output::Enables& enables)
    {
        if (frame[chip].IsExpMode())
            printRegistersValuesForExpandedMode(chip, frame, playing, enables);
        else
            printRegistersValuesForCompatibleMode(chip, frame, playing, enables);
    }

    void printRegistersHeaderForMode(const std::string& str)
    {
        bool regIdx = false;
        for (size_t i = 0; i < str.length(); ++i)
        {
            if (str[i] == '|' || str[i] == ' ')
                m_framesBuffer.color(FG_CYAN);
            else
            {
                if (regIdx)
                    m_framesBuffer.color(FG_GREY);
                else
                    m_framesBuffer.color(FG_DARK_CYAN);
                regIdx ^= true;
            }
            m_framesBuffer.draw(str[i]);
        }
    }

    void printRegistersHeader(bool isExpMode)
    {
        if (isExpMode)
            printRegistersHeaderForMode(k_headerForExpMode);
        else
            printRegistersHeaderForMode(k_headerForComMode);
    }

    // Chooses how the frames fit the width: the chips side by side, the second
    // chip below the first one, only the first chip, or no frames at all.
    // A block of frames is shown only with the header and at least three
    // frames: the previous, the playing and the next one.
    FramesLayout getFramesLayout(const Stream& stream, int width, int rows)
    {
        FramesLayout layout{};
        int registers = int(stream.IsExpandedModeUsed() ? k_headerForExpMode.length() : k_headerForComMode.length());
        bool isTwoChips = stream.IsSecondChipUsed();
        int space = (width - 1);

        if (rows >= k_minFramesRows)
        {
            if (isTwoChips && space >= 6 + 2 * registers)
            {
                layout.chips = 2;
            }
            else if (space >= 6 + registers)
            {
                layout.chips = 1;
                layout.stacked = (isTwoChips && rows >= 2 * k_minFramesRows);
            }
        }

        if (layout.chips)
        {
            layout.width = (6 + layout.chips * registers);
            layout.meters = (space >= layout.width + 4);
            layout.rows = (layout.stacked ? rows / 2 * 2 : rows);
        }
        return layout;
    }

    void printFramesBlock(const Stream& stream, int frameId, const Output& output, int x, int y, int rows, int chip, int chips)
    {
        bool isExpMode = stream.IsExpandedModeUsed();
        int range = (rows - 2) / 2;

        // print header
        m_framesBuffer.position(x, y).color(FG_DARK_CYAN).draw("FRAME").move(1, 0);
        for (int i = 0; i < chips; ++i) printRegistersHeader(isExpMode);

        // prepare fake frame
        static Frame fakeFrame;
        if (fakeFrame[0].IsExpMode() != isExpMode || fakeFrame[1].IsExpMode() != isExpMode)
        {
            isExpMode ? fakeFrame[0].SetExpMode() : fakeFrame[0].ResetExpMode();
            isExpMode ? fakeFrame[1].SetExpMode() : fakeFrame[1].ResetExpMode();
            fakeFrame.ResetChanges();
        }

        // print frames
        for (int i = frameId - range, row = y + 1; row < y + rows; ++i, ++row)
        {
            bool highlight = (i == frameId);
            bool useFakeFrame = (i < 0 || i >= int(stream.play.framesCount));
            const Frame& frame = useFakeFrame ? fakeFrame : stream.play.GetFrame(i);

            // print frame number
            m_framesBuffer.position(x, row);
            m_framesBuffer.color(highlight ? BG_DARK_MAGENTA | FG_WHITE : FG_DARK_GREY);
            if (useFakeFrame)
                m_framesBuffer.draw(std::string(5, '.'));
            else
            {
                std::stringstream ss;
                ss << std::setfill('0') << std::setw(5) << i;
                m_framesBuffer.draw(ss.str());
            }
            m_framesBuffer.draw(' ');

            // print frame registers
            for (int c = chip; c < chip + chips; ++c)
            {
                printRegistersValues(c, frame, highlight, output.GetEnables());
            }
        }
    }

    void updateLevels(const Output& output)
    {
        // One column of the meters for each frame written to the output: the
        // trails move with the playback, not with the redraws, and stop at a
        // pause. Take the levels also when the meters are hidden.
        static uint32_t s_levelsCount = 0;
        static std::vector<Output::Levels> s_levels;
        output.GetLevels(s_levelsCount, s_levels);

        m_levels.insert(m_levels.end(), s_levels.begin(), s_levels.end());
        while (m_levels.size() > k_maxLevels) m_levels.pop_front();
    }

    void updateBands(const Output& output)
    {
        // the bars and the peaks also move with the written frames,
        // also when the analyzer is hidden
        static uint32_t s_bandsCount = 0;
        static std::vector<Output::Bands> s_bands;
        output.GetBands(s_bandsCount, s_bands);

        for (const Output::Bands& bands : s_bands)
        {
            for (int band = 0; band < LevelMeter::k_bands; ++band)
            {
                AnalyzerBand& shown = m_analyzer[band];
                shown.level = std::max(0, shown.level - k_barFall);
                if (bands.levels[band] >= shown.level)
                {
                    shown.level = bands.levels[band];
                    shown.sound = bands.sounds[band];
                }

                if (shown.level >= shown.peak)
                {
                    shown.peak = shown.level;
                    shown.hold = k_peakHold;
                }
                else if (shown.hold > 0)
                    --shown.hold;
                else
                    shown.peak = std::max(shown.level, shown.peak - k_peakFall);
            }
        }
    }

    // the semitones per column of the analyzer that fit the width, zero if none
    int getAnalyzerGroup(int width)
    {
        int space = (width - 1);
        for (int group = 1; group <= k_maxBandsPerColumn; ++group)
        {
            if (12 % group == 0 && LevelMeter::k_bands / group <= space) return group;
        }
        return 0;
    }

    void printAnalyzer(int width, int rows, int group)
    {
        // draw all of the analyzer in each frame, in the middle of the width;
        // the last row is the axis
        m_framesBuffer.resize(width, rows);

        int columns = (LevelMeter::k_bands / group);
        int x = (1 + (width - 1 - columns) / 2);
        int bars = (rows - 1);

        for (int column = 0; column < columns; ++column)
        {
            const AnalyzerBand* loudest = &m_analyzer[column * group];
            int peak = 0;
            for (int band = column * group; band < (column + 1) * group; ++band)
            {
                if (m_analyzer[band].level > loudest->level) loudest = &m_analyzer[band];
                peak = std::max(peak, m_analyzer[band].peak);
            }

            int bar = ((loudest->level * bars + 127) / 255);
            for (int y = 0; y < bar; ++y)
            {
                m_framesBuffer.position(x + column, bars - 1 - y).color(k_soundColors[loudest->sound]).draw(k_halfBlock);
            }

            // the peak, in half cells, shows only above the bar
            int halves = ((peak * 2 * bars + 127) / 255);
            int y = ((halves - 1) / 2);
            if (halves > 0 && y >= bar)
            {
                m_framesBuffer.position(x + column, bars - 1 - y).color(halves == 2 * bars ? FG_RED : FG_WHITE);
                m_framesBuffer.draw(halves & 1 ? k_lowerQuarter : k_upperQuarter);
            }
        }

        // A label is at the center of the column of its frequency. If a label
        // of 3 characters and a space do not fit in an octave, label every
        // second octave.
        m_framesBuffer.position(x, bars).color(FG_DARK_GREY).draw(std::string(columns, '-'));
        int step = (12 / group >= 4 ? 1 : 2);
        for (size_t i = 0; i < std::size(k_analyzerLabels); i += step)
        {
            const auto& label = k_analyzerLabels[i];
            int band = int(std::floor(12 * std::log2(label.first / LevelMeter::k_firstBandFreq) + 0.5));
            int length = int(std::strlen(label.second));
            int start = (band / group - (length - 1) / 2);
            if (start >= 0 && start + length <= columns)
            {
                m_framesBuffer.position(x + start, bars).color(FG_DARK_CYAN).draw(label.second);
            }
        }
    }

    // a bar of a trail, up and down from the middle, in half cells
    void printTrailBar(int x, int halves)
    {
        int half_h = (m_framesBuffer.h / 2);
        int last_y = (half_h - 1);

        for (int y = 0; y < half_h && 2 * y < halves; ++y)
        {
            SHORT colorU = (y == last_y ? FG_RED : (y == 0 ? FG_CYAN : FG_GREY));
            SHORT colorD = (y == last_y ? FG_DARK_RED : (y == 0 ? FG_DARK_CYAN : FG_DARK_GREY));
            bool whole = (2 * y + 1 < halves);

            m_framesBuffer.position(x, last_y - y).color(colorU).draw(whole ? k_halfBlock : k_lowerQuarter);
            m_framesBuffer.position(x, half_h + y).color(colorD).draw(whole ? k_halfBlock : k_upperQuarter);
        }
    }

    void printLevels(int offset, int dumpWidth)
    {
        int width  = m_framesBuffer.w;
        int halves = (m_framesBuffer.h / 2 * 2);

        // The trails go from the registers dump to the edges, the newest level
        // next to the dump. They come from the history in each frame, so a new
        // window size shows them fully at once.
        int xL = (offset - 2);
        int xR = (offset + dumpWidth + 1);
        int columns = std::min(int(m_levels.size()), std::max(xL, width - xR));

        for (int i = 0; i < columns; ++i)
        {
            const Output::Levels& levels = m_levels[m_levels.size() - 1 - i];
            if (xL - i >= 1) printTrailBar(xL - i, (levels.L * halves + 127) / 255);
            if (xR + i < width) printTrailBar(xR + i, (levels.R * halves + 127) / 255);
        }
    }

    void printFrames(const Stream& stream, int frameId, const Output& output, const FramesLayout& layout, int width)
    {
        // draw all of the block in each frame, the meters from the history
        m_framesBuffer.resize(width, layout.rows);

        int offset = 1 + (width - 1 - layout.width) / 2;
        if (layout.stacked)
        {
            int rows = (layout.rows / 2);
            printFramesBlock(stream, frameId, output, offset, 0, rows, 0, 1);
            printFramesBlock(stream, frameId, output, offset, rows, rows, 1, 1);
        }
        else
        {
            printFramesBlock(stream, frameId, output, offset, 0, layout.rows, 0, layout.chips);
        }

        if (layout.meters)
        {
            printLevels(offset, layout.width);
        }
    }

	////////////////////////////////////////////////////////////////////////////

    void DrawDecoding(const FileView& file, const Stream& stream)
    {
        if (isTimeToDraw(file))
        {
            drawFileView(file, getProperties(stream, nullptr), [](PrintBuffer& canvas, int y)
            {
                const auto t = std::chrono::system_clock::now();
                auto ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
                drawProgress(canvas, y, 0.5f, ms / 64, "Decoding");
            });
        }
    }

    void DrawPlayback(const FileView& file, const Stream& stream, int frameId, const Output& output, bool showFrames, bool showAnalyzer)
    {
        screen().Sync();
        updateLevels(output);
        updateBands(output);
        std::string outputName = output.toString();
        std::vector<Property> properties = getProperties(stream, &outputName);

        // The frames get the rows that the header, the properties and the
        // progress bar do not use. The analyzer replaces the frames and the
        // meters, uses the full width and needs the rows of a block of frames.
        int maxHeight = screen().GetMaxHeight();
        int propertiesRows = std::clamp(maxHeight - 2, 0, int(properties.size()));
        int framesRows = (showFrames ? std::min(k_maxFramesRows, maxHeight - 2 - propertiesRows) : 0);
        FramesLayout layout = getFramesLayout(stream, screen().GetWidth(), framesRows);
        int group = (showAnalyzer ? getAnalyzerGroup(screen().GetWidth()) : 0);
        int rows = (showAnalyzer ? (group && framesRows >= k_minFramesRows ? framesRows : 0) : layout.rows);

        PrintBuffer& canvas = screen().Begin(2 + propertiesRows + rows);
        drawFileHeader(canvas, 0, file);
        for (int i = 0; i < propertiesRows; ++i)
        {
            drawProperty(canvas, 1 + i, properties[i]);
        }

        if (rows)
        {
            if (showAnalyzer)
                printAnalyzer(canvas.w, rows, group);
            else
                printFrames(stream, frameId, output, layout, canvas.w);
            canvas.paste(m_framesBuffer, 0, 1 + propertiesRows);
        }

        int hh, mm, ss, ms;
        size_t playbackFrames = stream.play.framesCount;
        size_t remainingFrames = (playbackFrames - frameId);
        stream.ComputeDuration(remainingFrames, hh, mm, ss, ms);

        std::stringstream sstream;
        if (hh < 10) sstream << '0'; sstream << hh << ':';
        if (mm < 10) sstream << '0'; sstream << mm << ':';
        if (ss < 10) sstream << '0'; sstream << ss;
        float position = (playbackFrames ? float(frameId) / playbackFrames : 0.f);
        drawProgress(canvas, canvas.h - 1, position, (1000 - ms) / 64, sstream.str());

        screen().Render();
    }

    void DrawEncoding(const FileView& file, const Stream& stream, int frameId, size_t framesCount, const std::string& output)
    {
        if (isTimeToDraw(file))
        {
            drawFileView(file, getProperties(stream, &output), [&](PrintBuffer& canvas, int y)
            {
                float position = (framesCount ? float(frameId) / framesCount : 0.f);
                drawProgress(canvas, y, position, frameId / 256, "Encoding");
            });
        }
    }

    void CommitFile(const FileView& file, const Stream& stream, const std::string& output)
    {
        screen().Erase();
        screen().Print([file](PrintBuffer& canvas, int y) { drawFileHeader(canvas, y, file); });

        for (const Property& property : getProperties(stream, &output))
        {
            if (screen().IsActive())
            {
                // the value is cut to the window width, which may change later
                screen().Print([property](PrintBuffer& canvas, int y) { drawProperty(canvas, y, property); });
            }
            else
            {
                // the output is redirected: plain text, the values are not cut
                std::string dots(9 - property.label.length(), '.');
                screen().Print(' ' + property.label + dots + ": " + property.value);
            }
        }
    }

    void EraseFile()
    {
        screen().Erase();
    }
}
