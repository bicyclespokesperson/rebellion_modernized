// The SMACKW32.DLL entry points the game imports (platform/smacker.h), implemented on the Smacker decoder (smk_movie.h) and the
// software mixer (dsound/mixer.h). Compiled without dllimport: the declarations of platform/smacker.h are used as they are, with
// REB_DLLIMPORT emptied before the include.
#pragma once
#include "core/reb_abi.h"
#undef REB_DLLIMPORT
#define REB_DLLIMPORT
#include "platform/smacker.h"

#include <stdint.h>

namespace mwsmk {

// The clock SmackWait compares against, in milliseconds. The default is a monotonic system clock; tests install their own.
double SystemClockMs();  // monotonic milliseconds (smack_clock.cpp)
using ClockFunction = double (*)();
void SetClock(ClockFunction clock);

// How SmackBufferBlit / SmackBufferSetPalette reach a window's device context (user32's GetDC / ReleaseDC in the port).
using GetDcFunction = HDC (*)(HWND);
using ReleaseDcFunction = int (*)(HWND, HDC);
void SetDeviceContextProvider(GetDcFunction get, ReleaseDcFunction release);

}  // namespace mwsmk

extern "C" {
// Not used by the game: the decoded audio of the current frame (RAD's SmackGetTrackData), for tests.
uint32_t REB_STDCALL SmackGetTrackData(Smack* smack, void* dest, uint32_t trackFlag);
}
