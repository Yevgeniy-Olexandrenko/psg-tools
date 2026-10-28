#pragma once

#include <fstream>
#include "stream/Stream.h"

class Encoder
{
public:
	// which frames the encoder gets: the stream as it is, with
	// its loop, or the frames as the player plays them, with the
	// extra loops or the cut silence at the end, and without a loop
	enum class Duration { Original, Playback };

	virtual bool Open(const Stream& stream) = 0;
	virtual void Encode(const Frame& frame) = 0;
	virtual void Close(const Stream& stream) = 0;

	// duration used when the user does not choose one
	virtual Duration GetDefaultDuration() const { return Duration::Original; }

	// the encoder gets the source frames and processes them by itself
	virtual bool HasOwnProcessing() const { return false; }

	// called before Open(), loop frame 0 means there is no loop
	void SetFrames(size_t framesCount, FrameId loopFrameId);

protected:
	bool CheckFileExt(const Stream& stream, const std::string& ext) const;

protected:
	size_t m_framesCount = 0;
	FrameId m_loopFrameId = 0;
};
