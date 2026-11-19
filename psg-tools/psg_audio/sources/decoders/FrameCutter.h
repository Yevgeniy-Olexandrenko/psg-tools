#pragma once

#include <vector>
#include "stream/Stream.h"

// Cuts a stream of timed writes to the chips into frames. A player writes the
// registers once per update, in a burst with short pauses inside, and waits for
// the next update: a frame is taken at the start of the long pause after a
// burst, as the player has just set it, and every frame time while nothing is
// written. A burst too close to the last frame is a late part of its update.
// The times between the updates give the frame rate of the player.
//
// The time is in samples of 44.1 kHz. The caller tells the cutter about the
// writes and how long it is to the next one (at least), and moves the time:
//   OnWrite() for the writes at the current time;
//   IsFrameEnd(wait) before it moves: true means take a frame now;
//   Advance(wait) moves the time, but not past the next frame without updates.
class FrameCutter
{
public:
	// frameTime: the samples of a frame at the nominal frame rate.
	explicit FrameCutter(int frameTime);

	void OnWrite();
	bool IsFrameEnd(int wait);
	int  Advance(int wait);

	// The caller ended a frame by itself (not at an update of the player),
	// sinceFrame samples ago.
	void EndFrame(int sinceFrame = 0);

	// The rate of the updates, when most of them come at about the same time
	// and the rate is not above the limit.
	bool GetFrameRate(FrameRate& frameRate) const;

	int GetTime() const { return m_time; }

private:
	const int  m_frameTime;
	const int  m_longPause;
	const int  m_noUpdate;

	int  m_time;       // samples
	bool m_written;    // since the last frame
	int  m_sinceWrite; // samples
	int  m_sinceFrame; // samples
	int  m_burstTime;  // of the first write after the last frame
	int  m_updateTime; // of the burst of the last frame with an update, -1 after a frame without it
	int  m_updates;    // after another update
	std::vector<int> m_updateTimes; // number of updates by time after another update
};
