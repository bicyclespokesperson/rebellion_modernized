// The monotonic millisecond clock behind SmackWait, kept apart from the minwin headers (it needs <windows.h> on
// Windows).
#if defined(_WIN32)
#include <windows.h>
#else
#include <ctime>
#endif

namespace mwsmk
{

#if defined(_WIN32)
namespace
{

// The pacing of videos is wall-clock business, so it reads kernel32's real counter through GetProcAddress, never a
// virtual one: a polled SmackWait would otherwise run a virtual clock ahead by minutes while a movie plays.
using CounterFunction = BOOL(WINAPI*)(LARGE_INTEGER*);

CounterFunction RealCounterFunction(const char* name)
{
  return reinterpret_cast<CounterFunction>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), name));
}

} // namespace
#endif

double SystemClockMs()
{
#if defined(_WIN32)
  static const CounterFunction frequency = RealCounterFunction("QueryPerformanceFrequency");
  static const CounterFunction counter = RealCounterFunction("QueryPerformanceCounter");
  LARGE_INTEGER f, c;
  frequency(&f);
  counter(&c);
  return static_cast<double>(c.QuadPart) * 1000.0 / static_cast<double>(f.QuadPart);
#else
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<double>(ts.tv_sec) * 1000.0 + static_cast<double>(ts.tv_nsec) / 1.0e6;
#endif
}

} // namespace mwsmk
