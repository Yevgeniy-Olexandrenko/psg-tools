#pragma once

// Gives the frame times to an output that plays the frames on the timer of
// the computer: Wait() returns at the time of the next frame. The times keep
// to one schedule, so a late return makes the next wait shorter. The first
// wait and a wait after a break (a pause or a slow write) start the schedule
// again from that moment.
class FrameTimer
{
public:
	FrameTimer();
	~FrameTimer();

	void Start(double frameRate);
	void Wait();

private:
	double GetTime() const;
	void SleepAndMeasure();

	void*  m_timer;     // high resolution waitable timer, if the system has it
	double m_frequency; // of the performance counter
	double m_sleepTime; // of Sleep(1), if there is no such timer
	double m_period;
	double m_next;
	unsigned long m_thread; // the last thread that waited
};
