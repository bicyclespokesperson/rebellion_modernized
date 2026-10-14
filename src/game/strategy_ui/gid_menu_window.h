#pragma once
#include <cstdint>

#include "platform/win32.h"
#include "strategy_ui/object_menu.h"

class DibBitmap;

// The Galactic Information Display menu of the galaxy view (child 0x140, opened by the GID console button): a menu of
// six categories (loyalty, fleets, personnel, facilities, ... ) whose sub-menus (children 0x141-0x146) list the
// display modes; clicking a mode sends WM_COMMAND 0x140 with the mode in the high word to the galaxy view.
class GidMenuWindow : public MenuWindowBase
{
public:
  GidMenuWindow(HINSTANCE instance,
                int x,
                int y,
                int width,
                int height,
                int side,
                CoolWindow* parent,
                uint32_t childId,
                DibBitmap* backdrop); // 0x451130
  ~GidMenuWindow() override; // 0x4511d0 (scalar deleting destructor 0x4511b0)

  // Builds the category entries and the six sub-menu windows.
  void OnCreate1() override; // slot 14, 0x4511e0

  DibBitmap* m_backdrop; // +0x14c not owned
  CoolWindow* m_owner; // +0x150 the galaxy view
  int m_side; // +0x154
  MenuWindowBase* m_subMenus[6]; // +0x158 the sub-menu windows (ids 0x141-0x146)
};
