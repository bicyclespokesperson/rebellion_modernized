#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "platform/win32.h"
#include "ui/bitmap_overlay.h"
#include "ui/cool_window.h"

class DibBitmap;
class istream;
class ostream;

// Bits of CoolGameWindow::m_flags.
enum CoolGameWindowFlags : uint32_t {
    kGameWindowPopup = 0x01,         // OnCreate opens it as a popup (BeginPopup); WM_SHOWWINDOW raises it to the top
    kGameWindowTileBackground = 0x02,  // together with the tiled background it is not drawn through m_paintBackground
    kGameWindowDraggable = 0x04,     // the left button drags the window
};

// Bits of CoolGameWindow::m_dragMode.
enum CoolGameWindowDragMode : uint32_t {
    kDragLive = 0x01,      // the window itself follows the mouse
    kDragOutline = 0x02,   // an XOR outline rectangle follows the mouse (otherwise the previous mode's rectangle)
};

// Custom messages of the window tree.
constexpr uint32_t kCloseChildMessage = 0x405;  // posted to the parent: destroy child (wParam id, lParam the window)

// "CoolGameWindow": the base class of the game's panels. It paints m_paintBackground (the background picture, or the
// background tiled over the window) in WM_PAINT and then the virtual DrawOverlay, owns the child buttons (m_children)
// and lets the user drag it by the left button within limits.
class CoolGameWindow : public CoolWindow {
public:
    // `stringSlot` / `bitmapSlot` name the resource DLLs the strings and bitmaps come from; `backgroundId` is the
    // RT_BITMAP of the background (0: none). Bit 0x10000000 of `style` (WS_VISIBLE) starts the window as shown.
    CoolGameWindow(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t stringSlot,
                   uint32_t bitmapSlot, uint32_t backgroundId, uint32_t childId, uint32_t style, uint32_t flags,
                   uint32_t unknownB0, uint32_t unknownB4);  // 0x606380
    virtual ~CoolGameWindow();  // 0x606580 (scalar deleting destructor 0x606560)

    virtual void Hide() override;                                                       // slot 2, 0x607d30
    virtual LRESULT REB_STDCALL WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x606650
    virtual void SaveState(ostream* out);              // slot 9, 0x5ae1e0 (shared): does nothing; the strategy windows write their state
    virtual void LoadState(istream* in);               // slot 10, 0x5ae1e0
    virtual void ShowAndTrack(int command);            // slot 11, 0x607d00: Show that keeps m_shown
    virtual void PostCloseRequest();                   // slot 12, 0x606960: asks the parent to destroy this window
    virtual void OnCreate();                           // slot 13, 0x6068f0: WM_CREATE
    virtual void OnCreate1();                          // slot 14, 0x619730 (shared): does nothing
    virtual void OnCreate2();                          // slot 15
    virtual void OnCreate3();                          // slot 16
    virtual void DrawOverlay(HDC dc);                  // slot 17, 0x5ae1e0: drawn over the background
    virtual void OnCommand(int a, int b, int c);       // slot 18, 0x48b3d0 (shared): WM_COMMAND (wParam, wParam, lParam)
    virtual void OnKeyDown(WPARAM key, LPARAM lParam);  // slot 19, 0x606940: handed on to the parent
    virtual int Virtual14();                           // slot 20, 0x40f340: returns 1
    virtual void Virtual15();                          // slot 21, 0x619730

    void CenterIn(int left, int top, int right, int bottom);    // 0x606980
    int BeginOffscreen(DibBitmap* target);                      // 0x6069d0: memory DC drawing onto `target`
    DibBitmap* EndOffscreen();                                   // 0x606ab0: returns the target if the window owns it
    int DrawTextResource(UINT stringId, RECT area, int fontSlot, COLORREF color, UINT format);  // 0x606b40
    int DrawResourceBitmap(int x, int y, uint32_t bitmapId);    // 0x606bd0: keyed onto the background
    // Creates a button from a bitmap resource (the window is sized from the bitmap) and adds it to m_children.
    int AddButton(int x, int y, uint32_t normalId, uint32_t pressedId, uint32_t childId, UINT tooltipId,
                   uint16_t labelStringId, int fontSlot, COLORREF color, COLORREF pressedColor, int repeatRate,
                   int pixelHit);  // 0x606c90
    int AddCloseButton(uint32_t bitmapId, uint32_t pressedId, uint32_t childId, UINT tooltipId);  // 0x606e10: top right
    int BeginDrag(int x, int y);                                 // 0x606ee0
    int EndDrag();                                               // 0x607010
    void SetDragLimits(RECT limits, HWND parentWindow);          // 0x6071a0
    int DragTo(int x, int y);                                    // 0x6071e0
    POINT* MakePoint(POINT* out, LPARAM lParam);                 // 0x607370: the signed words of lParam
    int HasFlags(uint32_t mask);                                // 0x607390: all of the bits in `mask`
    void FocusAndRemember();                                     // 0x6073b0: SetFocus, remember the previous focus
    DibBitmap* GetPaintBackgroundIfOwned();                      // 0x6073d0
    int NeedsOwnTarget();                                       // 0x6073f0
    int TileBackground();                                        // 0x607420: m_background tiled over the window
    uint32_t SetPaintBackground(DibBitmap* bitmap, uint32_t overlayId);  // 0x6075b0: returns the overlay id, 0 if none
    DibBitmap* SetPaintBackgroundById(uint32_t overlayId);       // 0x6075e0
    uint32_t AddOverlay(DibBitmap* bitmap, uint32_t overlayId);  // 0x607610
    DibBitmap* FindOverlayBitmap(uint32_t overlayId);            // 0x6076b0
    void InvalidateClient();                                     // 0x6076e0
    int DrawBackground(HDC dc);                                 // 0x607710
    // Draws a frame of corner and edge bitmaps into m_frame (the size of the client area).
    int BuildFrame(HMODULE module, uint32_t topId, uint32_t bottomId, uint32_t leftId, uint32_t rightId,
                    uint32_t topLeftId, uint32_t topRightId, uint32_t bottomLeftId, uint32_t bottomRightId);  // 0x607740
    void SetDragMode(uint32_t mode);                             // 0x607ce0
    DibBitmap* LoadBitmapCopy(uint32_t bitmapId);                // 0x607d50: DIB section copy of a resource bitmap
    CoolWindow* FindChild(uint32_t childId);                     // 0x607e90

    uint32_t m_stringSlot;        // +0x94 resource DLL of the strings (g_stringModule while this window is created)
    uint32_t m_bitmapSlot;        // +0x98 resource DLL of the bitmaps
    uint32_t m_backgroundId;      // +0x9c
    DibBitmap* m_background;      // +0xa0 owned
    DibBitmap* m_paintBackground;  // +0xa4 what WM_PAINT draws: m_background, the tiled copy or something set from outside
    DibBitmap* m_frame;           // +0xa8 owned, built by BuildFrame
    uint32_t m_commandId;         // +0xac copy of the child id
    uint32_t m_unknownB0;         // +0xb0
    uint32_t m_unknownB4;         // +0xb4
    uint32_t m_flags;             // +0xb8 CoolGameWindowFlags
    RECT m_dragLimits;            // +0xbc empty: unrestricted
    HWND m_dragParent;            // +0xcc window the drag coordinates are relative to
    HDC m_memoryDc;               // +0xd0 BeginOffscreen
    HGDIOBJ m_savedBitmap;        // +0xd4
    HGDIOBJ m_savedPalette;       // +0xd8
    HGDIOBJ m_savedFont;          // +0xdc
    int m_dragging;               // +0xe0
    int m_dragX;                  // +0xe4 outline position, or the grab point
    int m_dragY;                  // +0xe8
    int m_grabX;                  // +0xec
    int m_grabY;                  // +0xf0
    uint32_t m_dragMode;          // +0xf4 CoolGameWindowDragMode (initially kDragLive)
    BitmapOverlayTree m_overlays;  // +0xf8 overlay registry (IdAvlTree subclass, ctor 0x60f420)
    DibBitmap* m_tiledBackground;  // +0x104 owned
    DibBitmap* m_drawTarget;      // +0x108 target of BeginOffscreen
    HWND m_previousFocus;         // +0x10c
    int m_shown;                  // +0x110 follows Show/Hide (starts as WS_VISIBLE)
};
