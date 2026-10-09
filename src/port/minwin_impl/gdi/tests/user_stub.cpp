// Stand-in for the window manager's services (gdi_api.h) in the GDI tests.
#include "../../gdi_api.h"

int g_paletteNotifications = 0;

namespace minwin_user {
void NotifyPaletteChanged(HWND) { ++g_paletteNotifications; }
}  // namespace minwin_user
