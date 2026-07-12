#pragma once
#include <windows.h>

class FrameLimiter {
public:
    explicit FrameLimiter(double targetFps);

    ~FrameLimiter();

    void SetTargetFps(double fps);

    void Wait();

private:
    HANDLE m_timer = nullptr;
    double m_freq = 0.0;
    double m_frameTicks = 0.0;
    LONGLONG m_nextTick = 0;
};