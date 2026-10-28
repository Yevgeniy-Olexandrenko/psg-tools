#pragma once

#include <filesystem>
#include <string>

#include "KeyboardInput.h"
#include "PrintBuffer.h"
#include "stream/Stream.h"
#include "player/Output.h"

namespace gui
{
	// The file shown in the live view at the bottom of the screen
	struct FileView
	{
		std::filesystem::path path;
		int index;
		int amount;
		bool favorite;
	};

	void Init(const std::wstring& title);
	void Shutdown();
	void Update();
	const KeyState& GetKeyState(int key);

	// Update() for the converter, which reports every frame and reads no keys:
	// the history still scrolls while the files are being converted
	void UpdateBusy();

	// The player keeps the live view pinned to the bottom of the window. The
	// converter goes from file to file quickly, so its live view is the end of
	// the history, and a history scrolled up stays still.
	void SetLivePinned(bool pinned);

	// Lines added to the history, above the live view
	void Print(const std::string& text, WORD color = FG_GREY);
	void PrintRuler();

	// The live view is redrawn by each call, its layout adapts to the
	// current size of the console window
	void DrawDecoding(const FileView& file, const Stream& stream);
	void DrawPlayback(const FileView& file, const Stream& stream, int frameId, const Output& output, bool showFrames);
	void DrawEncoding(const FileView& file, const Stream& stream, int frameId, size_t framesCount, const std::string& output);

	// Removes the live view and leaves the header and all the properties of
	// the file in the history, no matter how many of them fit the window
	void CommitFile(const FileView& file, const Stream& stream, const std::string& output);
	void EraseFile();
}
