#pragma once
#include <cstdint>

#include "platform/win32.h"
#include "ui/cool_window.h"

class CoolStrobeButton;
class DibBitmap;

// "CoolTabControl": a row of CoolStrobeButtons (its children) of which one is selected. The control paints a
// piece of its parent's background behind the buttons; clicking a tab (WM_COMMAND from the button) selects it and
// posts WM_COMMAND (tab id << 16 | control id) to the parent.
class CoolTabControl : public CoolWindow {
public:
    // The background is cut from `parentBackground` at (x, y).
    CoolTabControl(HINSTANCE instance, int x, int y, int width, int height, uint32_t childId, CoolWindow* parent,
                   DibBitmap* parentBackground);  // 0x60d590
    virtual ~CoolTabControl();                    // 0x60d690 (scalar deleting destructor 0x60d670)

    // WM_PAINT draws the background, WM_ERASEBKGND is claimed, WM_COMMAND from a tab selects it.
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // 0x60d860

    // Adopts `tab` as a child (re-parenting its window if needed). Null if it is already a tab of this control.
    // The first tab is always selected; later ones only when `select` is set.
    CoolStrobeButton* AddTab(CoolStrobeButton* tab, int select, int notify);  // 0x60d700
    void ReloadSelectedBitmaps();                    // 0x60d7b0: every tab's selected bitmap from its pressed one
    // Selects the tab with child id `tabId`; with `notify`, sends the parent the WM_COMMAND a click would post.
    void SelectTab(uint32_t tabId, int notify);       // 0x60d7e0
    // Replaces the background by a copy of `source` (cut at (x, y)) and gives it to every tab.
    void SetBackground(DibBitmap* source, int x, int y);  // 0x60da10

    CoolStrobeButton* m_selected;  // +0x94
    DibBitmap* m_background;       // +0x98 owned
};
