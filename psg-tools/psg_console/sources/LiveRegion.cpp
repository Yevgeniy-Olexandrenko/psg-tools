#include <algorithm>
#include <cstring>

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

    // Under ConPTY (Windows Terminal) the console buffer is exactly as tall as
    // the window, while classic conhost keeps a long buffer with scrollback.
    bool IsWindowSizedBuffer(const console::Geometry& geometry)
    {
        return (geometry.bufferHeight == geometry.height);
    }
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
    Sync();
}

bool LiveRegion::Sync()
{
    console::Geometry geometry;
    if (!console::GetGeometry(geometry)) return false;
    if (geometry.width == m_width && geometry.height == m_height) return false;

    // on resize the console reflows the text, and the first row of the region,
    // which may span several rows now, still starts at the cursor, unless the
    // region got taller than the window: then its top went out of the buffer
    // and all visible rows belong to the region
    int top = geometry.cursorY;
    if (m_shown > 0)
    {
        int count = m_width;
        while (count > 1 && m_screen[count - 1].Char.UnicodeChar == L' ') --count;

        if (!console::IsTextAt(top, m_screen.data(), count))
        {
            top = geometry.windowTop;
            console::SetCursorRow(top);
        }
    }
    console::ClearRows(top, geometry.bufferHeight - top);

    m_width = geometry.width;
    m_height = geometry.height;
    m_maxHeight = std::max(1, geometry.height);
    m_screen.clear();
    m_content = 0;
    m_shown = 0;
    m_reserved = 0;
    return true;
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
    console::Geometry geometry;
    rows = std::clamp(rows, 0, m_content);
    if (rows == 0 || !console::GetGeometry(geometry)) return;

    console::SetCursorRow(geometry.cursorY + rows);
    m_screen.erase(m_screen.begin(), m_screen.begin() + size_t(m_width) * rows);
    m_content -= rows;
    m_shown -= rows;
    m_reserved -= rows;
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
    // right below it, so nothing blinks
    std::vector<CHAR_INFO> shown(m_screen);
    int height = m_content;

    Erase();
    console::WriteLine(text, attributes);
    m_reserved = 0;

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
    // get a no-break space: it looks blank, but the terminal does not take
    // the rows for empty ones, which it would drop on resize, while the console
    // would keep them, and then both would place the text differently
    int fill = height;
    if (IsWindowSizedBuffer(geometry))
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
        m_frame[width * y].Char.UnicodeChar = L' ';
    }

    for (int y = 0; y < rows;)
    {
        if (IsRowShown(&m_frame[width * y], y))
        {
            ++y;
            continue;
        }

        // write a run of changed rows at once
        int end = (y + 1);
        while (end < rows && !IsRowShown(&m_frame[width * end], end)) ++end;
        console::WriteCells(&m_frame[width * y], m_width, end - y, top + y);
        y = end;
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
