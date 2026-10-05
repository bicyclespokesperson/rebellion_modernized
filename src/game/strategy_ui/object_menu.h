#pragma once
#include <cstdint>

#include "platform/win32.h"

class CoolWindow;
class DibBitmap;

// The object pop-up menu (the "Game Menu Window", 0x4422a0-0x442d70).
//
// Adds the entry for `command` (a menu command id, its text is string `command` of the string DLL) to the menu that
// is about to be shown at (x, y) over `window`. `flags`: 1 = entry unavailable, 2 = checked.
void* AddMenuEntry(uint32_t command, uint32_t kind, uint32_t flags, int x, int y, CoolWindow* window,
                   uint32_t color, uint32_t shadowColor, uint32_t disabledColor);  // 0x442590
// Opens the menu built by the AddMenuEntry calls at (x, y) over `owner`.
void* ShowMenu(int x, int y, HMODULE resources, CoolWindow* window, CoolWindow* owner, DibBitmap* backdrop);  // 0x442380
