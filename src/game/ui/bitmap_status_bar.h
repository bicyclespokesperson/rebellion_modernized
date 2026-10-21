#pragma once
#include <cstdint>

#include "platform/win32.h"
#include "ui/cool_status_bar.h"

class DibBitmap;

// How a BitmapStatusBar's bar grows (m_orientation / m_direction).
enum BitmapStatusBarOrientation : int { kBarHorizontal = 0, kBarVertical = 1 };
enum BitmapStatusBarDirection : int { kBarFromStart = 0, kBarFromEnd = 1 }; // from the left/top, or the right/bottom

// A CoolwinStatusBar that is drawn off-screen and can show a picture over the bar (colour-keyed), and that can
// run vertically or from the far end. The window takes the picture's size. Used by the system and sector windows
// (0x455060, 0x459e30).
class BitmapStatusBar : public CoolwinStatusBar
{
  public:
  BitmapStatusBar(int x, int y, int width, int height, CoolWindow* parent, COLORREF barColor, COLORREF backgroundColor,
                  DibBitmap* picture, int orientation,
                  int direction); // 0x4acec0
  ~BitmapStatusBar() override;    // 0x4acfc0 (scalar deleting destructor 0x4acfa0)

  // WM_PAINT draws background, bar, ticks and picture into a memory DC and blits it; the palette messages
  // realize g_palette; WM_ERASEBKGND is claimed; buttons get the null brush.
  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override; // 0x4ad210

  // Takes ownership of `image` and copies it into the picture (sizing the window to it on first use); an image that
  // is not loaded removes the picture.
  void SetPicture(DibBitmap* image); // 0x4ad040

  DibBitmap* m_picture; // +0xb8 owned heap copy of the picture
  DibBitmap* m_canvas;  // +0xbc owned DIB section the window is drawn into while there is a picture
  int m_orientation;    // +0xc0 BitmapStatusBarOrientation
  int m_direction;      // +0xc4 BitmapStatusBarDirection
};
