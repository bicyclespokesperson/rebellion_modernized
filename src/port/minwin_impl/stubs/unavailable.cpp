// Services the port does not provide, answering the way the original's callers expect when the service is absent:
//   MCI (CD audio, the "avivideo" device) and the AVIFile reader: the game ships no AVI files and its MCI window class
//   is dead code,
//     every call fails;
//   winmm's timer information: timeGetDevCaps / timeBeginPeriod / timeEndPeriod succeed without doing anything
//   (timeGetTime and the
//     multimedia timers are in user/mmtimer.cpp);
//   DirectPlay (LAN multiplayer uses its own transport, docs/multiplayer.md): no service providers, no lobby,
//   nothing can be created.
#include "port/minwin/vfw.h"
#include "port/minwin/winmm.h"

#include "platform/dplay.h"

namespace
{
constexpr MCIERROR kMciUnsupportedFunction = 274;
constexpr HRESULT kAviErrorUnsupported = static_cast<HRESULT>(0x80044065);
constexpr HRESULT kDirectPlayUnsupported = static_cast<HRESULT>(0x8877000a); // DPERR_UNSUPPORTED
constexpr UINT kTimerPeriodMin = 1, kTimerPeriodMax = 1000000;
} // namespace

MCIERROR REB_STDCALL reb::compat::mciSendCommandA(MCIDEVICEID, UINT, DWORD_PTR, DWORD_PTR)
{
  return kMciUnsupportedFunction;
}
MCIDEVICEID REB_STDCALL reb::compat::mciGetDeviceIDA(LPCSTR)
{
  return 0;
}
HDRVR REB_STDCALL reb::compat::OpenDriver(LPCWSTR, LPCWSTR, LPARAM)
{
  return nullptr;
}

MMRESULT REB_STDCALL reb::compat::timeGetDevCaps(LPTIMECAPS caps, UINT size)
{
  if (!caps || size < sizeof(TIMECAPS)) {
    return 11; // TIMERR_STRUCT
  }
  caps->wPeriodMin = kTimerPeriodMin;
  caps->wPeriodMax = kTimerPeriodMax;
  return TIMERR_NOERROR;
}
MMRESULT REB_STDCALL reb::compat::timeBeginPeriod(UINT)
{
  return TIMERR_NOERROR;
}
MMRESULT REB_STDCALL reb::compat::timeEndPeriod(UINT)
{
  return TIMERR_NOERROR;
}

HRESULT REB_STDCALL reb::compat::AVIFileOpenA(PAVIFILE* file, LPCSTR, UINT, LPCLSID)
{
  if (file) {
    *file = nullptr;
  }
  return kAviErrorUnsupported;
}
HRESULT REB_STDCALL reb::compat::AVIFileInfoA(PAVIFILE, LPAVIFILEINFOA, LONG)
{
  return kAviErrorUnsupported;
}
ULONG REB_STDCALL reb::compat::AVIFileRelease(PAVIFILE)
{
  return 0;
}

HRESULT REB_STDCALL reb::compat::DirectPlayEnumerateA(LPDPENUMDPCALLBACKA, LPVOID)
{
  return 0;
}
HRESULT REB_STDCALL reb::compat::DirectPlayCreate(GUID*, IUnknown** created, IUnknown*)
{
  if (created) {
    *created = nullptr;
  }
  return kDirectPlayUnsupported;
}
HRESULT REB_STDCALL reb::compat::DirectPlayLobbyCreateA(GUID*, IDirectPlayLobbyA** lobby, IUnknown*, LPVOID, DWORD)
{
  if (lobby) {
    *lobby = nullptr;
  }
  return kDirectPlayUnsupported;
}
