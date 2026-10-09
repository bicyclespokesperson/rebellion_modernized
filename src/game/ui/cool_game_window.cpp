#include "ui/cool_game_window.h"

#include "core/checked_new.h"
#include "core/containers.h"
#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"
#include "ui/bitmap_overlay.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/resource_dll.h"

// Overlay registry of the miscellaneous controls (0x60f420 range): an IdAvlTree subclass and its nodes.
void* __thiscall FUN_0060f420(void* registry);
void __thiscall FUN_0060f460(void* registry);
void* __thiscall FUN_0060f4f0(void* memory);

namespace {

constexpr uint32_t kPanelFlagsStyleVisible = 0x10000000;
constexpr uint32_t kResourceBitmapKind = 2;
constexpr uint32_t kHelperBitmapKind = 10;
constexpr char kBitBltMode = '@';

void DeleteBitmap(DibBitmap* bitmap) {
    if (bitmap) delete bitmap;
}

DibBitmap* LoadResourceBitmap(HMODULE module, uint32_t id, uint32_t kind) { return NewOrNull<DibBitmap>(module, id, kind); }

void DrawXorOutline(CoolGameWindow* window, int x, int y) {
    HDC dc = GetWindowDC(window->m_parent->m_hwnd);
    HGDIOBJ brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    HGDIOBJ pen = SelectObject(dc, GetStockObject(NULL_PEN));
    SetROP2(dc, R2_XORPEN);
    RECT outline;
    SetRect(&outline, x, y, window->m_width + x, window->m_height + y);
    Rectangle(dc, outline.left, outline.top, outline.right, outline.bottom);
    SelectObject(dc, brush);
    SelectObject(dc, pen);
    ReleaseDC(window->m_parent->m_hwnd, dc);
}

}  // namespace

CoolGameWindow::CoolGameWindow(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                               uint32_t stringSlot, uint32_t bitmapSlot, uint32_t backgroundId, uint32_t childId,
                               uint32_t style, uint32_t flags, uint32_t unknownB0, uint32_t unknownB4)
    : CoolWindow(instance, "CoolGameWindow", style, x, y, width, height, parent, nullptr,
                 static_cast<HBRUSH>(GetStockObject(NULL_BRUSH)), nullptr, nullptr, nullptr, 0, 0) {
    FUN_0060f420(m_overlays);
    m_stringSlot = stringSlot;
    m_bitmapSlot = bitmapSlot;
    m_backgroundId = backgroundId;
    if (backgroundId != 0) {
        DibBitmap* bitmap = NewOrNull<DibBitmap>(CoolAppGetResourceModule(m_bitmapSlot),
                                                 static_cast<uint32_t>(static_cast<uint16_t>(m_backgroundId)),
                                                 kResourceBitmapKind);
        m_background = bitmap;
        if (!bitmap->IsLoaded()) {
            DeleteBitmap(m_background);
            m_background = nullptr;
        }
    } else {
        m_background = nullptr;
    }
    m_flags = flags;
    m_childId = childId;
    m_commandId = childId;
    m_paintBackground = nullptr;
    m_tiledBackground = nullptr;
    m_drawTarget = nullptr;
    m_frame = nullptr;
    m_unknownB0 = unknownB0;
    m_unknownB4 = unknownB4;
    m_memoryDc = nullptr;
    m_savedBitmap = nullptr;
    m_savedPalette = nullptr;
    m_savedFont = nullptr;
    m_dragging = 0;
    SetRectEmpty(&m_dragLimits);
    m_dragParent = parent ? parent->m_hwnd : nullptr;
    m_dragMode = kDragLive;
    m_previousFocus = nullptr;
    m_shown = (style & kPanelFlagsStyleVisible) == kPanelFlagsStyleVisible ? 1 : 0;
}

CoolGameWindow::~CoolGameWindow() {
    if (m_background) {
        delete m_background;
        m_background = nullptr;
    }
    if (m_tiledBackground) {
        delete m_tiledBackground;
        m_tiledBackground = nullptr;
    }
    if (m_frame) {
        delete m_frame;
        m_frame = nullptr;
    }
    if (m_previousFocus) SetFocus(m_previousFocus);
    FUN_0060f460(m_overlays);
    REB_KEEP_VPTR_STORE();
}

void CoolGameWindow::Hide() {
    m_shown = 0;
    Show(0);
}

void CoolGameWindow::SaveState(ostream* out) {}

void CoolGameWindow::LoadState(istream* in) {}

void CoolGameWindow::ShowAndTrack(int command) {
    if (command == 5) {
        m_shown = 1;
        CoolWindow::Show(5);
        return;
    }
    if (command == 0) m_shown = 0;
    CoolWindow::Show(command);
}

void CoolGameWindow::PostCloseRequest() {
    PostMessageA(m_parent->m_hwnd, kCloseChildMessage, m_childId, reinterpret_cast<LPARAM>(this));
}

void CoolGameWindow::OnCreate() {
    if (HasFlags(kGameWindowPopup)) BeginPopup();
    g_stringModule = CoolAppGetResourceModule(m_stringSlot);
    TileBackground();
    OnCreate1();
    OnCreate2();
    OnCreate3();
}

void CoolGameWindow::OnCreate1() {}

void CoolGameWindow::OnCreate2() {}

void CoolGameWindow::OnCreate3() {}

void CoolGameWindow::DrawOverlay(HDC) {}

void CoolGameWindow::OnCommand(int, int, int) {}

void CoolGameWindow::OnKeyDown(WPARAM key, LPARAM lParam) {
    CoolWindow* parent = m_parent;
    if (parent) parent->WindowProc(WM_KEYDOWN, key, lParam);
}

int CoolGameWindow::Virtual14() { return 1; }

void CoolGameWindow::Virtual15() {}

void CoolGameWindow::CenterIn(int left, int top, int right, int bottom) {
    SetWindowPos(m_hwnd, nullptr, ((right - m_clientWidth) - left) / 2 + left, ((bottom - m_clientHeight) - top) / 2 + top,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

int CoolGameWindow::BeginOffscreen(DibBitmap* target) {
    HDC screen = GetDC(m_hwnd);
    HFONT font = CoolGetFont(4);
    if (target == nullptr)
        m_drawTarget = m_paintBackground;
    else
        m_drawTarget = target;
    const bool hasTarget = m_drawTarget != nullptr;
    if (hasTarget) {
        m_memoryDc = CreateCompatibleDC(screen);
        m_savedBitmap = SelectObject(m_memoryDc, m_drawTarget->DibSectionHandle());
        m_savedPalette = SelectObject(m_memoryDc, g_palette);
        ReleaseDC(m_hwnd, screen);
        m_savedFont = SelectObject(m_memoryDc, font);
        SetTextColor(m_memoryDc, PALETTERGB(255, 255, 255));
        SetBkMode(m_memoryDc, TRANSPARENT);
    }
    return hasTarget;
}

DibBitmap* CoolGameWindow::EndOffscreen() {
    SelectObject(m_memoryDc, m_savedPalette);
    SelectObject(m_memoryDc, m_savedBitmap);
    SelectObject(m_memoryDc, m_savedFont);
    DeleteDC(m_memoryDc);
    m_memoryDc = nullptr;
    m_savedBitmap = nullptr;
    m_savedPalette = nullptr;
    m_savedFont = nullptr;
    DibBitmap* owned = nullptr;
    if (NeedsOwnTarget()) owned = m_drawTarget;
    m_drawTarget = nullptr;
    return owned;
}

int CoolGameWindow::DrawTextResource(UINT stringId, RECT area, int fontSlot, COLORREF color, UINT format) {
    HDC dc = m_memoryDc;
    if (dc) {
        char text[0x100];
        SetTextColor(dc, color);
        SelectObject(m_memoryDc, CoolGetFont(fontSlot));
        LoadResourceString(stringId, text, 0xff);
        DrawTextA(m_memoryDc, text, -1, &area, format);
    }
    return dc != nullptr;
}

int CoolGameWindow::DrawResourceBitmap(int x, int y, uint32_t bitmapId) {
    bool drawn = false;
    DibBitmap* bitmap = NewOrNull<DibBitmap>(CoolAppGetResourceModule(m_bitmapSlot), bitmapId, kResourceBitmapKind);
    // BUG(original): the bitmap leaks when it is not drawn.
    if (m_paintBackground && bitmap && bitmap->IsLoaded()) {
        bitmap->DrawKeyed(m_paintBackground, x, y);
        delete bitmap;
        drawn = true;
    }
    return drawn;
}

int CoolGameWindow::AddButton(int x, int y, uint32_t normalId, uint32_t pressedId, uint32_t childId, UINT tooltipId,
                               uint16_t labelStringId, int fontSlot, COLORREF color, COLORREF pressedColor,
                               int repeatRate, int pixelHit) {
    CoolStrobeButton* button = nullptr;
    HMODULE module = CoolAppGetResourceModule(m_bitmapSlot);
    DibBitmap* size = NewOrNull<DibBitmap>(module, normalId, kResourceBitmapKind);
    if (size) {
        const int height = size->Height();
        const int width = size->Width();
        void* memory = operator new(sizeof(CoolStrobeButton));
        if (memory)
            button = new (memory) CoolStrobeButton(module, x, y, width, height, this, childId, normalId, pressedId,
                                                   GetPaintBackgroundIfOwned(), repeatRate, pixelHit);
        delete size;
    }
    if (!button) return false;
    button->Create(0);
    if (tooltipId != 0) AddToolFromResource(button, tooltipId);
    if (labelStringId != 0) button->SetLabelFromStringId(labelStringId);
    button->SetFont(fontSlot);
    button->SetStateColor(0, color);
    button->SetStateColor(1, pressedColor);
    button->SetStateColor(4, pressedColor);
    m_children.Insert(button);
    return true;
}

int CoolGameWindow::AddCloseButton(uint32_t bitmapId, uint32_t pressedId, uint32_t childId, UINT tooltipId) {
    DibBitmap* bitmap = NewOrNull<DibBitmap>(CoolAppGetResourceModule(m_bitmapSlot), bitmapId, kResourceBitmapKind);
    if (!bitmap) return false;
    AddButton(m_width - bitmap->Width() - 3, 3, bitmapId, pressedId, childId, tooltipId, 0, 0, 0, 0, 0, 1);
    delete bitmap;
    return true;
}

int CoolGameWindow::BeginDrag(int x, int y) {
    HWND hwnd = m_hwnd;
    HWND capture = GetCapture();
    if ((m_flags & kGameWindowDraggable) && (capture == nullptr || capture == hwnd)) {
        m_dragX = x;
        m_dragging = 1;
        m_dragY = y;
        if (m_dragMode & kDragOutline) {
            m_grabX = x;
            m_dragX = m_x;
            m_grabY = y;
            m_dragY = m_y;
            DrawXorOutline(this, m_dragX, m_dragY);
        }
        SetCaptureWindow(m_hwnd);
        return 1;
    }
    return 0;
}

int CoolGameWindow::EndDrag() {
    HWND hwnd = m_hwnd;
    m_dragging = 0;
    if (GetCapture() == hwnd) ReleaseCapture();
    if (!IsRectEmpty(&m_dragLimits)) {
        RECT position;
        int left;
        int top;
        if (!(m_dragMode & kDragLive)) {
            DrawXorOutline(this, m_dragX, m_dragY);
            left = m_dragX;
            top = m_dragY;
            SetRectEmpty(&position);
        } else {
            GetWindowRect(m_hwnd, &position);
            ScreenToClient(m_dragParent, reinterpret_cast<LPPOINT>(&position));
            left = position.left;
            top = position.top;
        }
        int newX = m_dragLimits.left;
        if (newX <= left) {
            newX = left;
            if (m_dragLimits.right < left + m_clientWidth) newX = m_dragLimits.right - m_clientWidth;
        }
        int newY = m_dragLimits.top;
        if (newY <= top) {
            newY = top;
            if (m_dragLimits.bottom < top + m_clientHeight) newY = m_dragLimits.bottom - m_clientHeight;
        }
        if (newX != position.left || newY != position.top)
            SetWindowPos(m_hwnd, nullptr, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    return 1;
}

void CoolGameWindow::SetDragLimits(RECT limits, HWND parentWindow) {
    SetRectEmpty(&m_dragLimits);
    if (HasFlags(kGameWindowDraggable)) {
        CopyRect(&m_dragLimits, &limits);
        if (parentWindow) m_dragParent = parentWindow;
    }
}

int CoolGameWindow::DragTo(int x, int y) {
    if (!m_dragging) return 0;
    if (m_dragMode & kDragLive) {
        POINT point = {x, y};
        MapWindowPoints(m_hwnd, m_parent->m_hwnd, &point, 1);
        SetWindowPos(m_hwnd, nullptr, point.x - m_dragX, point.y - m_dragY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        return 1;
    }
    if (m_parent) {
        HDC dc = GetWindowDC(m_parent->m_hwnd);
        HGDIOBJ brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        HGDIOBJ pen = SelectObject(dc, GetStockObject(NULL_PEN));
        SetROP2(dc, R2_XORPEN);
        RECT old;
        SetRect(&old, m_dragX, m_dragY, m_dragX + m_width, m_dragY + m_height);
        Rectangle(dc, old.left, old.top, old.right, old.bottom);
        const int left = x + (m_x - m_grabX);
        const int top = (m_y - m_grabY) + y;
        m_dragX = left;
        m_dragY = top;
        RECT moved;
        SetRect(&moved, left, top, left + m_width, top + m_height);
        Rectangle(dc, moved.left, moved.top, moved.right, moved.bottom);
        SelectObject(dc, brush);
        SelectObject(dc, pen);
        ReleaseDC(m_parent->m_hwnd, dc);
    }
    return 1;
}

POINT* CoolGameWindow::MakePoint(POINT* out, LPARAM lParam) {
    out->x = static_cast<short>(lParam);
    out->y = static_cast<short>(static_cast<uint32_t>(lParam) >> 16);
    return out;
}

int CoolGameWindow::HasFlags(uint32_t mask) { return (m_flags & mask) == mask; }

void CoolGameWindow::FocusAndRemember() { m_previousFocus = SetFocus(m_hwnd); }

DibBitmap* CoolGameWindow::GetPaintBackgroundIfOwned() {
    DibBitmap* bitmap = nullptr;
    if (NeedsOwnTarget()) bitmap = m_paintBackground;
    return bitmap;
}

int CoolGameWindow::NeedsOwnTarget() {
    if (m_paintBackground == m_tiledBackground && HasFlags(kGameWindowTileBackground)) return false;
    return true;
}

int CoolGameWindow::TileBackground() {
    int tiled = 0;
    if (m_background) {
        const int tileWidth = m_background->Width();
        const int tileHeight = m_background->Height();
        int rows = m_height / tileHeight;
        int columns = m_width / tileWidth;
        if (rows != 1) ++rows;
        if (columns != 1) ++columns;
        DibBitmap* previous = m_tiledBackground;
        if (previous) {
            delete previous;
            m_tiledBackground = nullptr;
        }
        HDC dc = GetDC(m_hwnd);
        m_tiledBackground = NewOrNull<DibBitmap>(static_cast<LONG>(columns * tileWidth), static_cast<LONG>(rows * tileHeight),
                                                 m_background, dc);
        if (m_tiledBackground) {
            for (int rowsLeft = rows, y = 0; rowsLeft > 0; --rowsLeft, y += tileHeight) {
                for (int columnsLeft = columns, x = 0; columnsLeft > 0; --columnsLeft, x += tileWidth)
                    m_background->Blit(m_tiledBackground, x, y, 0, 0, 0, 0);
            }
            tiled = 1;
            m_paintBackground = m_tiledBackground;
        }
        ReleaseDC(m_hwnd, dc);
    }
    InvalidateClient();
    return tiled;
}

uint32_t CoolGameWindow::SetPaintBackground(DibBitmap* bitmap, uint32_t overlayId) {
    uint32_t result = 0;
    if (bitmap) {
        m_paintBackground = bitmap;
        InvalidateClient();
        result = AddOverlay(bitmap, overlayId);
    }
    return result;
}

DibBitmap* CoolGameWindow::SetPaintBackgroundById(uint32_t overlayId) {
    DibBitmap* bitmap = FindOverlayBitmap(overlayId);
    if (bitmap) {
        m_paintBackground = bitmap;
        InvalidateClient();
    }
    return bitmap;
}

uint32_t CoolGameWindow::AddOverlay(DibBitmap* bitmap, uint32_t overlayId) {
    uint32_t result = 0;
    void* memory = operator new(0x28);
    void* node = memory ? FUN_0060f4f0(memory) : nullptr;
    if (bitmap && node && bitmap->IsLoaded()) {
        static_cast<IdNode*>(node)->SetId(overlayId);
        using SetBitmap = void(__thiscall*)(void*, DibBitmap*);
        reinterpret_cast<SetBitmap*>(*static_cast<void***>(node))[2](node, bitmap);
        result = reinterpret_cast<IdAvlTree*>(m_overlays)->AddWithId(static_cast<IdNode*>(node));
    }
    return result;
}

DibBitmap* CoolGameWindow::FindOverlayBitmap(uint32_t overlayId) {
    DibBitmap* bitmap = nullptr;
    void* node = reinterpret_cast<BitmapOverlayTree*>(m_overlays)->FindOverlay(overlayId);
    if (node) {
        using GetBitmap = DibBitmap*(__thiscall*)(void*);
        bitmap = reinterpret_cast<GetBitmap*>(*static_cast<void***>(node))[1](node);
    }
    return bitmap;
}

void CoolGameWindow::InvalidateClient() {
    HWND hwnd = m_hwnd;
    if (hwnd) {
        RECT client;
        GetClientRect(hwnd, &client);
        InvalidateRect(hwnd, &client, FALSE);
    }
}

int CoolGameWindow::DrawBackground(HDC dc) {
    DibBitmap* background = m_paintBackground;
    if (background) background->DrawToDc(dc, kBitBltMode, SRCCOPY, 0, 0, 0, 0, 0, 0);
    return background != nullptr;
}

int CoolGameWindow::BuildFrame(HMODULE module, uint32_t topId, uint32_t bottomId, uint32_t leftId,
                                uint32_t rightId, uint32_t topLeftId, uint32_t topRightId, uint32_t bottomLeftId,
                                uint32_t bottomRightId) {
    DeleteBitmap(m_frame);
    m_frame = NewOrNull<DibBitmap>(m_clientWidth, m_clientHeight, m_paintBackground);
    const uint8_t colorKey = m_frame->m_colorKey;
    const int frameHeight = m_frame->Height();
    const int frameWidth = m_frame->Width();
    m_frame->FillRect(0, 0, frameWidth, frameHeight, colorKey);
    DibBitmap* topLeft = LoadResourceBitmap(module, topLeftId, kHelperBitmapKind);
    DibBitmap* top = LoadResourceBitmap(module, topId, kHelperBitmapKind);
    DibBitmap* topRight = LoadResourceBitmap(module, topRightId, kHelperBitmapKind);
    DibBitmap* right = LoadResourceBitmap(module, rightId, kHelperBitmapKind);
    DibBitmap* bottomRight = LoadResourceBitmap(module, bottomRightId, kHelperBitmapKind);
    DibBitmap* bottom = LoadResourceBitmap(module, bottomId, kHelperBitmapKind);
    DibBitmap* bottomLeft = LoadResourceBitmap(module, bottomLeftId, kHelperBitmapKind);
    DibBitmap* left = LoadResourceBitmap(module, leftId, kHelperBitmapKind);
    if (m_frame) {
        topLeft->Blit(m_frame, 0, 0, 0, 0, 0, 0);
        topRight->Blit(m_frame, m_frame->Width() - topRight->Width(), 0, 0, 0, 0, 0);
        int baseline = m_frame->Height();
        const int topHeight = top->Height();
        int x = topRight->Width();
        while (x < m_frame->Width() - topRight->Width()) {
            top->Blit(m_frame, x, 0, 0, 0, top->Width(), top->Height());
            bottom->Blit(m_frame, x, baseline - topHeight, 0, 0, bottom->Width(), bottom->Height());
            x += top->Width();
        }
        bottomLeft->Blit(m_frame, 0, m_height - bottomLeft->Height(), 0, 0, bottomLeft->Width(), bottomLeft->Height());
        const int clientHeight = m_clientHeight;
        const int clientWidth = m_clientWidth;
        bottomRight->Blit(m_frame, clientWidth - bottomRight->Width(), clientHeight - bottomRight->Height(), 0, 0,
                          bottomRight->Width(), bottomRight->Height());
        const int frameWidthNow = m_frame->Width();
        const int rightEdge = bottomRight->Width();
        int y = bottomRight->Height();
        while (y < m_frame->Height() - bottomRight->Height()) {
            left->Blit(m_frame, 0, y, 0, 0, left->Width(), left->Height());
            right->Blit(m_frame, frameWidthNow - rightEdge, y, 0, 0, right->Width(), right->Height());
            y += bottomRight->Height();
        }
        m_frame->m_colorKey = colorKey;
        DeleteBitmap(topLeft);
        DeleteBitmap(top);
        DeleteBitmap(topRight);
        DeleteBitmap(right);
        DeleteBitmap(bottomRight);
        DeleteBitmap(bottom);
        DeleteBitmap(bottomLeft);
        DeleteBitmap(left);
    }
    return m_frame != nullptr;
}

void CoolGameWindow::SetDragMode(uint32_t mode) { m_dragMode = (m_dragMode & ~3u) | mode; }

DibBitmap* CoolGameWindow::LoadBitmapCopy(uint32_t bitmapId) {
    DibBitmap* copy = nullptr;
    HMODULE module = CoolAppGetResourceModule(m_bitmapSlot);
    if (module) {
        DibBitmap* bitmap = NewOrNull<DibBitmap>(module, bitmapId, kResourceBitmapKind);
        if (bitmap->IsLoaded()) {
            HDC dc = GetDC(m_hwnd);
            const int width = bitmap->Width();
            const int height = bitmap->Height();
            copy = NewOrNull<DibBitmap>(static_cast<LONG>(width), static_cast<LONG>(height), bitmap, dc);
            if (copy->IsLoaded()) bitmap->Blit(copy, 0, 0, 0, 0, width, height);
            ReleaseDC(m_hwnd, dc);
        }
        if (bitmap) delete bitmap;
    }
    return copy;
}

CoolWindow* CoolGameWindow::FindChild(uint32_t childId) { return m_children.FindById(childId); }

LRESULT __stdcall CoolGameWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND hwnd = m_hwnd;
    LRESULT result = 0;
    switch (message) {
    case WM_CREATE:
        OnCreate();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        HPALETTE previous = SelectPalette(dc, g_palette, FALSE);
        DrawBackground(dc);
        DrawOverlay(dc);
        SelectPalette(dc, previous, FALSE);
        EndPaint(hwnd, &paint);
        return result;
    }
    case WM_SHOWWINDOW:
        if (HasFlags(1)) SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        return CoolWindow::WindowProc(WM_SHOWWINDOW, wParam, lParam);
    case WM_KEYDOWN:
        OnKeyDown(wParam, lParam);
        return result;
    case WM_COMMAND:
        OnCommand(static_cast<int>(wParam), static_cast<int>(wParam), static_cast<int>(lParam));
        return result;
    case WM_MOUSEMOVE:
        if ((wParam & MK_LBUTTON) && m_dragging) {
            POINT point;
            MakePoint(&point, lParam);
            DragTo(point.x, point.y);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (m_dragging == 0) {
            POINT point;
            MakePoint(&point, lParam);
            BeginDrag(point.x, point.y);
        }
        return 0;
    case WM_CAPTURECHANGED:
        if (reinterpret_cast<HWND>(lParam) == hwnd) return 0;
        if (m_dragging) EndDrag();
        return 0;
    case WM_LBUTTONUP:
        if (m_dragging) EndDrag();
        return 0;
    case WM_PALETTECHANGED:
    case WM_QUERYNEWPALETTE:
        if (message == WM_PALETTECHANGED) {
            InvalidateRect(hwnd, nullptr, FALSE);
            if (hwnd == reinterpret_cast<HWND>(wParam)) return 0;
        }
        {
            HDC dc = GetDC(hwnd);
            HPALETTE previous = SelectPalette(dc, g_palette, FALSE);
            result = RealizePalette(dc);
            SelectPalette(dc, previous, FALSE);
            if (result) InvalidateRect(hwnd, nullptr, FALSE);
            ReleaseDC(hwnd, dc);
        }
        return result;
    }
    return CoolWindow::WindowProc(message, wParam, lParam);
}
