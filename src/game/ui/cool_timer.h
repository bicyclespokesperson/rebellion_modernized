#pragma once
#include <cstdint>

#include "core/layout.h"
#include "platform/win32.h"

// A multimedia timer (timeSetEvent) that calls its virtual OnTick on the winmm timer thread, serialised by a
// critical section and skipped while paused (m_pauseCount != 0). Used by the strobe buttons and the sound
// streams. In the standalone port this thread is replaced by the main-loop pump (docs/platform_layer.md).
class CoolTimer {
public:
    CoolTimer(UINT delay, UINT type, UINT resolution);  // 0x601f40: type is TIME_ONESHOT (0) or TIME_PERIODIC (1)
    virtual ~CoolTimer();                               // 0x601ff0 (scalar deleting destructor 0x601fd0)
    virtual void OnTick() = 0;                          // slot 1, called with m_pauseCount raised to 1

    void Pause();                              // 0x602010: nested pause count + 1 (the timer keeps running)
    void Stop();                               // 0x602020: kills the timer event
    void Restart(UINT delay, UINT type);       // 0x602060: 0 keeps the previous delay / type

    // The timeSetEvent callback (dwUser is the CoolTimer).
    static void CALLBACK TimerProc(UINT timerId, UINT message, DWORD_PTR user, DWORD_PTR reserved1,
                                   DWORD_PTR reserved2);  // 0x6020d0

    int m_pauseCount;         // +0x04 starts at 1 during construction
    UINT m_timerId;           // +0x08 0 when no event is armed
    UINT m_resolution;        // +0x0c at least g_timerPeriod
    UINT m_delay;             // +0x10 milliseconds
    UINT m_type;              // +0x14
    int m_running;            // +0x18 1 while an event is armed
    int m_tickMissed;         // +0x1c set when a periodic tick arrived while paused or busy
    CRITICAL_SECTION m_lock;  // +0x20
};

int BeginTimerPeriod(UINT milliseconds);  // 0x601ea0: timeBeginPeriod within the device's limits; 1 if started
int EndTimerPeriod();                     // 0x601f10: 1 if a period was active

extern int g_timerPeriodActive;  // 0x6be5b8
extern UINT g_timerPeriod;       // 0x6ac7d4 the period BeginTimerPeriod chose (10 at start-up)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(CoolTimer, 0x38);
REB_ASSERT_OFFSET(CoolTimer, m_pauseCount, 0x04);
REB_ASSERT_OFFSET(CoolTimer, m_running, 0x18);
REB_ASSERT_OFFSET(CoolTimer, m_lock, 0x20);
#pragma clang diagnostic pop
