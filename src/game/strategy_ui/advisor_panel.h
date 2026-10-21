#pragma once
#include <cstdint>

#include "platform/win32.h"
#include "ui/cool_button.h"
#include "ui/cool_game_window.h"

class DibBitmap;
class GalaxyWindow;

// The help panel of one kind of window of the galaxy view (child type 0x11-0x75 are the window kinds): a darkened copy
// of the galaxy view with the panel's artwork over it, laid out from an RCDATA resource of the sprite DLL (bitmaps
// and captions), and a close button. WM_LBUTTONUP repaints it.
class AdvisorPanel : public CoolGameWindow
{
  public:
  AdvisorPanel(HINSTANCE instance, int x, int y, int width, int height, GalaxyWindow* parent, uint32_t childId,
               uint32_t kind); // 0x452090
  ~AdvisorPanel() override;    // 0x452190 (scalar deleting destructor 0x452170)

  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override; // slot 5, 0x452a70
  void OnCreate1() override;                                               // slot 14, 0x4522f0
  void DrawOverlay(HDC dc) override;                                       // slot 17, 0x452240
  void OnCommand(int a, int b, int c) override;                            // slot 18, 0x452a50

  // Lays out the artwork and captions of the resource `resourceId` of `module` over the panel, growing the panel to the
  // size the resource asks for (0x452630).
  void BuildFromResource(HMODULE module, uint32_t resourceId);

  uint32_t m_kind;                 // +0x114 the window kind the panel explains
  uint8_t m_unk118[4];             // +0x118
  GalaxyWindow* m_galaxy;          // +0x11c
  uint8_t m_unk120[0x20];          // +0x120
  uint8_t* m_remapTable;           // +0x140 darkening table of the background (heap block)
  CoolStrobeButton* m_closeButton; // +0x144
  DibBitmap* m_layer;              // +0x148 the artwork and captions, owned
};
