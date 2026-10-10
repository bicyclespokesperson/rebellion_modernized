// Connects the self-contained component libraries to each other and to the window manager, once, before the game starts
// (minwin_wiring::Install, called by the standalone's entry glue):
//   dsound  -> kernel32 SetEvent (buffer notifications), the SDL audio backend (started by the first DirectSoundCreate);
//   smack   -> the window manager's GetDC / ReleaseDC (SmackBufferBlit, SmackBufferSetPalette).
// The palette-changed notification (gdi -> window manager) is a plain call, minwin_user::NotifyPaletteChanged.
#include "port/minwin/minwin.h"
#include "port/minwin_impl/dsound/mixer.h"
#include "port/minwin_impl/smack/smack_api.h"

namespace minwin_audio {
void Install();
}

namespace {

void SignalEvent(void* event) { SetEvent(static_cast<HANDLE>(event)); }

HDC GetDeviceContext(HWND window) { return GetDC(window); }

int ReleaseDeviceContext(HWND window, HDC dc) { return ReleaseDC(window, dc); }

}  // namespace

namespace minwin_wiring {

void Install() {
    mwds::SetEventSignaler(SignalEvent);
    minwin_audio::Install();
    mwsmk::SetDeviceContextProvider(GetDeviceContext, ReleaseDeviceContext);
}

}  // namespace minwin_wiring
