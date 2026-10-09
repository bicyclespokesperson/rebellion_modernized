// The monotonic millisecond clock behind SmackWait, kept apart from the minwin headers (it needs <windows.h> on Windows).
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

namespace mwsmk {

double SystemClockMs() {
#if defined(_WIN32)
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return static_cast<double>(c.QuadPart) * 1000.0 / static_cast<double>(f.QuadPart);
#else
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) * 1000.0 + static_cast<double>(ts.tv_nsec) / 1.0e6;
#endif
}

}  // namespace mwsmk
