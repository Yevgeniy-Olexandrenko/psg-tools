#pragma once

#include <optional>
#include <filesystem>
#include "stream/Stream.h"
#include "encoders/Encoder.h"
#include "player/Player.h"
#include "player/Emulator/Emulator.h"
#include "player/Streamer/Streamer.h"
#include "player/WaveFile/WaveFile.h"

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
	using Duration = std::optional<Encoder::Duration>;
	bool Encode(const std::filesystem::path& path, Stream& stream, Duration duration = {});

protected:
	virtual void OnFrameEncoded(const Stream& stream, FrameId frameId, size_t framesCount) {}
};
