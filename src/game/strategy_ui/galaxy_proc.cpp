#include "core/reb_abi.h"
#include <cstdint>
#include <new>

#include "app/rebapp_globals.h"
#include "audio/music.h"
#include "audio/sound_manager.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "events/game_clock_hooks.h"
#include "gameobj/object_key.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/object_menu.h"
#include "strategy_ui/panel_drag_button.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_game_window.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"
#include "ui/cool_window.h"
#include "ui/custom_dialog_box.h"
#include "ui/game_window_object.h"
#include "ui/message_template.h"
#include "ui/object_list_item.h"
#include "ui/sprite_stage.h"

// The Galactic Information Display menu (0x451130, window_type6_and_panel).
CoolGameWindow* REB_THISCALL FUN_00451130(void* memory, HINSTANCE instance, int x, int y, int width, int height,
                                        int side, CoolWindow* parent, uint32_t childId, DibBitmap* background);

namespace {

constexpr uint32_t kTextSlot = 2;
constexpr uint32_t kSpriteSlot = 7;
constexpr uint32_t kCommandF1 = 0x70;
constexpr uint32_t kKeyF1 = 0x70;
constexpr uint32_t kKeyF7 = 0x76;
constexpr uint32_t kMouseModeNormal = 1;
constexpr uint32_t kMouseModeTargeting = 2;
constexpr uint32_t kMouseModeRestoring = 8;
constexpr uint32_t kStringGroup = 2;
constexpr uint32_t kHoverRightMargin = 0x3c;
constexpr uint32_t kTickCountReady = 2;

int LowWord(LPARAM value) { return static_cast<int>(static_cast<uint32_t>(value) & 0xffff); }
int HighWord(LPARAM value) { return static_cast<int>(static_cast<uint32_t>(value) >> 16); }

ResourceStringId StringId(uint16_t id) { return ResourceStringId{static_cast<int16_t>(id), kStringGroup}; }

HWND SendToBottom() { return HWND_BOTTOM; }

bool InputBlocked() { return static_cast<uint32_t>(GameHookTickCount()) < kTickCountReady; }

struct TopBarLayout {
    COLORREF m_color;
    int m_dateX;
    int m_dateY;
    POINT m_counters[3];
    RECT m_counterTips[3];
    RECT m_dateTip;
};

constexpr TopBarLayout kAllianceBar = {0x20000ff,
                                       0x68,
                                       0x12,
                                       {{0x109, 0x10}, {0x168, 0x10}, {0x1c9, 0x10}},
                                       {{0xf0, 0x10, 0x14a, 0x21}, {0x14f, 0x10, 0x1a9, 0x21}, {0x1ae, 0x10, 0x208, 0x21}},
                                       {0x67, 0x15, 0xb9, 0x21}};

constexpr TopBarLayout kEmpireBar = {0x200ff00,
                                     500,
                                     0x12,
                                     {{0xaf, 0x13}, {0x111, 0x13}, {0x173, 0x13}},
                                     {{0x95, 0x12, 0xf1, 0x25}, {0xf8, 0x12, 0x154, 0x25}, {0x159, 0x12, 0x1b5, 0x25}},
                                     {499, 0x13, 0x244, 0x1f}};

constexpr uint32_t kCounterCommands[3] = {kGalaxyRawMaterials, kGalaxyRefinedMaterials, kGalaxyMaintenance};
constexpr uint16_t kCounterTips[3] = {0x1510, 0x1511, 0x1512};
constexpr uint16_t kDateTip = 0x1513;
constexpr uint32_t kDateTipCommand = 0x14f;

}  // namespace

LRESULT REB_STDCALL GalaxyWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT:
            return OnPaint();
        case WM_CREATE:
            return OnCreate();
        case WM_KEYDOWN:
            return OnKeyDown(wParam, lParam);
        case WM_SHOWWINDOW:
            return OnShowWindow(wParam, lParam);
        case WM_MOUSEMOVE:
            return OnMouseMove(wParam, lParam);
        case WM_COMMAND:
            return OnCommandMessage(wParam, lParam);
        case kGalaxyMovieDone:
            return OnMovieDone();
        case WM_LBUTTONUP:
            return OnLeftButtonUp(wParam, lParam);
        case WM_LBUTTONDBLCLK:
            return OnLeftDoubleClick(wParam, lParam);
        case WM_RBUTTONUP:
            return OnRightButtonUp(wParam, lParam);
        case kGalaxyCloseChild:
            OnCloseChild(wParam);
            return 0;
        case kGalaxyClosePopups:
            return DismissMenu();
        case kGalaxyActivateChild:
            OnActivateChild(wParam);
            return 0;
        case kGalaxyOpenModal:
        case kGalaxyPlayMovie:
            return OnModalRequest(message, wParam, lParam);
        case kGalaxyAccelerator:
            OnAccelerator(wParam);
            break;
        default:
            break;
    }
    return GameWindowObject::WindowProc(message, wParam, lParam);
}

LRESULT GalaxyWindow::OnCreate() {
    HMODULE spriteModule = CoolAppGetResourceModule(kSpriteSlot);
    HMODULE textModule = CoolAppGetResourceModule(kTextSlot);
    SetMessageTemplateSource(0xbb9, 0xbb8, textModule);
    HWND parentHwnd = m_parent->m_hwnd;
    AppSetVolume(AppGetVolume());
    SoundManager::Initialize(parentHwnd, 1, 0);
    BuildBackground(0);
    m_childId = (m_side != 1 ? 1 : 0) + 7;
    if (GetBackground()) {
        HDC screen = GetDC(m_hwnd);
        SetIdentityPalette(screen, GetBackground()->CreatePaletteFromColorTable());
        m_offscreenDc = CreateCompatibleDC(screen);
        void* memory = ::operator new(sizeof(DibBitmap));
        m_offscreen = memory ? new (memory) DibBitmap(m_clientWidth, m_clientHeight, GetBackground(), screen) : nullptr;
        ReleaseDC(m_hwnd, screen);
    }
    const TopBarLayout& bar = m_side == 1 ? kAllianceBar : kEmpireBar;
    m_dateLabel.MoveTo(bar.m_dateX, bar.m_dateY);
    CoolLabel* counters[3] = {&m_resourceLabels[0], &m_resourceLabels[1], &m_resourceLabels[2]};
    for (CoolLabel* counter : counters) counter->m_width = 0x3f;
    for (CoolLabel* counter : counters) counter->m_height = 0x11;
    for (int i = 0; i < 3; ++i) counters[i]->MoveTo(bar.m_counters[i].x, bar.m_counters[i].y);
    RECT counterTips[3];
    RECT dateTip;
    static constexpr int kTipRectOrder[3] = {1, 0, 2};
    for (int i : kTipRectOrder) {
        const RECT& tip = bar.m_counterTips[i];
        SetRect(&counterTips[i], tip.left, tip.top, tip.right, tip.bottom);
    }
    SetRect(&dateTip, bar.m_dateTip.left, bar.m_dateTip.top, bar.m_dateTip.right, bar.m_dateTip.bottom);

    m_dateLabel.SetColor(bar.m_color, nullptr);
    m_dateLabel.m_width = 0x3c;
    m_dateLabel.m_drawFlags = 1;
    m_dateLabel.m_layoutFlags = kLabelFixedWidth;
    m_dateLabel.SetFont(10);
    const ResourceStringId dateTipText = StringId(kDateTip);
    AddRectToolFromString(dateTip, kDateTipCommand, &dateTipText);
    for (int i = 0; i < 3; ++i) {
        counters[i]->SetFont(8);
        counters[i]->m_layoutFlags = kLabelFixedWidth | kLabelFixedHeight;
        counters[i]->m_drawFlags = 0x26;
        counters[i]->SetColor(bar.m_color, nullptr);
        const ResourceStringId tipText = StringId(kCounterTips[i]);
        AddRectToolFromString(counterTips[i], kCounterCommands[i], &tipText);
    }
    CreateConsoleButtons(spriteModule);
    DisableControls();
    CreateConsoleButtonWindows(textModule);
    LoadGidIcons(spriteModule);
    CreateDroidAnimations();
    m_normalCursor = g_defaultCursor ? g_defaultCursor : LoadCursorA(nullptr, MAKEINTRESOURCEA(0x7f00));
    m_targetCursor = LoadCursorA(m_instance, MAKEINTRESOURCEA(0x3ea));
    if (AppGetCommandLineFlags() & 0x100) {
        m_latencyLabel.m_width = 500;
        m_latencyLabel.m_height = 10;
        m_latencyLabel.MoveTo(0x46, 0);
        m_latencyLabel.SetFont(4);
        m_latencyLabel.m_layoutFlags = kLabelFixedWidth | kLabelFixedHeight;
        m_latencyLabel.m_drawFlags = 0x25;
        m_latencyLabel.SetColor(bar.m_color, nullptr);
    }
    InitMusic();
    if (m_restoring == 0) {
        StartMusic();
        m_restoring = 0;
    }
    m_created = 1;
    UiSetSetupDone();
    return 0;
}

LRESULT GalaxyWindow::OnPaint() {
    PAINTSTRUCT paint;
    HDC paintDc = BeginPaint(m_hwnd, &paint);
    HGDIOBJ previousBitmap = SelectObject(m_offscreenDc, m_offscreen->DibSectionHandle());
    HPALETTE previousPalette = SelectPalette(m_offscreenDc, g_palette, FALSE);
    SelectPalette(paintDc, g_palette, FALSE);
    Draw(m_offscreenDc);
    m_dateLabel.Draw(m_offscreenDc);
    for (CoolLabel& counter : m_resourceLabels) counter.Draw(m_offscreenDc);
    m_latencyLabel.Draw(m_offscreenDc);
    if (m_droidStage) m_droidStage->Draw(m_offscreen);
    if (m_droidStage2) m_droidStage2->Draw(m_offscreen);
    for (MinimizedWindowSlot& slot : m_minimized) {
        if (slot.m_childId == 0 || !slot.m_icon) continue;
        slot.m_icon->DrawToDc(m_offscreenDc, '@', SRCCOPY, slot.m_area.left, slot.m_area.top, 0, 0, 0, 0);
    }
    const RECT& dirty = paint.rcPaint;
    BitBlt(paintDc, dirty.left, dirty.top, dirty.right - dirty.left, dirty.bottom - dirty.top, m_offscreenDc,
           dirty.left, dirty.top, SRCCOPY);
    if (m_hoveredSector) {
        m_sectorLabel.MoveTo(m_hoveredSector->m_x, m_hoveredSector->m_y);
        m_sectorLabel.SetText(m_hoveredSector->m_text.m_text);
        m_sectorLabel.Measure(paintDc);
        m_sectorLabel.Draw(paintDc);
    }
    SelectPalette(m_offscreenDc, previousPalette, FALSE);
    SelectObject(m_offscreenDc, previousBitmap);
    EndPaint(m_hwnd, &paint);
    return 0;
}

LRESULT GalaxyWindow::OnKeyDown(WPARAM key, LPARAM lParam) {
    bool popupOpen = false;
    if (key >= kKeyF1 && key <= kKeyF7) {
        for (AvlNode* node = m_children.First(); node; node = node->m_successor) {
            if (static_cast<CoolGameWindow*>(node)->m_flags & kGameWindowPopup) {
                popupOpen = true;
                break;
            }
        }
    }
    if (popupOpen) return 0;
    if (GetKeyState(VK_CONTROL) < 0) return 0;
    switch (key) {
        case VK_TAB:
            return 0;
        case VK_F1:
            PostMessageA(m_hwnd, WM_COMMAND, kGalaxyGameControls, 0);
            return 0;
        case VK_F2:
            OpenSystemFinder();
            return 0;
        case VK_F3:
            OpenFleetFinder();
            return 0;
        case VK_F4:
            OpenTroopFinder();
            return 0;
        case VK_F5:
            OpenPersonnelFinder();
            return 0;
        case VK_F6:
            OpenMessageIndex(1, 0x79);
            return 0;
        case VK_F7: {
            ObjectKey object;
            NullableKey type;
            type.m_value = 0;
            OpenEncyclopedia(&object, &type);
            return 0;
        }
        default:
            break;
    }
    return GameWindowObject::WindowProc(WM_KEYDOWN, key, lParam);
}

LRESULT GalaxyWindow::OnShowWindow(WPARAM shown, LPARAM lParam) {
    if (shown == 0) {
        if (m_tickTimer) m_tickTimer->m_suspended = 1;
        if (m_droidStage) m_droidStage->Disable();
        if (m_droidStage2) m_droidStage2->Disable();
    } else {
        if (m_tickTimer) {
            m_tickTimer->m_suspended = 0;
            m_tickTimer->OnTick();
        }
        if (m_droidStage) m_droidStage->Enable();
        if (m_droidStage2) m_droidStage2->Enable();
        if (m_mouseMode & kMouseModeRestoring) m_mouseMode = kMouseModeNormal;
    }
    return GameWindowObject::WindowProc(WM_SHOWWINDOW, shown, lParam);
}

LRESULT GalaxyWindow::OnMouseMove(WPARAM keys, LPARAM position) {
    POINT cursor = {LowWord(position), HighWord(position)};
    if (m_mouseMode != kMouseModeNormal && !(keys & MK_SHIFT)) {
        SetCursor(m_currentCursor);
        return 0;
    }
    for (CoolListItem* node = m_sectorItems.First(); node; node = node->Next()) {
        ObjectListItem* item = static_cast<ObjectListItem*>(node);
        RECT area = item->m_rect;
        const BOOL inside = PtInRect(&area, cursor);
        if (item->m_state.m_value & 1) {
            if (!inside) {
                area.right += kHoverRightMargin;
                InvalidateRect(m_hwnd, &area, FALSE);
                item->m_state.m_value &= ~1u;
                m_hoveredSector = nullptr;
                continue;
            }
        } else if (!inside) {
            continue;
        }
        if (m_hoveredSector != item) {
            area.right += kHoverRightMargin;
            InvalidateRect(m_hwnd, &area, FALSE);
            item->m_state.m_value |= 1;
            m_hoveredSector = item;
            break;
        }
    }
    SetCursor(m_normalCursor);
    return 0;
}

LRESULT GalaxyWindow::OnMovieDone() {
    if (!m_pendingMovie) return 0;
    HWND parentHwnd = m_parent->m_hwnd;
    UiFinishSync(reinterpret_cast<PlayerMessage*>(m_pendingMovie));
    m_pendingMovie = 0;
    AppSetVolume(AppGetVolume());
    SoundManager::Initialize(parentHwnd, 1, 0);
    InitMusic();
    m_musicPending = 1;
    StartMusic();
    return 0;
}
