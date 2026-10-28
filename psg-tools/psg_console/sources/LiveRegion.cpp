#include <algorithm>
#include <cstring>
#include <thread>

#include "LiveRegion.h"
#include "Console.h"

namespace
{
    CHAR_INFO MakeBlank()
    {
        CHAR_INFO blank;
        blank.Char.UnicodeChar = L' ';
        blank.Attributes = FG_GREY;
        return blank;
    }

    const CHAR_INFO k_blank = MakeBlank();

    // When the window gets lower, the terminal moves its top rows to its own
    // scrollback, while under ConPTY the console just drops them, and the app
    // cannot clean that scrollback. So a few rows of the history are always
    // kept above the region: then they are the ones that go there, not
    // a snapshot of the top of the region. The long buffer of classic conhost
    // keeps everything itself
    int GetHistoryReserve(int windowHeight)
    {
        return (console::IsPseudoConsole() ? std::min(4, windowHeight / 4) : 0);
    }

    // The rows of the region also stop a few columns short of the right edge
    // under ConPTY: when the window gets narrower, every row that no longer
    // fits is wrapped onto two by the terminal and the console, which pushes
    // the top of the region out of the window as well. With the spare columns
    // a small step of the border does not wrap anything, and the next frame
    // is drawn for the new width already
    int GetColumnReserve(int width)
    {
        return (console::IsPseudoConsole() ? std::min(8, width / 8) : 0);
    }

    const auto k_steadyTime = std::chrono::milliseconds(300);
    const auto k_steadyTimeout = std::chrono::seconds(2);
}

LiveRegion::LiveRegion()
    : m_canvas(0, 0)
    , m_width(0)
    , m_height(0)
    , m_maxHeight(0)
    , m_content(0)
    , m_shown(0)
    , m_reserved(0)
{
    // takes the console colors before anything is drawn
    console::MapAttributes(k_blank.Attributes);
    Sync();
}

bool LiveRegion::Sync()
{
    console::Geometry geometry;
    if (!console::GetGeometry(geometry)) return false;
    int width = (geometry.width - GetColumnReserve(geometry.width));
    if (width == m_width && geometry.height == m_height) return false;

    int top = FindTop(geometry);
    console::SetCursorRow(top);
    console::ClearRows(top, geometry.bufferHeight - top);

    if (m_width > 0) m_resizeTime = std::chrono::steady_clock::now();
    m_width = width;
    m_height = geometry.height;
    m_maxHeight = std::max(1, geometry.height - GetHistoryReserve(geometry.height));
    m_screen.clear();
    m_content = 0;
    m_shown = 0;
    m_reserved = 0;
    return true;
}

int LiveRegion::FindTop(const console::Geometry& geometry) const
{
    // On resize the console reflows the text, and the region still starts at
    // the cursor. The long buffer of classic conhost always keeps it so.
    // Under ConPTY the console may put the cursor elsewhere, so the region is
    // looked for in the window: it starts right below the last line of the
    // history, and its first row is the one that was shown there
    int cursor = geometry.cursorY;
    if (!console::IsPseudoConsole()) return cursor;

    int count = m_width;
    while (count > 1 && m_content > 0 && m_screen[count - 1].Char.UnicodeChar == L' ') --count;
    auto isRegionAt = [&](int y)
    {
        return (m_content > 0 && console::IsTextAt(y, m_screen.data(), count));
    };
    if (isRegionAt(cursor)) return cursor;

    // the same line may be shown in the region too, so the lowest place where
    // the region follows the line wins, or just the lowest one without a region
    int lineRows = (int(m_lastLine.size()) + geometry.width) / (geometry.width + 1);
    int afterLine = -1;
    for (int y = geometry.windowBottom; y >= geometry.windowTop && !m_lastLine.empty(); --y)
    {
        if (console::IsTextAt(y, m_lastLine.data(), int(m_lastLine.size())))
        {
            int below = std::min(y + std::max(1, lineRows), geometry.bufferHeight - 1);
            if (isRegionAt(below)) return below;
            if (afterLine < 0) afterLine = below;
        }
    }
    if (m_content == 0) return (afterLine >= 0 ? afterLine : cursor);

    for (int y = geometry.windowBottom; y > geometry.windowTop; --y)
    {
        if (isRegionAt(y)) return y;
    }

    // the region got taller than the window and its top went out of the buffer:
    // then all visible rows belong to the region
    return (afterLine >= 0 ? afterLine : geometry.windowTop);
}

void LiveRegion::SetLastLine(const CHAR_INFO* cells, int count)
{
    while (count > 0 && cells[count - 1].Char.UnicodeChar == L' ') --count;
    m_lastLine.assign(cells, cells + count);
}

void LiveRegion::WaitForSteadySize()
{
    auto start = std::chrono::steady_clock::now();
    for (;;)
    {
        Sync();
        auto now = std::chrono::steady_clock::now();
        if (now - m_resizeTime >= k_steadyTime || now - start >= k_steadyTimeout) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

PrintBuffer& LiveRegion::Begin(int height)
{
    m_canvas.resize(m_width, std::clamp(height, 0, m_maxHeight));
    m_canvas.position(0, 0).color(FG_GREY);
    return m_canvas;
}

void LiveRegion::Render()
{
    if (m_canvas.w == m_width)
    {
        Present(m_canvas.buffer, m_canvas.h);
    }
}

void LiveRegion::Commit(int rows)
{
    rows = std::clamp(rows, 0, m_content);
    if (rows == 0) return;

    // the rows go to the history as lines of text, like everything else there:
    // the console and the terminal reflow such lines alike on resize, while
    // the cells written over the region may get joined with the rows below
    // them once the window gets narrower and wider again
    std::vector<CHAR_INFO> shown(m_screen);
    int height = m_content;

    Erase();
    for (int y = 0; y < rows; ++y)
    {
        console::WriteRow(&shown[size_t(m_width) * y], m_width);
    }
    SetLastLine(&shown[size_t(m_width) * (rows - 1)], m_width);
    m_reserved = 0;

    if (height > rows) Present(&shown[size_t(m_width) * rows], height - rows);
}

void LiveRegion::Erase()
{
    console::Geometry geometry;
    if (m_shown > 0 && console::GetGeometry(geometry))
    {
        console::ClearRows(geometry.cursorY, m_shown);
    }
    m_screen.clear();
    m_content = 0;
    m_shown = 0;
}

void LiveRegion::Print(const std::string& text, WORD attributes)
{
    // the message takes the place of the region, which then is shown again
    // right below it at once
    std::vector<CHAR_INFO> shown(m_screen);
    int height = m_content;

    Erase();
    console::WriteLine(text, attributes);
    m_reserved = 0;

    std::vector<CHAR_INFO> line(text.size(), k_blank);
    std::wstring wide(text.size(), L' ');
    int size = MultiByteToWideChar(CP_ACP, 0, text.data(), int(text.size()), wide.data(), int(wide.size()));
    for (int i = 0; i < size; ++i) line[i].Char.UnicodeChar = wide[i];
    SetLastLine(line.data(), size);

    if (height > 0) Present(shown.data(), height);
}

void LiveRegion::Present(const CHAR_INFO* cells, int height)
{
    console::Geometry geometry;
    if (m_width <= 0 || !console::GetGeometry(geometry)) return;
    int top = geometry.cursorY;

    if (height > m_reserved)
    {
        // make room for the region: if it does not fit below the cursor, the
        // new lines scroll the history up, and in conhost they also bring
        // the region into view; the rows keep what was shown on them.
        // The region ends right at the bottom of the window then: under ConPTY
        // empty rows below it make the terminal and the console place the text
        // differently when they reflow it on resize
        top = (console::WriteNewLines(height - 1) - (height - 1));
        console::SetCursorRow(top);
        console::ShowCursor(false);
        m_reserved = height;
        m_screen.clear();
        m_content = 0;
        m_shown = 0;
    }

    // Under ConPTY the rows from the region down to the bottom of the window
    // get a blank Braille pattern: it looks like a space, but it is not one,
    // so the terminal does not take the rows for empty ones, which it would
    // drop on resize, while the console would keep them, and then both would
    // place the text differently
    int fill = height;
    if (console::IsPseudoConsole())
    {
        fill = std::max(height, geometry.windowBottom - top + 1);
    }

    // rows that are no longer a part of the region get blanked
    const size_t width = size_t(m_width);
    int rows = std::max(fill, m_shown);
    m_frame.assign(width * rows, k_blank);
    std::copy(cells, cells + width * height, m_frame.begin());
    for (int y = height; y < fill; ++y)
    {
        m_frame[width * y].Char.UnicodeChar = wchar_t(0x2800);
    }
    for (CHAR_INFO& cell : m_frame)
    {
        cell.Attributes = console::MapAttributes(cell.Attributes);
    }

    // all the changed rows are written at once, otherwise the terminal may show
    // a part of them from the previous frame and a part from the new one
    int first = 0, last = (rows - 1);
    while (first <= last && IsRowShown(&m_frame[width * first], first)) ++first;
    while (last >= first && IsRowShown(&m_frame[width * last], last)) --last;
    if (first <= last)
    {
        console::WriteCells(&m_frame[width * first], m_width, last - first + 1, top + first);
    }

    m_frame.resize(width * fill);
    m_screen.swap(m_frame);
    m_content = height;
    m_shown = fill;
}

bool LiveRegion::IsRowShown(const CHAR_INFO* row, int y) const
{
    return (y < m_shown && std::memcmp(row, &m_screen[size_t(m_width) * y], sizeof(CHAR_INFO) * m_width) == 0);
}
