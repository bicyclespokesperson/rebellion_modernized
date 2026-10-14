#pragma once
#include <cstdint>

#include "platform/win32.h"
#include "ui/cool_window.h"

// "CoolwinStatusBar": a horizontal progress bar. The background is filled with one brush, the first
// position/range of the width with another, and every tick-step units a vertical tick line is drawn.
class CoolwinStatusBar : public CoolWindow
{
public:
  CoolwinStatusBar(int x,
                   int y,
                   int width,
                   int height,
                   CoolWindow* parent,
                   COLORREF barColor,
                   COLORREF backgroundColor,
                   uint32_t style); // 0x60e150
  ~CoolwinStatusBar() override; // 0x60e250 (scalar deleting destructor 0x60e230)

  // Paints on WM_PAINT; everything else goes to DefWindowProcA (not to CoolWindow::WindowProc).
  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override; // 0x60e460

  void SetBarColor(COLORREF color); // 0x60e2d0
  void SetBackgroundColor(COLORREF color); // 0x60e340
  void SetTickColor(COLORREF color); // 0x60e3b0
  void SetRange(uint32_t range, uint32_t tickStep); // 0x60e400: tickStep 0 = no ticks
  void SetPosition(uint32_t position); // 0x60e440

  uint32_t m_range; // +0x94 starts at 100
  uint32_t m_position; // +0x98
  uint32_t m_tickStep; // +0x9c
  COLORREF m_barColor; // +0xa0
  COLORREF m_backgroundColor; // +0xa4
  COLORREF m_tickColor; // +0xa8 never written
  HBRUSH m_barBrush; // +0xac
  HBRUSH m_backgroundBrush; // +0xb0
  HPEN m_tickPen; // +0xb4 null: black ticks
};
