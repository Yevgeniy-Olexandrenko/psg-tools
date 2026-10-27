#include <algorithm>

#include "PrintBuffer.h"

PrintBuffer::PrintBuffer(int w, int h)
    : x(0), y(0)
    , w(w), h(h)
    , buffer(new CHAR_INFO[w * h])
    , col(FG_WHITE)
{
    clear();
}

PrintBuffer::~PrintBuffer()
{
    if (buffer)
    {
        delete[] buffer;
        buffer = nullptr;
    }
}

void PrintBuffer::resize(int w, int h)
{
    if (w * h != this->w * this->h)
    {
        delete[] buffer;
        buffer = new CHAR_INFO[w * h];
    }
    this->w = w;
    this->h = h;
    clear();
}

void PrintBuffer::clear()
{
    CHAR_INFO blank;
    blank.Char.UnicodeChar = L' ';
    blank.Attributes = FG_GREY;
    std::fill(buffer, buffer + w * h, blank);
}

void PrintBuffer::render()
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) return;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(handle, &csbi);

    SHORT cx = csbi.dwCursorPosition.X;
    SHORT cy = csbi.dwCursorPosition.Y;

    SMALL_RECT rect = { cx, cy, cx + w - 1, cy + h - 1 };
    WriteConsoleOutput(handle, buffer, { w, h }, { 0,0 }, &rect);
}

PrintBuffer& PrintBuffer::position(int x, int y)
{
    this->x = x;
    this->y = y;
    return *this;
}

PrintBuffer& PrintBuffer::move(int dx, int dy)
{
    this->x += dx;
    this->y += dy;
    return *this;
}

PrintBuffer& PrintBuffer::color(SHORT color)
{
    this->col = color;
    return *this;
}

PrintBuffer& PrintBuffer::draw(std::wstring s)
{
    for (wchar_t c : s) draw(c);
    return *this;
}

PrintBuffer& PrintBuffer::draw(std::string s)
{
    for (char c : s) draw(c);
    return *this;
}

PrintBuffer& PrintBuffer::draw(wchar_t c)
{
    // characters outside of the buffer are skipped, but still advance
    // the position, so text that starts off the left edge stays in place
    if (x >= 0 && x < w && y >= 0 && y < h)
    {
        CHAR_INFO& cell = buffer[y * w + x];
        cell.Char.UnicodeChar = c;
        cell.Attributes = col;
    }
    x++;
    return *this;
}

PrintBuffer& PrintBuffer::draw(char c)
{
    return draw(wchar_t((unsigned char)c));
}

PrintBuffer& PrintBuffer::paste(const PrintBuffer& other, int x, int y)
{
    for (int row = 0; row < other.h; ++row)
    {
        for (int col = 0; col < other.w; ++col)
        {
            int tx = (x + col), ty = (y + row);
            if (tx >= 0 && tx < w && ty >= 0 && ty < h)
            {
                buffer[ty * w + tx] = other.buffer[row * other.w + col];
            }
        }
    }
    return *this;
}
