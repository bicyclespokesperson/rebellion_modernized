// WINMM clock and timers for the game: the window manager's implementations (src/port/minwin_impl/user/mmtimer.cpp),
// replaced by the virtual clock under REB_DETERMINISTIC and disabled under REB_NO_TIMERS (native_clock.h).
#include "platform/win32.h"
#include "port/native/native_clock.h"
#include "port/minwin_impl/user/mmtimer.h"

DWORD REB_STDCALL reb::compat::timeGetTime(void)
{
  if (native_clock::Deterministic()) {
    return native_clock::VirtualTickMs();
  }
  return minwin::RealTimeMs();
}

UINT REB_STDCALL reb::compat::timeSetEvent(UINT delay, UINT resolution, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags)
{
  if (native_clock::NoTimers()) {
    return 1;
  }
  return minwin::SetMultimediaTimer(delay, resolution, callback, user, flags);
}
