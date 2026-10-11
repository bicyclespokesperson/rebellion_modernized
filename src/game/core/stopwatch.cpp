#include "core/stopwatch.h"


Stopwatch::Stopwatch() {
    LARGE_INTEGER frequency;
    if (QueryPerformanceFrequency(&frequency)) {
        // BUG(original): only the low 32 bits of the frequency are used, and the quotient is stored as a float.
        m_secondsPerTick = static_cast<float>(1.0f / static_cast<double>(static_cast<uint64_t>(frequency.LowPart)));
    } else {
        m_secondsPerTick = 0.0f;
    }
    Reset();
}

Stopwatch::~Stopwatch() {  }

void Stopwatch::Reset() {
    if (m_secondsPerTick != 0.0f) {
        if constexpr (sizeof(void*) == 4) {
            QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&m_startLow));  // m_startLow/m_startHigh are the 64-bit counter
        } else {
            // The pair is not 8-aligned once the vptr is 8 bytes wide.
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            m_startLow = now.LowPart;
            m_startHigh = now.HighPart;
        }
    } else
        m_startLow = timeGetTime();
}

double Stopwatch::ElapsedSeconds() {
    if (m_secondsPerTick == 0.0f) {
        const uint32_t elapsedMs = timeGetTime() - m_startLow;
        return static_cast<double>(static_cast<uint64_t>(elapsedMs)) * 0.001f;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const bool borrow = now.LowPart < m_startLow;
    const uint32_t lowDifference = now.LowPart - m_startLow;
    const int32_t highNow = static_cast<int32_t>(now.HighPart) - (borrow ? 1 : 0);
    const double lowSeconds = static_cast<double>(static_cast<uint64_t>(lowDifference)) * m_secondsPerTick;
    if (highNow == m_startHigh) return lowSeconds;
    const double highTickSeconds = static_cast<double>(m_secondsPerTick) * 4294967296.0f;
    return static_cast<double>(highNow - m_startHigh) * highTickSeconds + lowSeconds;
}
