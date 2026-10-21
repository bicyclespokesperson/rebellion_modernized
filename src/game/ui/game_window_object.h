#pragma once
#include <cstdint>

#include "platform/win32.h"
#include "ui/cool_window.h"

class DibBitmap;

// The generic "GAME_WINDOW_OBJECT" window: a child window painted from a background bitmap, with a virtual
// overlay hook. It also keeps the palette in step (WM_QUERYNEWPALETTE / WM_PALETTECHANGED).
class GameWindowObject : public CoolWindow
{
  public:
  GameWindowObject(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, DibBitmap* background,
                   uint32_t extraStyle); // 0x5ff1c0
  ~GameWindowObject() override;          // 0x5ff250 (scalar deleting destructor 0x5ff230)

  // Slot 5: WM_PAINT draws the background (slot 10) and then the overlay (slot 14); WM_DESTROY returns 0 without
  // reaching the base; WM_ERASEBKGND is claimed; WM_CTLCOLORBTN gives buttons the null brush; the palette
  // messages realize g_palette. Everything else goes to CoolWindow::WindowProc.
  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override; // 0x5ff530
  virtual void DrawRect(HDC dc, const RECT* area); // slot 9, 0x5ff4b0: background, clipped to `area`
  virtual void Draw(HDC dc);                       // slot 10, 0x5ff450: whole background
  virtual void TickIdleAnimations();               // slot 11, 0x619730: does nothing
  virtual void OnIdle();                           // slot 12, 0x619730: does nothing
  virtual int HandleWindowRequest(int a, int b, int c, intptr_t d, int e); // slot 13, 0x5ff810: returns 0
  virtual void DrawOverlay(HDC dc); // slot 14, 0x5ae1e0: does nothing; WM_PAINT calls it last

  // Gives the window its background. `image` (owned, deleted here) is copied or drawn over the existing
  // background; with a null `image` the background is deleted. m_source, when set, is the picture that is
  // copied into a fresh background first, starting at (srcX, srcY).
  void SetBackground(DibBitmap* image, int srcX, int srcY); // 0x5ff2d0
  void SetBackgroundPreservingAddress(DibBitmap* image, int srcX, int srcY);
  DibBitmap* GetBackground(); // 0x5ff440
  // Draws the text of string resource `stringId` into the background bitmap (not onto the screen).
  int DrawStringResource(UINT stringId, RECT area, int fontSlot, COLORREF color, UINT format); // 0x5ff6b0

  DibBitmap* m_background; // +0x94 what Draw paints; owned
  DibBitmap* m_source;     // +0x98 optional picture the background is cut from; owned
};
