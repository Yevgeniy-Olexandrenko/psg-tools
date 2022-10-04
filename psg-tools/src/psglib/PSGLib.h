#pragma once

#include <filesystem>
#include "stream/Stream.h"
#include "stream/Player.h"
#include "output/Emulator/Emulator.h"
#include "output/Streamer/Streamer.h"

class PSGHandler
{
public:
	static const std::string DecodeFileTypes;
	static const std::string EncodeFileTypes;

protected:
	bool Decode(const std::filesystem::path& path, Stream& stream);
	bool Encode(const std::filesystem::path& path, Stream& stream);

protected:
	virtual void OnFrameDecoded(Stream& stream, FrameId frameId) {}
	virtual void OnFrameEncoded(Stream& stream, FrameId frameId) {}
};
