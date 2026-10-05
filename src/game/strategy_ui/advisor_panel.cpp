#include "strategy_ui/advisor_panel.h"

#include <cstdint>
#include <new>

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_label.h"
#include "ui/cool_window.h"

uint32_t LAB_004ac550(uint32_t color);

namespace {

constexpr uint32_t kArtSlot = 7;
constexpr uint32_t kStringSlot = 2;
constexpr uint32_t kPanelStyle = 0x56000000;
constexpr uint32_t kStyleVisible = 0x10000000;
constexpr uint32_t kCloseId = 1;
constexpr uint32_t kRcData = 10;
constexpr uint32_t kBitmapKind = 10;

void DeleteBitmap(DibBitmap* bitmap) {
    if (!bitmap) return;
    bitmap->~DibBitmap();
    ::operator delete(bitmap);
}

uint32_t ResourceOfKind(uint32_t kind, int side) {
    switch (kind) {
    case 0x11: return side == 1 ? 0x27c7 : 0x27c8;
    case 0x12: return 0x2d50;
    case 0x21: return 0x2d51;
    case 0x22: return 0x2d52;
    case 0x30: return 0x27d9;
    case 0x43: return 0x2d53;
    case 0x44: return 0x2d54;
    case 0x51: return 0x27cc;
    case 0x52: return 0x27cd;
    case 0x53: return 0x27ce;
    case 0x54: return 0x27cf;
    case 0x61: return 0x27d0;
    case 0x62: return 0x27d1;
    case 0x63: return 0x27d2;
    case 0x64: return 0x27d3;
    case 0x65: return 0x2d55;
    case 0x66: return 0x2d56;
    case 0x67: return 0x2d57;
    case 0x71: return 0x27d4;
    case 0x72: return 0x27d5;
    case 0x73: return 0x27d6;
    case 0x74: return 0x27d7;
    case 0x75: return 0x27d8;
    default: return 0;
    }
}

}  // namespace

AdvisorPanel::AdvisorPanel(HINSTANCE instance, int x, int y, int width, int height, GalaxyWindow* parent,
                           uint32_t childId, uint32_t kind)
    : CoolGameWindow(instance, x, y, width, height, parent, kStringSlot, kArtSlot, 0, childId, kPanelStyle, 0, 0, 0) {
    m_style &= ~kStyleVisible;
    CoolGameWindow::m_flags |= 4;
    m_kind = kind;
    m_galaxy = parent;
    SetDragLimits(parent->m_panelArea, parent->m_hwnd);
    m_closeButton = nullptr;
    m_layer = nullptr;
    m_remapTable = nullptr;
}

AdvisorPanel::~AdvisorPanel() {
    REB_KEEP_VPTR_STORE();
    delete m_closeButton;
    DeleteBitmap(m_layer);
    if (m_remapTable) ::operator delete(m_remapTable);
    DeleteBitmap(m_paintBackground);
}

void AdvisorPanel::DrawOverlay(HDC dc) {
    m_galaxy->m_offscreen->Blit(m_paintBackground, 0, 0, m_x, m_y, m_clientWidth, m_clientHeight);
    m_paintBackground->ApplyRemapTable(0, 0, static_cast<int16_t>(m_clientWidth), static_cast<int16_t>(m_clientHeight));
    if (m_layer) m_layer->DrawKeyed(m_paintBackground, 0, 0);
    if (m_frame) m_frame->DrawKeyed(m_paintBackground, 0, 0);
    m_paintBackground->DrawToDc(dc, 0x40, SRCCOPY, 0, 0, 0, 0, 0, 0);
}

void AdvisorPanel::OnCreate1() {
    HMODULE art = CoolAppGetResourceModule(kArtSlot);
    m_paintBackground = NewOrNull<DibBitmap>(m_clientWidth, m_clientHeight, m_galaxy->m_offscreen);
    m_remapTable = m_paintBackground->BuildRemapTable(LAB_004ac550);
    const uint32_t resourceId = ResourceOfKind(m_kind, m_galaxy->m_side);
    if (resourceId == 0) return;
    BuildFromResource(art, resourceId);
    BuildFrame(art, 0x2779, 0x277a, 0x277b, 0x2778, 0x2775, 0x2774, 0x2777, 0x2776);
    DibBitmap* closeImage = NewOrNull<DibBitmap>(art, 0x277cu, 2u);
    if (!closeImage) return;
    m_closeButton = NewOrNull<CoolStrobeButton>(art, (m_width - closeImage->Width()) - 3, 3, closeImage->Width(),
                                                closeImage->Height(), this, kCloseId, 0x277cu, 0x277du,
                                                GetPaintBackgroundIfOwned(), 0, 1);
    if (m_closeButton) {
        m_closeButton->Create(0);
        DeleteBitmap(closeImage);
    }
}

void AdvisorPanel::OnCommand(int a, int b, int c) {
    if (static_cast<int16_t>(a) == static_cast<int16_t>(kCloseId)) PostCloseRequest();
}

LRESULT __stdcall AdvisorPanel::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_LBUTTONUP) InvalidateRect(m_hwnd, nullptr, FALSE);
    return CoolGameWindow::WindowProc(message, wParam, lParam);
}

void AdvisorPanel::BuildFromResource(HMODULE module, uint32_t resourceId) {
    HDC memoryDc = nullptr;
    HGDIOBJ previousBitmap = nullptr;
    HRSRC resource = FindResourceA(module, MAKEINTRESOURCEA(resourceId & 0xffff), MAKEINTRESOURCEA(kRcData));
    if (resource) {
        HGLOBAL data = LoadResource(module, resource);
        const uint16_t* entry = static_cast<const uint16_t*>(LockResource(data));
        const uint16_t width = entry[0];
        const uint16_t height = entry[1];
        if (width < static_cast<uint16_t>(m_clientWidth) || height < static_cast<uint16_t>(m_clientHeight)) {
            SetWindowPos(m_hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
            m_width = m_clientWidth;
            m_height = m_clientHeight;
            DeleteBitmap(m_paintBackground);
            if (m_remapTable) ::operator delete(m_remapTable);
            m_paintBackground = NewOrNull<DibBitmap>(m_clientWidth, m_clientHeight, m_galaxy->m_offscreen);
            m_remapTable = m_paintBackground->BuildRemapTable(LAB_004ac550);
        }
        HDC window = GetDC(m_hwnd);
        m_layer = NewOrNull<DibBitmap>(static_cast<LONG>(width), static_cast<LONG>(height), m_paintBackground, window);
        memoryDc = CreateCompatibleDC(window);
        previousBitmap = SelectObject(memoryDc, m_layer->DibSectionHandle());
        ReleaseDC(m_hwnd, window);
        HMODULE entryModule = CoolAppGetResourceModule(entry[2]);
        const int16_t stringGroup = static_cast<int16_t>(entry[3]);
        entry += 4;
        for (uint16_t kind = *entry; kind != 0; kind = *entry) {
            if (kind < 3) {
                const uint16_t x = entry[1];
                const uint16_t y = entry[2];
                const uint16_t bitmapId = entry[3];
                entry += 3;
                if (bitmapId != 0) {
                    if (DibBitmap* bitmap = NewOrNull<DibBitmap>(entryModule, static_cast<uint32_t>(bitmapId), kBitmapKind)) {
                        bitmap->DrawKeyed(m_layer, x, y);
                        DeleteBitmap(bitmap);
                    }
                }
            } else if (kind == 3) {
                uint16_t x = entry[1];
                uint16_t y = entry[2];
                const uint16_t stringId = entry[3];
                const uint16_t font = entry[4];
                const uint16_t red = entry[5];
                const uint16_t green = entry[6];
                const uint16_t blue = entry[7];
                entry += 7;
                if (stringId != 0) {
                    const ResourceStringId id = {static_cast<int16_t>(stringId), stringGroup};
                    if (CoolLabel* label = NewOrNull<CoolLabel>(&id, 0, 0, 0, 0, 0u, 0u)) {
                        label->SetColor(((((blue & 0xff) | 0x200) << 8) | (green & 0xff)) << 8 | (red & 0xff), nullptr);
                        label->SetFont(font);
                        if (y == 1) {
                            label->m_width = width;
                            label->m_layoutFlags = 1;
                            label->m_drawFlags = 1;
                            y = 1;
                            x = 0;
                        }
                        label->MoveTo(x, y);
                        label->Draw(memoryDc);
                        label->~CoolLabel();
                        ::operator delete(label);
                    }
                }
            }
            ++entry;
        }
        FreeResource(data);
    }
    if (memoryDc) {
        SelectObject(memoryDc, previousBitmap);
        DeleteObject(memoryDc);
    }
}
