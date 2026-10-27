#include <algorithm>

#include "Console.h"

namespace console
{
    namespace
    {
        // Windows 7 conhost fails console calls that move more than 64 KB
        // of data, so cells are written in chunks well below that limit.
        const int k_maxCellsPerWrite = 8000;

        HANDLE GetOutput()
        {
            return GetStdHandle(STD_OUTPUT_HANDLE);
        }

        bool GetInfo(CONSOLE_SCREEN_BUFFER_INFO& info)
        {
            HANDLE handle = GetOutput();
            return (handle != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(handle, &info));
        }

        void WriteText(HANDLE handle, const std::string& text)
        {
            int size = MultiByteToWideChar(CP_ACP, 0, text.data(), int(text.size()), nullptr, 0);
            std::wstring wide(size, L'\0');
            MultiByteToWideChar(CP_ACP, 0, text.data(), int(text.size()), wide.data(), size);

            DWORD written;
            WriteConsoleW(handle, wide.data(), DWORD(wide.size()), &written, nullptr);
        }
    }

    bool GetGeometry(Geometry& geometry)
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetInfo(info)) return false;

        const SMALL_RECT& window = info.srWindow;
        int windowWidth = (window.Right - window.Left + 1);

        // the last column stays empty: a full-width row is treated as wrapped
        // and gets joined with the next row when the terminal reflows its text
        geometry.width = std::max(0, std::min(windowWidth, int(info.dwSize.X)) - 1);
        geometry.height = (window.Bottom - window.Top + 1);
        geometry.windowTop = window.Top;
        geometry.windowBottom = window.Bottom;
        geometry.bufferHeight = info.dwSize.Y;
        geometry.cursorY = info.dwCursorPosition.Y;
        return true;
    }

    void ShowCursor(bool show)
    {
        HANDLE handle = GetOutput();
        CONSOLE_CURSOR_INFO cursor;
        if (GetConsoleCursorInfo(handle, &cursor) && bool(cursor.bVisible) != show)
        {
            cursor.bVisible = show;
            SetConsoleCursorInfo(handle, &cursor);
        }
    }

    void SetCursorRow(int y)
    {
        SetConsoleCursorPosition(GetOutput(), { 0, SHORT(y) });
    }

    void WriteCells(const CHAR_INFO* cells, int width, int rows, int y)
    {
        if (width <= 0) return;
        int chunk = std::max(1, k_maxCellsPerWrite / width);

        for (int row = 0; row < rows; row += chunk)
        {
            int count = std::min(chunk, rows - row);
            SMALL_RECT rect = { 0, SHORT(y + row), SHORT(width - 1), SHORT(y + row + count - 1) };
            WriteConsoleOutputW(GetOutput(), cells + row * width, { SHORT(width), SHORT(count) }, { 0, 0 }, &rect);
        }
    }

    bool IsTextAt(int y, const CHAR_INFO* cells, int count)
    {
        DWORD read = 0;
        std::wstring text(count, L' ');
        ReadConsoleOutputCharacterW(GetOutput(), text.data(), DWORD(count), { 0, SHORT(y) }, &read);
        if (int(read) != count) return false;

        for (int i = 0; i < count; ++i)
        {
            if (text[i] != cells[i].Char.UnicodeChar) return false;
        }
        return true;
    }

    void ClearRows(int y, int rows)
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (rows <= 0 || !GetInfo(info)) return;

        HANDLE handle = GetOutput();
        DWORD count = (DWORD(info.dwSize.X) * rows), written;
        FillConsoleOutputCharacterW(handle, L' ', count, { 0, SHORT(y) }, &written);
        FillConsoleOutputAttribute(handle, info.wAttributes, count, { 0, SHORT(y) }, &written);
    }

    int WriteNewLines(int count)
    {
        WriteText(GetOutput(), std::string(count, '\n'));

        CONSOLE_SCREEN_BUFFER_INFO info;
        return (GetInfo(info) ? info.dwCursorPosition.Y : 0);
    }

    void WriteLine(const std::string& text, WORD attributes)
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetInfo(info)) return;

        // the new line goes out in the original attributes, otherwise the row
        // scrolled in by it would be filled with the text background
        HANDLE handle = GetOutput();
        SetConsoleTextAttribute(handle, attributes);
        WriteText(handle, text);
        SetConsoleTextAttribute(handle, info.wAttributes);
        WriteText(handle, "\n");
    }

    void DisableQuickEdit()
    {
        // quick edit mode can only be changed together with ENABLE_EXTENDED_FLAGS
        HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode;
        if (GetConsoleMode(handle, &mode))
        {
            SetConsoleMode(handle, (mode | ENABLE_EXTENDED_FLAGS) & ~ENABLE_QUICK_EDIT_MODE);
        }
    }
}
