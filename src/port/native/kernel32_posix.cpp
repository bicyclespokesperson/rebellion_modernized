// KERNEL32 on POSIX: the few calls the game and minwin_impl still make that have no standard-library form, declared by
// port/minwin/kernel32.h. Threads, events and locks are std::thread / std::mutex in the callers (see
// port/native/thread_sync.h). File handles are descriptors plus one (src/port/native/win_compat.cpp); Windows-style path
// names (drive letters, backslashes, any case) are translated by the resource library's ResolvePath.
#include <ctime>
#include <unistd.h>

#include "port/minwin/minwin.h"
#include "port/native/native_clock.h"
#include "port/native/platform.h"

namespace
{

// Small values are file descriptors plus one (src/port/native/win_compat.cpp), not objects.
constexpr uintptr_t kSmallHandleLimit = 0x10000;

} // namespace

extern "C" {

// Nothing in the port reports Win32 error codes, so the last error is always success.
DWORD REB_STDCALL GetLastError(void)
{
  return 0;
}

BOOL REB_STDCALL CloseHandle(HANDLE handle)
{
  if (handle && handle != INVALID_HANDLE_VALUE && reinterpret_cast<uintptr_t>(handle) < kSmallHandleLimit) {
    return close(static_cast<int>(reinterpret_cast<uintptr_t>(handle)) - 1) == 0;
  }
  return FALSE;
}

DWORD REB_STDCALL GetCurrentThreadId(void)
{
  return reb::platform::CurrentThreadId();
}

BOOL REB_STDCALL QueryPerformanceFrequency(LARGE_INTEGER* frequency)
{
  frequency->QuadPart = native_clock::Deterministic() ? 1000000 : 1000000000;
  return TRUE;
}

BOOL REB_STDCALL QueryPerformanceCounter(LARGE_INTEGER* counter)
{
  if (native_clock::Deterministic()) {
    counter->QuadPart = static_cast<LONGLONG>(native_clock::AdvanceCounterNs() / 1000);
    return TRUE;
  }
  timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  counter->QuadPart = static_cast<LONGLONG>(now.tv_sec) * 1000000000 + now.tv_nsec;
  return TRUE;
}

} // extern "C"
