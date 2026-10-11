#include "strategy_ui/menu_window.h"

#include <cstddef>
#include <new>

#include "core/checked_new.h"
#include "core/game_string.h"
#include "gfx/dib_bitmap.h"
#include "ui/cool_label.h"
#include "ui/cool_window.h"

namespace {

constexpr uint32_t kResourceBitmapKind = 10;
constexpr char kBitBltMode = '@';
constexpr UINT kCommandMessage = WM_COMMAND;
constexpr UINT kTimerMessage = WM_TIMER;
constexpr COLORREF kDefaultHotColor = 0x2ffffff;
constexpr COLORREF kDefaultNormalColor = 0x2000000;
constexpr COLORREF kDefaultDisabledColor = 0x2808080;
constexpr UINT_PTR kSubMenuTimerId = 100;
constexpr UINT kSubMenuDelayMs = 500;
constexpr UINT kKeepZOrderFlags = SWP_NOSIZE | SWP_NOMOVE | SWP_NOOWNERZORDER;

HGDIOBJ SelectIntoDc(HDC dc, DibBitmap* bitmap) { return SelectObject(dc, bitmap->DibSectionHandle()); }

int16_t Narrow(int value) { return static_cast<int16_t>(value); }

constexpr size_t kAreaOffset = 0x60;

// BUG(original): the Return key handler invalidates the area of the hot item after it has cleared the pointer, so
// the rectangle is read at address 0x60.
RECT* AreaOf(MenuItemBase* item) { return reinterpret_cast<RECT*>(reinterpret_cast<char*>(item) + kAreaOffset); }

}  // namespace

// ---- Lists ---------------------------------------------------------------------------------------------------

OwningIdList::OwningIdList() {}

OwningIdList::~OwningIdList() {
    Reset();

}

int OwningIdList::IsLessById(LinkNode* a, LinkNode* b) {
    return static_cast<IdLinkNode*>(a)->m_id < static_cast<IdLinkNode*>(b)->m_id;
}

MenuItemList::MenuItemList() {}

MenuItemList::~MenuItemList() {  }

// ---- MenuCommand -----------------------------------------------------------------------------------------------

uint32_t MenuCommand::GetAvailable() const {
    uint32_t command = m_command;
    if (m_flags & 0xd) command = 0;
    return command;
}

// ---- MenuItemBase ----------------------------------------------------------------------------------------------

MenuItemBase::MenuItemBase(HMODULE module, const ResourceStringId* text, int x, int y, int width, int height,
                           uint32_t pictureId, uint32_t hotPictureId, uint32_t disabledPictureId,
                           uint32_t checkPictureId, uint32_t commandId, CoolWindow* owner, int enabled, int checked) {
    m_enabled = enabled;
    m_owner = owner;
    m_checked = checked;
    m_indent = 0;
    DibBitmap* normal = nullptr;
    DibBitmap* hot = nullptr;
    DibBitmap* disabled = nullptr;
    DibBitmap* check = nullptr;
    if (Narrow(pictureId) != 0) {
        normal = NewOrNull<DibBitmap>(module, pictureId, kResourceBitmapKind);
        if (m_enabled == 0 || Narrow(hotPictureId) == 0) {
            if (Narrow(disabledPictureId) == 0) disabledPictureId = pictureId;
            disabled = NewOrNull<DibBitmap>(module, disabledPictureId, kResourceBitmapKind);
        } else {
            hot = NewOrNull<DibBitmap>(module, hotPictureId, kResourceBitmapKind);
        }
    }
    if (Narrow(checkPictureId) != 0) check = NewOrNull<DibBitmap>(module, checkPictureId, kResourceBitmapKind);
    String* label = NewOrNull<String>(text);
    Initialize(commandId, x, y, width, height, normal, hot, disabled, check, label);
}

MenuItemBase::~MenuItemBase() {
    if (m_hotPicture) delete m_hotPicture;
    if (m_normalPicture) delete m_normalPicture;
    if (m_disabledPicture) delete m_disabledPicture;
    if (m_checkOverlay) delete m_checkOverlay;
    if (m_checkPicture) delete m_checkPicture;
    if (m_label) delete m_label;

}

int MenuItemBase::Virtual03() { return 0; }

int MenuItemBase::Virtual04() { return 0; }

void MenuItemBase::Initialize(uint32_t commandId, int x, int y, int width, int height, DibBitmap* normal,
                              DibBitmap* hot, DibBitmap* disabled, DibBitmap* check, String* text) {
    m_id = commandId;
    m_hotPicture = nullptr;
    m_normalPicture = nullptr;
    m_disabledPicture = nullptr;
    m_checkOverlay = nullptr;
    m_checkPicture = nullptr;
    m_backdrop = nullptr;
    m_hit = 0;
    m_hotColor = kDefaultHotColor;
    m_normalColor = kDefaultNormalColor;
    m_disabledColor = kDefaultDisabledColor;
    m_subMenu = nullptr;
    if (normal) m_normalPicture = normal;
    if (hot) m_hotPicture = hot;
    if (disabled) m_disabledPicture = disabled;
    if (check) m_checkPicture = check;
    if (text) {
        int labelX = x;
        if (m_normalPicture) {
            m_pictureX = x;
            m_pictureY = y;
            labelX = m_normalPicture->Width() + 1 + x;
        }
        if (m_checkPicture) {
            m_pictureX = labelX;
            m_pictureY = y;
            labelX = labelX + 1 + m_checkPicture->Width();
        }
        m_label = NewOrNull<CoolLabel>(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
        m_label->SetText(text->m_text);
        m_label->MoveTo(labelX, y);
        delete text;
    }
    if (!m_owner) {
        m_area.left = x;
        m_area.top = y;
        m_area.right = width + x;
        m_area.bottom = height + y;
    } else {
        HWND hwnd = m_owner->m_hwnd;
        HDC dc = GetDC(hwnd);
        m_label->Measure(dc);
        ReleaseDC(hwnd, dc);
        m_area = m_label->m_rect;
        m_area.left = 1;
        m_area.right = m_owner->m_width;
    }
    if (m_normalPicture && Narrow(m_area.bottom) < Narrow(Narrow(m_normalPicture->Height()) + Narrow(m_area.top)))
        m_area.bottom = m_normalPicture->Height() + m_area.top;
    if (m_checkPicture && Narrow(m_area.bottom) < Narrow(Narrow(m_checkPicture->Height()) + Narrow(m_area.top)))
        m_area.bottom = m_checkPicture->Height() + m_area.top;
}

void MenuItemBase::AttachSubMenu(MenuWindowBase* subMenu) {
    m_subMenuY = m_area.top;
    m_subMenuX = m_area.left + 1;
    m_subMenu = subMenu;
}

void MenuItemBase::Draw(HDC dc, int highlighted) {
    DibBitmap* picture;
    if (!m_enabled) {
        m_label->SetColor(m_disabledColor, nullptr);
        picture = m_disabledPicture;
    } else if (!highlighted) {
        m_label->SetColor(m_normalColor, nullptr);
        picture = m_normalPicture;
    } else {
        m_label->SetColor(m_hotColor, nullptr);
        picture = m_hotPicture;
    }
    if (picture) {
        const int height = picture->Height();
        const int width = picture->Width();
        picture->DrawToDc(dc, kBitBltMode, SRCCOPY, m_pictureX, m_pictureY, 0, 0, width, height);
    }
    if (m_checked && m_checkOverlay) {
        const int height = m_checkOverlay->Height();
        const int width = m_checkOverlay->Width();
        m_checkOverlay->DrawToDc(dc, kBitBltMode, SRCCOPY, m_pictureX, m_pictureY, 0, 0, width, height);
    }
    m_label->Draw(dc);
}

int MenuItemBase::GetWidth() {
    int labelWidth = 0;
    if (m_label && m_owner) {
        HDC dc = GetDC(m_owner->m_hwnd);
        m_label->Measure(dc);
        ReleaseDC(m_owner->m_hwnd, dc);
        labelWidth = m_label->m_rect.right - m_label->m_rect.left;
    }
    return m_indent + 0xc + labelWidth;
}

int MenuItemBase::GetHeight() {
    uint32_t pictureHeight = 0;
    uint32_t labelHeight = 0;
    if (m_normalPicture) pictureHeight = m_normalPicture->Height() + 4;
    if (m_label && m_owner) {
        HDC dc = GetDC(m_owner->m_hwnd);
        m_label->Measure(dc);
        ReleaseDC(m_owner->m_hwnd, dc);
        labelHeight = (m_label->m_rect.bottom - m_label->m_rect.top) + 4;
    }
    if (pictureHeight <= labelHeight) pictureHeight = labelHeight;
    return static_cast<int>(pictureHeight);
}

int MenuItemBase::Layout(int y, int width, int height) {
    SetRect(&m_area, 0, y, width, height + y);
    HDC dc = nullptr;
    m_pictureX = 6;
    m_pictureY = y;
    if (m_owner) dc = GetDC(m_owner->m_hwnd);
    if (m_label) {
        m_label->Measure(dc);
        m_label->MoveTo(m_indent + 6,
                        static_cast<int>(static_cast<uint32_t>((m_label->m_rect.top - m_label->m_rect.bottom) + height) >> 1) +
                            y);
        m_label->Measure(dc);
    }
    if (m_owner) ReleaseDC(m_owner->m_hwnd, dc);
    return height + y;
}

void MenuItemBase::TileNormalPicture() {
    DibBitmap* tiled = nullptr;
    if (m_disabledPicture && m_disabledPicture->IsLoaded()) {
        const int width = m_area.right - m_area.left;
        const int height = m_disabledPicture->Height();
        tiled = NewOrNull<DibBitmap>(width, height, m_disabledPicture);
        const int tileWidth = m_disabledPicture->Width();
        int x = 0;
        for (int count = width / tileWidth + 1; count > 0; --count) {
            m_disabledPicture->Blit(tiled, x, 0, 0, 0, 0, 0);
            x += tileWidth;
        }
        if (m_disabledPicture) delete m_disabledPicture;
        m_disabledPicture = tiled;
    }
}

void MenuItemBase::SetColors(COLORREF color, COLORREF shadowColor, COLORREF disabledColor) {
    m_hotColor = color;
    m_normalColor = shadowColor;
    m_disabledColor = disabledColor;
    if (disabledColor == 0) m_disabledColor = shadowColor;
}

void MenuItemBase::BuildCheckOverlay(DibBitmap* menuBackground) {
    if (m_checkOverlay) delete m_checkOverlay;
    m_checkOverlay = ComposeOverlay(m_checkPicture, menuBackground);
}

DibBitmap* MenuItemBase::ComposeOverlay(DibBitmap* picture, DibBitmap* menuBackground) {
    DibBitmap* result = nullptr;
    if (picture && menuBackground && picture->IsLoaded() && menuBackground->IsLoaded()) {
        const int width = picture->Width();
        const int height = picture->Height();
        DibBitmap* overlay = NewOrNull<DibBitmap>(width, height, picture);
        if (overlay && overlay->IsLoaded()) {
            menuBackground->Blit(overlay, 0, 0, m_pictureX, m_pictureY, width, height);
            picture->DrawKeyed(overlay, 0, 0);
            result = overlay;
        }
    }
    return result;
}

int MenuItemBase::GetPictureWidth() {
    if (m_normalPicture) return m_normalPicture->Width();
    if (m_checked && m_checkPicture) return m_checkPicture->Width();
    if (Virtual03()) return 0x14;
    return 0;
}

void MenuItemBase::SetIndent(int indent) { m_indent = indent; }

// ---- MenuItem --------------------------------------------------------------------------------------------------

MenuItem::MenuItem(uint32_t command, uint32_t flags, HMODULE module, const ResourceStringId* text, int x, int y,
                   int width, int height, uint32_t pictureId, uint32_t hotPictureId, uint32_t disabledPictureId,
                   uint32_t checkPictureId, uint32_t entryId, CoolWindow* owner, int enabled, int checked)
    : MenuItemBase(module, text, x, y, width, height, pictureId, hotPictureId, disabledPictureId, checkPictureId,
                   entryId, owner, enabled, checked) {
    m_command.m_command = command;
    m_command.m_flags = flags;
}

MenuItem::~MenuItem() {  }

int MenuItem::Virtual03() { return m_command.m_flags & 8; }

int MenuItem::Virtual04() { return m_command.m_flags & 4; }

// ---- MenuWindowBase --------------------------------------------------------------------------------------------

MenuWindowBase::MenuWindowBase(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                               uint32_t entryId, DibBitmap* backdrop)
    : CoolGameWindow(instance, x, y, width, height, parent, 0, 0, 0, entryId, 0x52000000, 0, 0, 0) {
    m_unknown114 = 0;
    m_hotItem = nullptr;
    m_arrow = nullptr;
    m_dimArrow = nullptr;
    m_remapTable = nullptr;
    m_dc = nullptr;
    m_timer = 0;
    m_parentMenu = nullptr;
    m_style = (m_style & ~static_cast<uint32_t>(WS_VISIBLE)) | 0x6000000;
    m_backdrop = backdrop;
}

MenuWindowBase::~MenuWindowBase() {
    if (m_arrow) delete m_arrow;
    if (m_dimArrow) delete m_dimArrow;
    if (m_remapTable) operator delete(m_remapTable);
    if (m_dc) DeleteDC(m_dc);
    for (MenuItemBase* item = static_cast<MenuItemBase*>(m_items.m_head); item;
         item = static_cast<MenuItemBase*>(item->m_next)) {
        if (MenuWindowBase* sub = item->m_subMenu) {
            sub->DestroyNativeWindow();
            delete sub;
        }
    }
    if (m_timer) {
        KillTimer(m_hwnd, m_timer);
        m_timer = 0;
    }

}

int MenuWindowBase::HandleMouse(MenuItem*, LONG, LONG, int) { return 0; }

void MenuWindowBase::AddTextEntry(HMODULE module, const ResourceStringId* text, uint32_t commandId, int x, int y,
                                  int width, int height, uint32_t pictureId, uint32_t hotPictureId,
                                  uint32_t disabledPictureId, uint32_t checkPictureId) {
    MenuItemBase* item = NewOrNull<MenuItemBase>(module, text, x, y, width, height, pictureId, hotPictureId,
                                                 disabledPictureId, checkPictureId, commandId,
                                                 static_cast<CoolWindow*>(this), 1, 0);
    if (item) {
        item->m_backdrop = m_paintBackground;
        m_items.AddWithId(item);
    }
}

void MenuWindowBase::SetEntryColors(COLORREF color, COLORREF shadowColor, COLORREF disabledColor) {
    for (MenuItemBase* item = static_cast<MenuItemBase*>(m_items.m_head); item;
         item = static_cast<MenuItemBase*>(item->m_next))
        item->SetColors(color, shadowColor, disabledColor);
}

void MenuWindowBase::OnCreate1() {
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, kKeepZOrderFlags);
    HDC windowDc = GetDC(m_hwnd);
    m_dc = CreateCompatibleDC(windowDc);
    DibBitmap* background = NewOrNull<DibBitmap>(static_cast<LONG>(m_clientWidth), static_cast<LONG>(m_clientHeight),
                                                 m_backdrop, m_dc);
    m_remapTable = background->BuildRemapTable(DimMenuColor);
    SetPaintBackground(background, 1);
    ReleaseDC(m_hwnd, windowDc);
}

void MenuWindowBase::AddItem(MenuItemBase* item) {
    if (item) {
        item->m_backdrop = m_paintBackground;
        m_items.AddWithId(item);
    }
}

void MenuWindowBase::AttachSubMenu(HMODULE resources, uint32_t itemId, MenuWindowBase* sub, uint32_t arrowId,
                                   uint32_t pressedArrowId) {
    for (MenuItemBase* item = static_cast<MenuItemBase*>(m_items.m_head); item;
         item = static_cast<MenuItemBase*>(item->m_next)) {
        if (item->m_id != itemId) continue;
        item->AttachSubMenu(sub);
        int bottomLimit;
        if (m_dragLimits.left == 0 || m_dragLimits.top == 0)
            bottomLimit = m_y + m_clientHeight;
        else
            bottomLimit = m_dragLimits.bottom;
        sub->m_parentMenu = this;
        int x = (m_x - sub->m_width) + 3;
        if (x < 0) x = m_width - 3 + m_x;
        const int subHeight = sub->m_clientHeight;
        int y;
        if (item->m_area.top + subHeight + m_y < bottomLimit)
            y = item->m_area.top - 3 + m_y;
        else
            y = (m_y + item->m_area.bottom) - subHeight + 3;
        MoveWindow(sub->m_hwnd, x, y, sub->m_clientWidth, subHeight, FALSE);
        break;
    }
    if (!m_arrow) {
        m_arrow = NewOrNull<DibBitmap>(resources, arrowId, kResourceBitmapKind);
        m_dimArrow = NewOrNull<DibBitmap>(resources, pressedArrowId, kResourceBitmapKind);
    }
}

LRESULT MenuWindowBase::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND hwnd = m_hwnd;
    const POINT mouse = {static_cast<LONG>(lParam & 0xffff), static_cast<LONG>(static_cast<uint32_t>(lParam) >> 16)};
    const uint32_t parentId = m_parentMenu ? m_parentMenu->m_childId : m_childId;
    if (message < 0x19) {
        if (message == WM_SHOWWINDOW) {
            if (wParam != 0) {
                SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, kKeepZOrderFlags);
                m_timer = SetTimer(hwnd, kSubMenuTimerId, kSubMenuDelayMs, nullptr);
                return 0;
            }
            if (!m_hotItem) return 0;
            InvalidateRect(hwnd, AreaOf(m_hotItem), FALSE);
            if (m_hotItem->m_subMenu) m_hotItem->m_subMenu->Hide();
            UINT_PTR timer = m_timer;
            m_hotItem = nullptr;
            if (!timer) return 0;
            KillTimer(hwnd, timer);
            m_timer = 0;
            return 0;
        }
        if (message == WM_PAINT) {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            HPALETTE savedPalette = SelectPalette(dc, g_palette, FALSE);
            SelectCurrentFont(m_dc);
            const int x = m_x;
            const int y = m_y;
            const int height = m_paintBackground->Height();
            const int width = m_paintBackground->Width();
            m_backdrop->Blit(m_paintBackground, 0, 0, Narrow(x) < 0 ? 0 : x, Narrow(y) < 0 ? 0 : y, width, height);
            const int blendHeight = m_paintBackground->Height();
            const int blendWidth = m_paintBackground->Width();
            m_paintBackground->ApplyRemapTable(0, 0, static_cast<int16_t>(blendWidth), static_cast<int16_t>(blendHeight));
            HGDIOBJ savedBitmap = SelectIntoDc(m_dc, m_paintBackground);
            for (MenuItemBase* item = static_cast<MenuItemBase*>(m_items.m_head); item;
                 item = static_cast<MenuItemBase*>(item->m_next)) {
                if (MenuWindowBase* sub = item->m_subMenu) {
                    if (item == m_hotItem) {
                        m_arrow->DrawKeyed(m_paintBackground, item->m_area.left + 6, item->m_area.top);
                    } else {
                        m_dimArrow->DrawKeyed(m_paintBackground, item->m_area.left + 6, item->m_area.top);
                        sub->Hide();
                    }
                }
                item->BuildCheckOverlay(m_paintBackground);
                item->Draw(m_dc, 0);
            }
            if (m_hotItem) m_hotItem->Draw(m_dc, 1);
            SelectObject(m_dc, savedBitmap);
            if (m_frame) m_frame->DrawKeyed(m_paintBackground, 0, 0);
            m_paintBackground->DrawToDc(dc, kBitBltMode, SRCCOPY, 0, 0, 0, 0, 0, 0);
            SelectPalette(dc, savedPalette, FALSE);
            RestoreFont(m_dc);
            EndPaint(hwnd, &paint);
            return 0;
        }
        if (message == WM_CLOSE) {
            UINT_PTR timer = m_timer;
            if (!timer) return 0;
            KillTimer(hwnd, timer);
            m_timer = 0;
            return 0;
        }
    } else if (message < 0x114) {
        if (message == kTimerMessage) {
            MSG pending;
            if (PeekMessageA(&pending, hwnd, 0, 0, PM_NOREMOVE)) return 0;
            if (!m_hotItem) return 0;
            MenuWindowBase* sub = m_hotItem->m_subMenu;
            if (!sub) return 0;
            sub->Show(SW_SHOW);
            return 0;
        }
        if (message == WM_KEYDOWN) {
            if (wParam != VK_RETURN) {
                if (wParam != VK_ESCAPE) {
                    m_parent->WindowProc(WM_KEYDOWN, wParam, lParam);
                    return 0;
                }
                SetFocus((m_parentMenu ? m_parentMenu : m_parent)->m_hwnd);
                PostCloseRequest();
                return 0;
            }
            MenuItemBase* hot = m_hotItem;
            if (!hot) return 0;
            const int centerX = (hot->m_area.right - hot->m_area.left) / 2 + hot->m_area.left;
            const int centerY = (hot->m_area.bottom - hot->m_area.top) / 2 + hot->m_area.top;
            if (HandleMouse(static_cast<MenuItem*>(hot), centerX, centerY, WM_LBUTTONUP) != 0) return 0;
            hot = m_hotItem;
            if (!hot) return 0;
            if (!hot->m_subMenu) {
                PostMessageA(m_parent->m_hwnd, kCommandMessage, (hot->m_id << 16) | (parentId & 0xffff), 0);
                m_hotItem = nullptr;
            }
            InvalidateRect(hwnd, AreaOf(m_hotItem), FALSE);
            return 0;
        }
    } else if (message == WM_MOUSEMOVE) {
        for (MenuItemBase* item = static_cast<MenuItemBase*>(m_items.m_head); item;
             item = static_cast<MenuItemBase*>(item->m_next)) {
            if (!item->m_enabled) continue;
            const BOOL hit = PtInRect(&item->m_area, mouse);
            item->m_hit = hit;
            if (hit && item != m_hotItem) {
                if (m_hotItem) InvalidateRect(hwnd, &m_hotItem->m_area, FALSE);
                InvalidateRect(hwnd, &item->m_area, FALSE);
                m_hotItem = item;
                break;
            }
        }
        if (MenuItemBase* hot = m_hotItem) {
            const BOOL hit = PtInRect(&hot->m_area, mouse);
            hot->m_hit = hit;
            if (!hit) {
                InvalidateRect(hwnd, &m_hotItem->m_area, FALSE);
                m_hotItem = nullptr;
            }
        }
        message = WM_MOUSEMOVE;
    } else if (message == WM_LBUTTONUP) {
        if (!m_hotItem) return 0;
        if (HandleMouse(static_cast<MenuItem*>(m_hotItem), Narrow(static_cast<int>(lParam)),
                        Narrow(static_cast<int>(static_cast<uint32_t>(lParam) >> 16)), WM_LBUTTONUP) == 0 &&
            m_hotItem) {
            if (m_hotItem->m_subMenu) return 0;
            PostMessageA(m_parent->m_hwnd, kCommandMessage, (m_hotItem->m_id << 16) | (parentId & 0xffff), 0);
            m_hotItem = nullptr;
            return 0;
        }
        message = WM_LBUTTONUP;
    }
    return CoolGameWindow::WindowProc(message, wParam, lParam);
}

uint32_t DimMenuColor(uint32_t color) {
    constexpr uint32_t kRedBlueMask = 0xff;
    const auto dim = [](uint32_t channel) { return static_cast<uint8_t>((channel & kRedBlueMask) / 6 + 0x32); };
    // BUG(original): the fourth byte of the result is whatever the caller left in ECX; it is never read
    // (BuildRemapTable compares three bytes).
    return dim(color) | dim(color >> 8) << 8 | dim(color >> 16) << 16;
}
