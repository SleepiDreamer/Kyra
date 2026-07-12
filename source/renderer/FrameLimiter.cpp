#include "FrameLimiter.h"

FrameLimiter::FrameLimiter(const double targetFps)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_freq = static_cast<double>(freq.QuadPart);
    SetTargetFps(targetFps);

    m_timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, 
									TIMER_ALL_ACCESS);
    if (!m_timer)
    {
	    m_timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    m_nextTick = now.QuadPart;
}

FrameLimiter::~FrameLimiter()
{
    if (m_timer)
    {
	    CloseHandle(m_timer);
    }
}

void FrameLimiter::SetTargetFps(const double fps)
{
    m_frameTicks = (fps > 0.0) ? (m_freq / fps) : 0.0;
}

void FrameLimiter::Wait()
{
    if (m_frameTicks <= 0.0) return;

    m_nextTick += static_cast<LONGLONG>(m_frameTicks);

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    if (now.QuadPart > m_nextTick) 
    {
        m_nextTick = now.QuadPart;
        return;
    }

    const double spinMargin = m_freq * 0.0005; // spin the last 0.5 milliseconds

    if (const LONGLONG waitTicks = m_nextTick - now.QuadPart - static_cast<LONGLONG>(spinMargin); waitTicks > 0 && m_timer)
    {
        const double seconds = static_cast<double>(waitTicks) / m_freq;
        LARGE_INTEGER due;
        due.QuadPart = -static_cast<LONGLONG>(seconds * 10'000'000.0);
        SetWaitableTimer(m_timer, &due, 0, nullptr, nullptr, FALSE);
        WaitForSingleObject(m_timer, INFINITE);
    }

    do
    {
	    QueryPerformanceCounter(&now);
    } while (now.QuadPart < m_nextTick);
}