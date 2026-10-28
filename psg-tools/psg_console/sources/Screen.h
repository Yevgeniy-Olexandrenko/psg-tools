#pragma once

#include <functional>
#include <string>
#include <vector>

#include "KeyboardInput.h"
#include "PrintBuffer.h"

// The console window as the program draws it while it runs: the history at
// the top and the live view of the current file right below it. All of it is
// drawn into a screen buffer of the program's own and redrawn whole, so nothing
// depends on how the console and the terminal reflow their text on resize.
// The history is kept by the program and scrolled by it: the wheel, PgUp and
// PgDn. Close() shows the standard output buffer again and prints the
// history there as plain lines, so after the program ends it stays in the
// console scrollback like the output of any other program.
// When the output is redirected there is no screen, and the lines of the
// history go straight to the output.
class Screen
{
public:
	using DrawRow = std::function<void(PrintBuffer& canvas, int y)>;

	Screen();
	~Screen();

	bool IsActive() const { return m_handle != nullptr; }

	// Re-reads the window size and returns true if it has changed since
	// the previous call. The caller is expected to redo its layout then.
	bool Sync();
	int GetWidth() const { return m_width; }
	int GetMaxHeight() const { return m_height; } // the whole window

	// Starts a new live view of the given height, limited by the window,
	// and returns the cleared canvas to draw it on.
	PrintBuffer& Begin(int height);

	// Puts the screen on the console: the end of the history and the live
	// view below it. Only rows that differ from what is shown are written.
	void Render();

	// Removes the live view, the history takes the whole window.
	void Erase();

	// Adds a line of text to the history. On screen it is cut at the window
	// edge, Close() prints the whole of it.
	void Print(const std::string& text, WORD attributes = FG_GREY);

	// Adds a row to the history, which is drawn anew for the window width,
	// so headers and rulers always span the whole window.
	void Print(const DrawRow& draw);

	// Scrolls the history with the wheel and the keys.
	void Update(const KeyboardInput& keyboard);

	// Shows the standard output buffer again and prints the history there.
	void Close();

private:
	struct Entry
	{
		std::string text; // a line of text, or empty for a drawn row
		WORD attributes;
		DrawRow draw;
	};

	void DrawEntry(const Entry& entry, PrintBuffer& rows) const;
	void Add(Entry entry);
	void Scroll(int rows);
	void Present();

	HANDLE m_handle;
	int m_width;
	int m_height;
	std::vector<Entry> m_history; // one row each
	int m_scroll; // rows the history is scrolled up from its end
	PrintBuffer m_live;
	int m_liveHeight;  // rows of the live view, zero when it is erased
	std::vector<CHAR_INFO> m_frame; // the whole screen being drawn
	std::vector<CHAR_INFO> m_shown; // the whole screen as it is on the console
};
