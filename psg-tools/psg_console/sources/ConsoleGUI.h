#pragma once

#include <filesystem>
#include <string>

#include "KeyboardInput.h"
#include "PrintBuffer.h"
#include "stream/Stream.h"
#include "player/Output.h"

namespace gui
{
	// The file shown in the live region at the bottom of the console
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

	// Lines printed into the scrollback history, above the live region
	void Print(const std::string& text, WORD color = FG_GREY);
	void PrintRuler();

	// The live region is redrawn by each call, its layout adapts to the
	// current size of the console window
	void DrawDecoding(const FileView& file, const Stream& stream);
	void DrawPlayback(const FileView& file, const Stream& stream, int frameId, const Output& output, bool showFrames);
	void DrawEncoding(const FileView& file, const Stream& stream, int frameId, const std::string& output);

	// Removes the live region and leaves the header and all the properties of
	// the file in the history, no matter how many of them fit the window
	void CommitFile(const FileView& file, const Stream& stream, const std::string& output);
	void EraseFile();
}
