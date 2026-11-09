#include <Windows.h>
#include "FrameTimer.h"

#pragma comment(lib, "winmm.lib")

namespace
{
    // the last part of a wait spins, so the wait ends on time
    constexpr double k_spinTime = 0.001;

    // after this number of late frames the schedule starts again
    constexpr double k_maxLateFrames = 3;
}

FrameTimer::FrameTimer()
    : m_timer(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS))
    , m_sleepTime(0)
    , m_period(0)
    , m_next(0)
    , m_thread(0)
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    m_frequency = double(frequency.QuadPart);

    // There is no high resolution timer before Windows 10 1803. Then Sleep(1)
    // takes 1-2 ms with timeBeginPeriod(1), but Windows 11 can ignore it for
    // a process without a visible window, and Sleep(1) takes up to 15.6 ms.
    if (!m_timer)
    {
        timeBeginPeriod(1);
        SleepAndMeasure();
        SleepAndMeasure();
    }
}

FrameTimer::~FrameTimer()
{
    if (m_timer)
        CloseHandle(m_timer);
    else
        timeEndPeriod(1);
}

void FrameTimer::Start(double frameRate)
{
    m_period = (1.0 / frameRate);
    m_next = 0;
}

void FrameTimer::Wait()
{
    // the waiting thread must wake up on time
    if (m_thread != GetCurrentThreadId())
    {
        m_thread = GetCurrentThreadId();
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    }

    double now = GetTime();
    m_next += m_period;
    if (now - m_next > k_maxLateFrames * m_period)
    {
        m_next = now;
        return;
    }

    double sleep = (m_next - now - k_spinTime);
    if (m_timer && sleep > 0)
    {
        // a negative due time is relative, in 100 ns units
        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -LONGLONG(sleep * 1e7);
        if (SetWaitableTimer(m_timer, &dueTime, 0, nullptr, nullptr, FALSE))
        {
            WaitForSingleObject(m_timer, INFINITE);
        }
    }
    else if (!m_timer)
    {
        while (m_next - GetTime() > m_sleepTime + k_spinTime) SleepAndMeasure();
    }
    while (GetTime() < m_next) SwitchToThread();
}

double FrameTimer::GetTime() const
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart / m_frequency);
}

// keeps the longest time of Sleep(1), and forgets it slowly
void FrameTimer::SleepAndMeasure()
{
    double start = GetTime();
    Sleep(1);
    double time = (GetTime() - start);
    m_sleepTime = (time > m_sleepTime ? time : 0.99 * m_sleepTime + 0.01 * time);
}
