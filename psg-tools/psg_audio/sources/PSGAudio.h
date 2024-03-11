#pragma once

#include <filesystem>
#include "stream/Stream.h"
#include "player/Player.h"
#include "player/Emulator/Emulator.h"
#include "player/Streamer/Streamer.h"

class FileDecoder
{
public:
	static const std::string FileTypes;
	bool Decode(const std::filesystem::path& path, Stream& stream);

protected:
	virtual void OnFrameDecoded(const Stream& stream, FrameId frameId) {}
	virtual bool IsAbortRequested() const { return false; }
};

class FileEncoder
{
public:
	static const std::string FileTypes;
	bool Encode(const std::filesystem::path& path, Stream& stream);

protected:
	virtual void OnFrameEncoded(const Stream& stream, FrameId frameId) {}
};
