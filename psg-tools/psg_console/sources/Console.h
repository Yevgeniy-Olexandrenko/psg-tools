#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <string>
#include <windows.h>

// Thin layer over the classic Win32 Console API. It is available on Windows 7
// and keeps working under ConPTY (Windows Terminal), where conhost translates
// these calls into VT sequences. It never changes the console window size.
namespace console
{
	struct Geometry
	{
		int width;        // columns that are safe to draw into
		int height;       // rows visible in the console window
		int windowTop;    // buffer row shown at the top of the window
		int windowBottom; // buffer row shown at the bottom of the window
		int bufferHeight; // rows in the screen buffer
		int cursorY;      // buffer row of the cursor
	};

	bool GetGeometry(Geometry& geometry);

	void ShowCursor(bool show);
	void SetCursorRow(int y);

	// Writes rows of cells to the buffer starting at row y, from column 0.
	void WriteCells(const CHAR_INFO* cells, int width, int rows, int y);

	// Checks if the buffer holds the characters of the given cells from the
	// start of row y. Text longer than the row continues on the next rows.
	bool IsTextAt(int y, const CHAR_INFO* cells, int count);

	// Fills whole rows with spaces in the current text attributes.
	void ClearRows(int y, int rows);

	// Writes the given number of new lines at the cursor, scrolling the buffer
	// when needed, and returns the cursor row after that.
	int WriteNewLines(int count);

	// Writes text in the given attributes and moves the cursor to the next
	// line. Long text is wrapped by the console.
	void WriteLine(const std::string& text, WORD attributes);

	void DisableQuickEdit();
}
