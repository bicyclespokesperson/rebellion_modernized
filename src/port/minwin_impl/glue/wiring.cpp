// Connects the self-contained component libraries to each other and to the window manager, once, before the game starts
// (minwin_wiring::Install, called by src/port/native/main.cpp):
//   dsound  -> the SDL audio backend (started by the first DirectSoundCreate); smack   -> the window manager's GetDC /
//   ReleaseDC (SmackBufferBlit, SmackBufferSetPalette).
// The palette-changed notification (gdi -> window manager) is a plain call, minwin_user::NotifyPaletteChanged.
#include "port/minwin/minwin.h"
#include "port/minwin_impl/dsound/mixer.h"
#include "port/minwin_impl/smack/smack_api.h"
#include "port/minwin_impl/audio/sdl_audio.h"

namespace
{

HDC GetDeviceContext(HWND window)
{
  return reb::compat::GetDC(window);
}

int ReleaseDeviceContext(HWND window, HDC dc)
{
  return reb::compat::ReleaseDC(window, dc);
}

} // namespace

namespace minwin_wiring
{

void Install()
{
  minwin_audio::Install();
  mwsmk::SetDeviceContextProvider(GetDeviceContext, ReleaseDeviceContext);
}

} // namespace minwin_wiring
