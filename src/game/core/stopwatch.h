#pragma once
#include <cstdint>

#include "platform/win32.h"

// Elapsed-time clock: QueryPerformanceCounter when the hardware has one, timeGetTime otherwise.
class Stopwatch {
public:
    Stopwatch();
    virtual ~Stopwatch();

    // Restarts the clock.
    void Reset();
    // Seconds since construction or the last Reset().
    double ElapsedSeconds();


    uint8_t m_unk04[4];      // +0x04
    float m_secondsPerTick;  // +0x08, 1.0f / low 32 bits of the QPC frequency; 0 selects timeGetTime
    uint8_t m_unk0c[4];      // +0x0c
    uint32_t m_startLow;     // +0x10 (QPC low dword, or timeGetTime in fallback mode)
    int32_t m_startHigh;     // +0x14
};
