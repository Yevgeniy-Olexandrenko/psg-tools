#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "Console.h"
#include "PrintBuffer.h"

// The bottom part of the console that is redrawn every frame. Everything above
// the cursor is scrollback history and is never touched again. The cursor is
// always kept at the top row of the region, so the region can be erased and
// drawn anew at any moment, even after the terminal reflowed its text.
class LiveRegion
{
public:
	LiveRegion();

	// Re-reads the console window size and returns true if it has changed since
	// the previous call. After a change the region is erased from the screen and
	// the caller is expected to redo its layout for the new size.
	bool Sync();
	int GetWidth() const { return m_width; }
	int GetMaxHeight() const { return m_maxHeight; } // the window, less a few rows of history under ConPTY

	// Waits, for a limited time, until the window size stops changing. The rows
	// that go to the history are better written then: while the user drags the
	// window border, the terminal and the console reflow them each its own way.
	void WaitForSteadySize();

	// Starts a new frame of the given height, limited by GetMaxHeight(),
	// and returns the cleared canvas to draw it on.
	PrintBuffer& Begin(int height);

	// Puts the frame on screen. Only rows that differ from what is already
	// shown are written.
	void Render();

	// Leaves the top rows of the shown frame in the history: they are written
	// there as lines of text, and the rest of the frame is shown below them.
	void Commit(int rows);

	// Removes the region from the screen.
	void Erase();

	// Prints a line of text into the history above the region. Long lines
	// are wrapped by the console.
	void Print(const std::string& text, WORD attributes = FG_GREY);

private:
	int FindTop(const console::Geometry& geometry) const;
	void SetLastLine(const CHAR_INFO* cells, int count);
	void Present(const CHAR_INFO* cells, int height);
	bool IsRowShown(const CHAR_INFO* row, int y) const;

	PrintBuffer m_canvas;
	std::vector<CHAR_INFO> m_screen; // rows of the region currently on screen
	std::vector<CHAR_INFO> m_frame;  // rows being written by Present()
	std::vector<CHAR_INFO> m_lastLine; // the last line written to the history
	int m_width;
	int m_height;
	int m_maxHeight;
	int m_content;  // rows of the frame in m_screen
	int m_shown;    // all rows in m_screen, including the filling ones below the frame
	int m_reserved; // rows below the cursor that already exist in the buffer
	std::chrono::steady_clock::time_point m_resizeTime; // the last size change
};
