#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "core/layout.h"
#include "platform/win32.h"
#include "ui/cool_button.h"

class DibBitmap;

// Bits of PanelDragButton::m_flags.
enum PanelDragButtonFlags : uint32_t {
    kDragButtonMovable = 0x1,         // the button can be dragged (an XOR outline follows the mouse)
    kDragButtonDoubleClickOpens = 0x2,  // a double click posts the button's command; single clicks are ignored
};

// The minimised Galactic Information Display legend (child 0x134 of the galaxy view): a picture button the player
// drags around inside `limits` (the outline is drawn on the galaxy view's window DC; on release the button snaps
// back inside the limits) and double-clicks to open the legend again.
class PanelDragButton : public CoolStrobeButton {
public:
    // `limits` are in the parent's coordinates; a limit rectangle with a zero left or top turns the clamping off.
    PanelDragButton(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t childId,
                    uint32_t normalBitmapId, uint32_t pressedBitmapId, DibBitmap* backdrop, int limitLeft,
                    int limitTop, int limitRight, int limitBottom, uint32_t flags);  // 0x452ab0
    virtual ~PanelDragButton();  // 0x452b60 (scalar deleting destructor 0x452b40)

    virtual LRESULT REB_STDCALL WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x452b70

    uint32_t m_flags;          // +0x114 PanelDragButtonFlags
    int m_dragging;            // +0x118 the outline is being dragged (the mouse is captured)
    RECT m_limits;             // +0x11c
    RECT m_outline;            // +0x12c the outline as last drawn
    int m_grabX;               // +0x13c where in the button the drag started
    int m_grabY;               // +0x140
    DibBitmap* m_backdropCopy;  // +0x144 the backdrop the constructor was given (not owned)

private:
    LRESULT OnMouseMove(WPARAM wParam, LPARAM lParam);
    LRESULT OnButtonUp();
    LRESULT OnDoubleClick(LPARAM lParam);
    void ClampToLimits(int* x, int* y);
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(PanelDragButton, 0x148);
REB_ASSERT_OFFSET(PanelDragButton, m_flags, 0x114);
REB_ASSERT_OFFSET(PanelDragButton, m_limits, 0x11c);
REB_ASSERT_OFFSET(PanelDragButton, m_outline, 0x12c);
REB_ASSERT_OFFSET(PanelDragButton, m_backdropCopy, 0x144);
#pragma clang diagnostic pop
