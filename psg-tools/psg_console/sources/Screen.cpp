#include <algorithm>
#include <cstring>

#include "Screen.h"
#include "Console.h"

namespace
{
    // width of the rows written when the output is redirected
    const int k_textWidth = 79;

    CHAR_INFO MakeBlank()
    {
        CHAR_INFO blank;
        blank.Char.UnicodeChar = L' ';
        blank.Attributes = FG_GREY;
        return blank;
    }

    const CHAR_INFO k_blank = MakeBlank();

    // the text as the console shows it: in the ANSI code page, with the tabs
    // turned into spaces up to the next multiple of 8 columns
    std::wstring ToScreenText(const std::string& text)
    {
        int size = MultiByteToWideChar(CP_ACP, 0, text.data(), int(text.size()), nullptr, 0);
        std::wstring wide(size, L'\0');
        MultiByteToWideChar(CP_ACP, 0, text.data(), int(text.size()), wide.data(), size);

        std::wstring result;
        for (wchar_t c : wide)
        {
            if (c == L'\t')
                result.append(8 - result.size() % 8, L' ');
            else
                result += c;
        }
        return result;
    }

    // the characters of the row as text in the ANSI code page
    std::string ToText(const PrintBuffer& row)
    {
        std::wstring text;
        for (int x = 0; x < row.w; ++x) text += row.buffer[x].Char.UnicodeChar;
        text.erase(text.find_last_not_of(L' ') + 1);

        int size = WideCharToMultiByte(CP_ACP, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(size, '\0');
        WideCharToMultiByte(CP_ACP, 0, text.data(), int(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }
}

Screen::Screen()
    : m_handle(nullptr)
    , m_width(0)
    , m_height(0)
    , m_scroll(0)
    , m_total(0)
    , m_live(0, 0)
    , m_liveHeight(0)
    , m_isLivePinned(true)
{
    // takes the console colors before anything is drawn
    console::MapAttributes(k_blank.Attributes);
    if (console::IsConsole())
    {
        m_handle = console::CreateScreen();
        Sync();
    }
}

Screen::~Screen()
{
    Close();
}

bool Screen::Sync()
{
    if (!IsActive()) return false;
    COORD size = console::FitScreen(m_handle);
    if (size.X == m_width && size.Y == m_height) return false;

    m_width = size.X;
    m_height = size.Y;
    m_shown.clear();
    return true;
}

PrintBuffer& Screen::Begin(int height)
{
    m_live.resize(GetWidth(), std::clamp(height, 0, m_height));
    m_live.position(0, 0).color(FG_GREY);
    return m_live;
}

void Screen::Render()
{
    m_liveHeight = m_live.h;
    Present();
}

void Screen::SetLivePinned(bool pinned)
{
    m_isLivePinned = pinned;
    Present();
}

void Screen::Erase()
{
    m_liveHeight = 0;
    Present();
}

void Screen::Print(const std::string& text, WORD attributes)
{
    if (!IsActive())
    {
        console::WriteLine(text, attributes);
        return;
    }
    Add({ text, attributes, nullptr });
}

void Screen::Print(const DrawRow& draw)
{
    if (!IsActive())
    {
        PrintBuffer row(k_textWidth, 1);
        draw(row, 0);
        console::WriteLine(ToText(row), FG_GREY);
        return;
    }
    Add({ std::string(), FG_GREY, draw });
}

void Screen::Update(const KeyboardInput& keyboard)
{
    if (!IsActive()) return;

    // a page is the history rows on screen, less the bar of a scrolled history;
    // End goes to the end of the history
    int live = (m_live.w == GetWidth() ? std::min(m_liveHeight, m_height) : 0);
    int page = std::max(1, (m_isLivePinned ? m_height - live : m_height) - 1);

    if (keyboard.GetKeyState(VK_END).pressed && m_scroll > 0)
    {
        m_scroll = 0;
        Present();
        return;
    }

    int rows = (3 * keyboard.GetWheel());
    if (keyboard.GetKeyState(VK_PRIOR).pressed) rows += page;
    if (keyboard.GetKeyState(VK_NEXT).pressed) rows -= page;
    Scroll(rows);
}

void Screen::Close()
{
    if (!IsActive()) return;
    console::CloseScreen(m_handle);
    m_handle = nullptr;

    // the last column stays empty: a full row is taken for a wrapped one,
    // and the terminal would join it with the next row on resize
    PrintBuffer row(std::max(1, console::GetWidth() - 1), 1);
    for (const Entry& entry : m_history)
    {
        if (entry.draw)
        {
            row.clear();
            entry.draw(row, 0);
            console::WriteRow(row.buffer, row.w);
        }
        else
        {
            console::WriteLine(entry.text, entry.attributes);
        }
    }
    m_history.clear();
}


void Screen::DrawEntry(const Entry& entry, PrintBuffer& rows) const
{
    if (entry.draw)
    {
        entry.draw(rows, 0);
        return;
    }

    // the text is cut at the window edge, the whole of it goes to the output
    // on Close()
    rows.position(0, 0).color(entry.attributes).draw(ToScreenText(entry.text));
}

void Screen::Add(Entry entry)
{
    m_history.push_back(std::move(entry));
    Present();
}

void Screen::Scroll(int rows)
{
    if (rows != 0)
    {
        m_scroll += rows;
        Present();
    }
}

void Screen::Present()
{
    if (!IsActive()) return;
    Sync();

    const int width = m_width, height = m_height;
    if (width <= 0 || height <= 0) return;

    // everything is drawn without the last column, so there is a one column
    // margin at the right edge, as there is at the left one
    const int canvas = GetWidth();

    // the live view is left out until it is drawn for the new width. A pinned
    // one is at the bottom of the window, below the history, otherwise it is
    // the last rows of the history and scrolls with it.
    int live = (m_live.w == canvas ? std::min(m_liveHeight, height) : 0);
    int history = int(m_history.size());
    int room = (m_isLivePinned ? height - live : height);
    int total = (m_isLivePinned ? history : history + live);

    // the view of a scrolled history stays where it is while the rows are
    // added below it or the live view changes its height; it never gets to
    // the end by itself, so it does not start to follow the new rows
    if (m_scroll > 0) m_scroll = std::max(1, m_scroll + total - m_total);
    m_total = total;
    m_scroll = std::clamp(m_scroll, 0, std::max(0, total - room));

    // the history fills the window from the top; only the rows that are seen
    // get drawn
    int shown = std::min(room, total);
    int first = (total - m_scroll - shown);
    m_frame.assign(size_t(width) * height, k_blank);

    PrintBuffer row(canvas, 1);
    for (int y = 0; y < shown; ++y)
    {
        CHAR_INFO* cells = &m_frame[size_t(y) * width];
        int index = (first + y);
        if (index >= history)
        {
            const CHAR_INFO* liveRow = (m_live.buffer + size_t(index - history) * canvas);
            std::copy(liveRow, liveRow + canvas, cells);
            continue;
        }
        row.clear();
        DrawEntry(m_history[index], row);
        std::copy(row.buffer, row.buffer + canvas, cells);
    }

    // a scrolled history tells how much of it is below, in its last row on
    // screen, centered between the one column margins at both edges
    if (m_scroll > 0 && shown > 0)
    {
        std::string label = "[ " + std::to_string(m_scroll) + (m_scroll == 1 ? " row" : " rows") + " below ]";
        int left = 1 + std::max(0, (width - 2 - int(label.size())) / 2);
        row.clear();
        row.position(left, 0).color(FG_DARK_GREY).draw(label);
        std::copy(row.buffer, row.buffer + canvas, m_frame.begin() + size_t(shown - 1) * width);
    }

    if (m_isLivePinned && live > 0)
    {
        for (int y = 0; y < live; ++y)
        {
            const CHAR_INFO* liveRow = (m_live.buffer + size_t(y) * canvas);
            std::copy(liveRow, liveRow + canvas, m_frame.begin() + size_t(shown + y) * width);
        }
    }
    for (CHAR_INFO& cell : m_frame)
    {
        cell.Attributes = console::MapAttributes(cell.Attributes);
    }

    // all the changed rows are written at once, otherwise the terminal may show
    // a part of them from the previous frame and a part from the new one
    auto isRowShown = [&](int y)
    {
        return (m_shown.size() == m_frame.size() &&
            std::memcmp(&m_frame[size_t(y) * width], &m_shown[size_t(y) * width], sizeof(CHAR_INFO) * width) == 0);
    };
    int top = 0, bottom = (height - 1);
    while (top <= bottom && isRowShown(top)) ++top;
    while (bottom >= top && isRowShown(bottom)) --bottom;
    if (top <= bottom)
    {
        console::WriteCells(m_handle, &m_frame[size_t(top) * width], width, bottom - top + 1, top);
    }
    m_shown.swap(m_frame);
}
