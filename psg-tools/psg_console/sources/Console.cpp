#include <algorithm>
#include <iterator>

#include "Console.h"

namespace console
{
    namespace
    {
        // Windows 7 conhost fails console calls that move more than 64 KB
        // of data, so cells are written in chunks well below that limit.
        const int k_maxCellsPerWrite = 8000;

        const WORD k_plainText = (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        const WORD k_backgroundMask = (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY);

        // ENABLE_VIRTUAL_TERMINAL_PROCESSING, which Windows 7 does not know
        const DWORD k_virtualTerminal = 0x0004;

        HANDLE GetOutput()
        {
            return GetStdHandle(STD_OUTPUT_HANDLE);
        }

        bool GetInfo(CONSOLE_SCREEN_BUFFER_INFO& info)
        {
            HANDLE handle = GetOutput();
            return (handle != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(handle, &info));
        }

        void WriteText(HANDLE handle, const std::wstring& text)
        {
            DWORD written;
            WriteConsoleW(handle, text.data(), DWORD(text.size()), &written, nullptr);
        }

        void WriteText(HANDLE handle, const std::string& text)
        {
            int size = MultiByteToWideChar(CP_ACP, 0, text.data(), int(text.size()), nullptr, 0);
            std::wstring wide(size, L'\0');
            MultiByteToWideChar(CP_ACP, 0, text.data(), int(text.size()), wide.data(), size);
            WriteText(handle, wide);
        }
    }

    bool IsConsole()
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        return GetInfo(info);
    }

    WORD MapAttributes(WORD attributes)
    {
        // the console colors are taken at the first call, before the program
        // has changed anything
        static WORD s_defaultAttributes = 0;
        static bool s_isDefaultKnown = false;
        if (!s_isDefaultKnown)
        {
            CONSOLE_SCREEN_BUFFER_INFO info;
            s_defaultAttributes = (GetInfo(info) ? info.wAttributes : k_plainText);
            s_isDefaultKnown = true;
        }

        if (attributes == k_plainText) return s_defaultAttributes;
        if (!(attributes & k_backgroundMask)) attributes |= (s_defaultAttributes & k_backgroundMask);
        return attributes;
    }

    HANDLE CreateScreen()
    {
        HANDLE screen = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CONSOLE_TEXTMODE_BUFFER, nullptr);
        if (screen == INVALID_HANDLE_VALUE) return nullptr;

        CONSOLE_CURSOR_INFO cursor = { 1, FALSE };
        SetConsoleCursorInfo(screen, &cursor);
        SetConsoleTextAttribute(screen, MapAttributes(k_plainText));
        if (!SetConsoleActiveScreenBuffer(screen))
        {
            CloseHandle(screen);
            return nullptr;
        }

        // Under ConPTY a buffer of its own is drawn on the main screen of the
        // terminal, which reflows it on resize and pushes the rows into its
        // scrollback. Where the console takes VT sequences (Windows 10 and
        // later), the buffer also asks the terminal for its alternate screen.
        // It is still a buffer of its own: if the process gets killed, the
        // console closes it, and the terminal gets its main screen back.
        DWORD mode = 0;
        if (GetConsoleMode(screen, &mode) && SetConsoleMode(screen, mode | k_virtualTerminal))
        {
            WriteText(screen, std::wstring(L"\x1b[?1049h"));
        }
        return screen;
    }

    void CloseScreen(HANDLE screen)
    {
        DWORD mode = 0;
        if (GetConsoleMode(screen, &mode) && (mode & k_virtualTerminal))
        {
            WriteText(screen, std::wstring(L"\x1b[?1049l"));
        }
        SetConsoleActiveScreenBuffer(GetOutput());
        CloseHandle(screen);
    }

    COORD FitScreen(HANDLE screen)
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetConsoleScreenBufferInfo(screen, &info)) return { 0, 0 };

        // the window is moved to the top left corner first, so the buffer can
        // be cut down to it; under ConPTY both already match the terminal
        const SMALL_RECT& window = info.srWindow;
        SHORT w = SHORT(window.Right - window.Left + 1);
        SHORT h = SHORT(window.Bottom - window.Top + 1);
        if (info.dwSize.X != w || info.dwSize.Y != h || window.Left != 0 || window.Top != 0)
        {
            SMALL_RECT origin = { 0, 0, SHORT(w - 1), SHORT(h - 1) };
            SetConsoleWindowInfo(screen, TRUE, &origin);
            SetConsoleScreenBufferSize(screen, { w, h });
        }
        return { w, h };
    }

    void WriteCells(HANDLE screen, const CHAR_INFO* cells, int width, int rows, int y)
    {
        if (width <= 0) return;
        int chunk = std::max(1, k_maxCellsPerWrite / width);

        for (int row = 0; row < rows; row += chunk)
        {
            int count = std::min(chunk, rows - row);
            SMALL_RECT rect = { 0, SHORT(y + row), SHORT(width - 1), SHORT(y + row + count - 1) };
            WriteConsoleOutputW(screen, cells + row * width, { SHORT(width), SHORT(count) }, { 0, 0 }, &rect);
        }
    }

    int GetWidth()
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        return (GetInfo(info) ? std::min(info.srWindow.Right - info.srWindow.Left + 1, int(info.dwSize.X)) : 0);
    }

    void WriteLine(const std::string& text, WORD attributes)
    {
        HANDLE handle = GetOutput();
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetInfo(info))
        {
            if (handle != INVALID_HANDLE_VALUE)
            {
                std::string line = (text + "\r\n");
                DWORD written;
                WriteFile(handle, line.data(), DWORD(line.size()), &written, nullptr);
            }
            return;
        }

        // the new line goes out in the original attributes, otherwise the row
        // scrolled in by it would be filled with the text background
        SetConsoleTextAttribute(handle, MapAttributes(attributes));
        WriteText(handle, text);
        SetConsoleTextAttribute(handle, info.wAttributes);
        WriteText(handle, std::wstring(L"\n"));
    }

    void WriteRow(const CHAR_INFO* cells, int count)
    {
        HANDLE handle = GetOutput();
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetInfo(info)) return;

        while (count > 0 && cells[count - 1].Char.UnicodeChar == L' ') --count;
        for (int start = 0; start < count;)
        {
            int end = start;
            std::wstring text;
            while (end < count && cells[end].Attributes == cells[start].Attributes)
            {
                text += cells[end++].Char.UnicodeChar;
            }
            SetConsoleTextAttribute(handle, MapAttributes(cells[start].Attributes));
            WriteText(handle, text);
            start = end;
        }
        SetConsoleTextAttribute(handle, info.wAttributes);
        WriteText(handle, std::wstring(L"\n"));
    }

    DWORD s_inputMode = 0;
    bool s_isInputModeSaved = false;

    void SetupInput()
    {
        // quick edit mode can only be changed together with ENABLE_EXTENDED_FLAGS;
        // with the mouse input the terminal sends the wheel to the program
        // instead of turning it into the arrow keys on its alternate screen
        HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
        if (!s_isInputModeSaved && GetConsoleMode(handle, &s_inputMode))
        {
            s_isInputModeSaved = true;
            SetConsoleMode(handle, (s_inputMode | ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT) & ~ENABLE_QUICK_EDIT_MODE);
        }
    }

    void RestoreInput()
    {
        HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
        if (s_isInputModeSaved)
        {
            FlushConsoleInputBuffer(handle);
            SetConsoleMode(handle, s_inputMode | ENABLE_EXTENDED_FLAGS);
            s_isInputModeSaved = false;
        }
    }

    std::wstring s_title;
    bool s_isTitleSaved = false;

    void SetTitle(const std::wstring& title)
    {
        if (!s_isTitleSaved)
        {
            wchar_t buffer[1024]{};
            GetConsoleTitleW(buffer, DWORD(std::size(buffer)));
            s_title = buffer;
            s_isTitleSaved = true;
        }
        SetConsoleTitleW(title.c_str());
    }

    void RestoreTitle()
    {
        if (s_isTitleSaved)
        {
            SetConsoleTitleW(s_title.c_str());
            s_isTitleSaved = false;
        }
    }
}
