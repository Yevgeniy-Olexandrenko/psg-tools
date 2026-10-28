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
	// True if the standard output is a console, not a file or a pipe.
	bool IsConsole();

	// The colors of the program set no background, so the background of the
	// console is used for them, and the plain grey text gets the default text
	// color of the console. Under ConPTY that keeps the colors of the terminal.
	WORD MapAttributes(WORD attributes);

	// A screen buffer of the program's own, shown instead of the standard
	// output one until CloseScreen(). Its cursor is hidden. Where the console
	// takes VT sequences, it is also on the alternate screen of the terminal.
	HANDLE CreateScreen();
	void CloseScreen(HANDLE screen);

	// Makes the buffer exactly as big as its window, so there is nothing to
	// scroll in it, and returns the window size.
	COORD FitScreen(HANDLE screen);

	// Writes rows of cells to the buffer starting at row y, from column 0.
	void WriteCells(HANDLE screen, const CHAR_INFO* cells, int width, int rows, int y);

	// Columns in the window of the standard output.
	int GetWidth();

	// Writes text in the given attributes and moves the cursor to the next
	// line. Long text is wrapped by the console. When the output is redirected
	// to a file or a pipe, the text goes there as a plain line.
	void WriteLine(const std::string& text, WORD attributes);

	// Writes a row of cells as a line of text in their attributes, without
	// the trailing spaces, and moves the cursor to the next line.
	void WriteRow(const CHAR_INFO* cells, int count);

	// Turns off the quick edit mode, which would freeze the output while the
	// user selects text, and turns on the mouse input for the wheel.
	// RestoreInput() brings back the original input mode and drops the keys
	// nobody has read, so they do not get to the shell.
	void SetupInput();
	void RestoreInput();

	// Sets the console title, the first call saves the original one for
	// RestoreTitle().
	void SetTitle(const std::wstring& title);
	void RestoreTitle();
}
