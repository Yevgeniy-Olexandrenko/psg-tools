#include <iomanip>
#include <sstream>
#include <array>

#include "ConsoleGUI.h"
#include "PrintBuffer.h"

namespace gui
{
    using namespace terminal;

	PrintBuffer   m_framesBuffer(k_consoleWidth, 16);
    KeyboardInput m_keyboardInput;

    bool SetConsoleSize(SHORT x, SHORT y)
    {
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (handle == INVALID_HANDLE_VALUE) return false;

        // If either dimension is greater than the largest console window we can have,
        // there is no point in attempting the change.
        COORD largestSize = GetLargestConsoleWindowSize(handle);
        if (x > largestSize.X) return false;
        if (y > largestSize.Y) return false;

        CONSOLE_SCREEN_BUFFER_INFO bufferInfo;
        if (!GetConsoleScreenBufferInfo(handle, &bufferInfo)) return false;

        SMALL_RECT& winInfo = bufferInfo.srWindow;
        COORD windowSize =
        {
            winInfo.Right - winInfo.Left + 1,
            winInfo.Bottom - winInfo.Top + 1
        };

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

            if (!SetConsoleWindowInfo(handle, TRUE, &info)) return false;
        }

        COORD size = { x, y };
        if (!SetConsoleScreenBufferSize(handle, size)) return false;

        SMALL_RECT info = { 0, 0, x - 1, y - 1 };
        if (!SetConsoleWindowInfo(handle, TRUE, &info)) return false;

        return true;
    }

    void Init(const std::wstring& title)
    {
        // disable console window resize
        HWND consoleWindow = GetConsoleWindow();
        SetWindowLong(consoleWindow, GWL_STYLE, GetWindowLong(consoleWindow, GWL_STYLE) & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX);

        // disable edit mode in console
        DWORD mode;
        HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
        GetConsoleMode(handle, &mode);
        SetConsoleMode(handle, mode & ~ENABLE_QUICK_EDIT_MODE);

        // set console buffer and window size
        SetConsoleTitle(title.c_str());
        SetConsoleSize(k_consoleWidth, k_consoleHeight);
    }

	void Update()
	{
        cursor::show(false);
		m_keyboardInput.Update();
	}

    void Clear(size_t height)
    {
        Clear(-int(height), height);
    }

    void Clear(int offset, size_t height)
	{
        if (offset < 0) cursor::move_up(-offset);
        else cursor::move_down(offset);

        for (size_t i = 0; i < height; ++i)
        {
            cursor::erase_line();
            cursor::move_down(1);
        }
        cursor::move_up(int(height));
	}

	const KeyState& GetKeyState(int key)
	{
		return m_keyboardInput.GetKeyState(key);
	}

    ////////////////////////////////////////////////////////////////////////////

    void trim(std::string& str)
    {
        str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](int c) { return !std::isspace(c); }));
        str.erase(std::find_if(str.rbegin(), str.rend(), [](int c) { return !std::isspace(c); }).base(), str.end());
    };

    bool limit(std::string& str, size_t add = 0)
    {
        size_t len = str.length();
        size_t max = terminal_width() - 2;
        if (len + add > max)
        {
            size_t reduce = (len + add - max + 3 + 2) / 2;
            str = str.substr(0, len / 2 - reduce) + " ~ " + str.substr(len / 2 + reduce, len);
            return true;
        }
        return false;
    }

    ////////////////////////////////////////////////////////////////////////////

	size_t PrintInputFile(const std::filesystem::path& path, int index, int amount, bool isFavorite)
	{
        cursor::show(false);
        std::string numberStr = std::to_string(index + 1);
        std::string totalStr  = std::to_string(amount);
        std::string fileName  = path.stem().string();
        std::string fileExt   = path.extension().string();

        size_t numLen = (numberStr.length() + 1 + totalStr.length());
        size_t strLen = (numLen + fileName.length() + fileExt.length());
        if (limit(fileName, 7 + numLen + fileExt.length()))
            strLen = (numLen + fileName.length() + fileExt.length());
        size_t delLen = (terminal_width() - 2 - 7 - strLen);

        std::cout << ' ' << color::bright_blue << std::string(delLen, '-');
        std::cout << color::bright_cyan << "[ ";
        std::cout << color::bright_yellow << numberStr << color::bright_cyan << '/' << color::bright_yellow << totalStr;
        std::cout << ' ' << (isFavorite ? color::bright_magenta : color::bright_white) << fileName;
        std::cout << color::bright_grey << fileExt;
        std::cout << color::bright_cyan << " ]";
        std::cout << color::bright_blue << "--";
        std::cout << color::reset << std::endl;
        return 1;
	}

    ////////////////////////////////////////////////////////////////////////////

    void printPropertyLabel(const std::string& label)
    {
        std::cout << ' ' << color::bright_yellow << label;
        std::cout << color::bright_grey << std::string(9 - label.length(), '.');
        std::cout << color::bright_magenta << ": ";
    };

    size_t printModuleProperty(const std::string& label, const Stream& stream, Stream::Property property)
    {
        std::string str = stream.ToString(property);
        trim(str);

        if (!str.empty())
        {
            limit(str, 9);
            printPropertyLabel(label);
            if (property == Stream::Property::Title)
                std::cout << color::bright_green;
            else
                std::cout << color::bright_white;
            std::cout << str << color::reset << std::endl;
            return 1;
        }
        return 0;
    };

    size_t printOutputProperty(const std::string& label, const std::string& output)
    {
        std::string str = output;
        trim(str);

        if (!str.empty())
        {
            printPropertyLabel(label);
            std::cout << color::bright_cyan;
            std::cout << str << color::reset << std::endl;
            return 1;
        }
        return 0;
    };

    size_t PrintBriefStreamInfo(const Stream& stream)
    {
        size_t height = 0;
        height += printModuleProperty("Title", stream, Stream::Property::Title);
        height += printModuleProperty("Artist", stream, Stream::Property::Artist);
        height += printModuleProperty("Comment", stream, Stream::Property::Comment);
        height += printModuleProperty("Type", stream, Stream::Property::Type);
        height += printModuleProperty("Chip", stream, Stream::Property::Chip);
        return height;
    }

	size_t PrintFullStreamInfo(const Stream& stream, const std::string& output)
	{
        size_t height = 0;
        height += printModuleProperty("Title", stream, Stream::Property::Title);
        height += printModuleProperty("Artist", stream, Stream::Property::Artist);
        height += printModuleProperty("Comment", stream, Stream::Property::Comment);
        height += printModuleProperty("Type", stream, Stream::Property::Type);
        height += printModuleProperty("Chip", stream, Stream::Property::Chip);
        height += printModuleProperty("Frames", stream, Stream::Property::Frames);
        height += printModuleProperty("Duration", stream, Stream::Property::Duration);
        height += printOutputProperty("Output", output);
        return height;
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

	size_t PrintStreamFrames(const Stream& stream, int frameId, const Output& output)
	{
        int height = int(m_framesBuffer.h);
        int width  = int(m_framesBuffer.w - 2);
        int range1 = (height - 2) / 2;
        int range2 = (height - 2) - range1;
        bool isTwoChips = stream.IsSecondChipUsed();
        bool isExpMode  = stream.IsExpandedModeUsed();

        // prepare console for drawing
        if (frameId == 0) { m_framesBuffer.clear(); cursor::show(false); }
        for (int i = 0; i < height; ++i) std::cout << std::endl;
        terminal::cursor::move_up(height);

        // print header
        int regs_w = int(isExpMode ? k_headerForExpMode.length() : k_headerForComMode.length());
        int dump_w = 6 + (isTwoChips ? 2 * regs_w : regs_w);
        int offset = 1 + (width - dump_w) / 2;
        m_framesBuffer.position(offset, 0).color(FG_DARK_CYAN).draw("FRAME").move(1, 0);
        printRegistersHeader(isExpMode);
        if (isTwoChips) printRegistersHeader(isExpMode);

        // prepare fake frame
        static Frame fakeFrame;
        if (fakeFrame[0].IsExpMode() != isExpMode || fakeFrame[1].IsExpMode() != isExpMode)
        {
            fakeFrame[0].SetExpMode(isExpMode);
            fakeFrame[1].SetExpMode(isExpMode);
            fakeFrame.ResetChanges();
        }

        // print frames
        for (int i = int(frameId - range1), y = 1; i <= int(frameId + range2); ++i, ++y)
        {
            bool highlight = (i == frameId);
            bool useFakeFrame = (i < 0 || i >= int(stream.play.framesCount()));
            const Frame& frame = useFakeFrame ? fakeFrame : stream.play.GetFrame(i);

            // print frame number
            m_framesBuffer.position(offset, y);
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
            printRegistersValues(0, frame, highlight, output.GetEnables());
            if (isTwoChips) printRegistersValues(1, frame, highlight, output.GetEnables());
        }

        // print levels
        float levelL, levelR;
        output.GetLevels(levelL, levelR);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 1; x <= offset - 3; ++x)
            {
                int i = (y * m_framesBuffer.w + x);
                m_framesBuffer.buffer[i] = m_framesBuffer.buffer[i + 1];
            }
            for (int x = width; x >= offset + dump_w + 2; --x)
            {
                int i = (y * m_framesBuffer.w + x);
                m_framesBuffer.buffer[i] = m_framesBuffer.buffer[i - 1];
            }
        }

        int half_h = (height / 2);
        int last_y = (half_h - 1);
        for (int y = 0; y < half_h; ++y)
        {
            wchar_t charL = (y < int(levelL* half_h + 0.5f) ? '|' : ' ');
            wchar_t charR = (y < int(levelR* half_h + 0.5f) ? '|' : ' ');

            SHORT colorU = (y == last_y ? FG_RED : (y == 0 ? FG_CYAN : FG_GREY));
            SHORT colorD = (y == last_y ? FG_DARK_RED : (y == 0 ? FG_DARK_CYAN : FG_DARK_GREY));
            
            m_framesBuffer.position(offset - 2, last_y - y).color(colorU).draw(charL);
            m_framesBuffer.position(offset - 2, half_h + y).color(colorD).draw(charL);

            m_framesBuffer.position(offset + dump_w + 1, last_y - y).color(colorU).draw(charR);
            m_framesBuffer.position(offset + dump_w + 1, half_h + y).color(colorD).draw(charR);
        }

        m_framesBuffer.render();
        cursor::move_down(height);
        return height;
	}

	////////////////////////////////////////////////////////////////////////////

    size_t PrintProgress(int index, size_t count, int spinner, const std::string& value)
    {
        static int s_spin = -1;
        int spin = (spinner & 3);

        if (spin != s_spin)
        {
            static const char c_spinner[] = R"(_\|/)";
            s_spin = spin;

            auto range = size_t(terminal_width() - 1 - 2 - 2 - 2 - value.length() - 2 - 2 - 1);
            auto size1 = size_t(float(index * range) / count + 0.5f);
            auto size2 = size_t(range - size1);

            std::cout << ' ' << color::bright_blue << std::string(2 + size1, '-');
            std::cout << color::bright_cyan << "[ ";
            std::cout << color::bright_magenta << c_spinner[spin] << ' ';
            std::cout << color::bright_white << value;
            std::cout << color::bright_cyan << " ]";
            std::cout << color::bright_blue << std::string(size2 + 2, '-');
            std::cout << color::reset << std::endl;
        }
        else
        {
            cursor::move_down(1);
        }
        return 1;
    }

    size_t PrintDecodingProgress(const Stream& stream)
    {
        const auto t = std::chrono::system_clock::now();
        auto ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
        return PrintProgress(1, 2, ms / 64, "Decoding");
    }

	size_t PrintPlaybackProgress(const Stream& stream, int frameId)
	{
        int hh, mm, ss, ms;
        size_t playbackFrames  = stream.play.framesCount();
        size_t remainingFrames = (playbackFrames - frameId);
        stream.ComputeDuration(remainingFrames, hh, mm, ss, ms);

        std::stringstream sstream;
        if (hh < 10) sstream << '0'; sstream << hh << ':';
        if (mm < 10) sstream << '0'; sstream << mm << ':';
        if (ss < 10) sstream << '0'; sstream << ss;
        return PrintProgress(frameId, playbackFrames, (1000 - ms) / 64, sstream.str());
	}

    size_t PrintEncodingProgress(const Stream& stream, int frameId)
    {
        auto frames = stream.framesCount();
        return PrintProgress(frameId, frames, frameId / 256, "Encoding");
    }
}
