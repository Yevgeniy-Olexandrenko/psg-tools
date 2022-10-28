#pragma once

#include <filesystem>
#include "stream/Stream.h"
#include "stream/Player.h"
#include "output/Emulator/Emulator.h"
#include "output/Streamer/Streamer.h"

class FileDecoder
{
public:
	static const std::string FileTypes;
	bool Decode(const std::filesystem::path& path, Stream& stream);

protected:
	virtual void OnFrameDecoded(Stream& stream, FrameId frameId) {}
};

class FileEncoder
{
public:
	static const std::string FileTypes;
	bool Encode(const std::filesystem::path& path, Stream& stream);

protected:
	virtual void OnFrameEncoded(Stream& stream, FrameId frameId) {}
};
