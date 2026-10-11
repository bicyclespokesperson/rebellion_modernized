#pragma once
#include <cstdint>

#include "platform/win32.h"

class CoolWindow;
class DibBitmap;

// Bits of CoolDragImage::m_flags.
enum CoolDragImageFlags : uint32_t {
    kDragImageDragging = 0x01,
};

// A picture that follows the mouse over the application window during drag and drop (used by CoolDragList and
// several game screens). It is drawn straight onto the main window's DC: BeginDrag saves the window's pixels,
// every move restores the old spot and draws the picture (colour-keyed) at the new one.
class CoolDragImage {
public:
    // `window` is ignored: the picture is always drawn on g_app.
    explicit CoolDragImage(CoolWindow* window);  // 0x60db00
    ~CoolDragImage();                            // 0x60db40: ends a drag in progress

    int IsDragging();                      // 0x60dba0
    int SetImage(DibBitmap* source);       // 0x60dbb0: takes a copy; 0 while dragging
    int ClearImage();                      // 0x60dc80
    int SetHotspot(int x, int y);          // 0x60dcb0: the point of the picture that sits under the cursor
    int BeginDrag(POINT cursor);           // 0x60dce0: cursor in m_window's client coordinates
    int EndDrag();                         // 0x60de50: restores the window's pixels
    int MoveTo(const POINT* cursor);       // 0x60df00

    int SaveBackground(HDC dc, const POINT* topLeft);      // 0x60df90: copies the whole window into m_saved
    int RestoreBackground(HDC dc);                         // 0x60e050: repaints the spot under the picture
    int DrawAt(HDC dc, const POINT* topLeft);              // 0x60e0a0
    void TopLeftFor(POINT* topLeft, const POINT* cursor);  // 0x60e120

    DibBitmap* m_image;      // +0x00 owned copy
    CoolWindow* m_window;    // +0x04
    DibBitmap* m_saved;      // +0x08 DIB section with the window's pixels, owned while dragging
    DibBitmap* m_composite;  // +0x0c scratch DIB section of the picture's size, owned while dragging
    int m_hotspotX;          // +0x10
    int m_hotspotY;          // +0x14
    POINT m_position;        // +0x18 top-left of the picture as last drawn
    LONG m_savedStyle;       // +0x20 m_window's style before BeginDrag cleared WS_CLIPCHILDREN
    uint32_t m_flags;        // +0x24 CoolDragImageFlags
};
