#include <cstdint>
#include <new>

#include "audio/sound_manager.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "economy/manu_mgr.h"
#include "economy/manufacturing_facility.h"
#include "economy/production_facility_types.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/system_window.h"
#include "ui/bitmap_status_bar.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_image.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_string_field.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"
#include "ui/cool_tab_control.h"
#include "ui/cool_window.h"
#include "ui/object_list_item.h"

extern HPALETTE g_palette;
extern const char g_statusSeparator[];  // 0x6a872c ":"

ObjectKey* __thiscall FUN_004025b0(GameObjNotifier* object, ObjectKey* out);
GameObjNotifier* __thiscall FUN_004a25c0(void* window);

namespace {

constexpr uint32_t kArtSlot = 7;
constexpr uint32_t kStringSlot = 2;
constexpr uint32_t kFirstPage = 0x67;
constexpr uint32_t kStyleVisible = 0x10000000;
constexpr UINT kGalaxyRefresh = 0x467;
constexpr UINT kDragNotify = 0x29a;
constexpr UINT kSelectionNotify = 0x29b;
constexpr int kCloseId = 0x65;
constexpr int kSideId = 100;
constexpr int kSectorId = 0xc9;
constexpr int kTabsId = 0x70;
constexpr int kListId = 2;
constexpr int kPanelAreaLeft = 8;
constexpr int kPanelAreaTop = 0x39;
constexpr int kPanelAreaRight = 0xe6;
constexpr int kPanelAreaBottom = 0x11a;
constexpr int kItemWidth = 0x45;
constexpr int kItemHeight = 0x28;
constexpr int kDragSlack = 3;

ResourceStringId StringId(uint16_t id) { return ResourceStringId{static_cast<int16_t>(id), 2}; }

void DeleteBitmap(DibBitmap* bitmap) {
    if (!bitmap) return;
    bitmap->~DibBitmap();
    ::operator delete(bitmap);
}

uint32_t ControlBits(GameObjNotifier* object) { return static_cast<GameObjRoot*>(object)->m_flags; }

uint32_t SelectedChildId(CoolTabControl* tabs) { return tabs->m_selected ? tabs->m_selected->m_childId : 0; }

CoolStrobeButton* NewButton(HMODULE art, int x, int y, int width, int height, CoolWindow* parent, uint32_t childId,
                            uint32_t normalId, uint32_t pressedId, DibBitmap* backdrop) {
    return NewOrNull<CoolStrobeButton>(art, x, y, width, height, parent, childId, normalId, pressedId, backdrop, 0, 1);
}

bool InPanelArea(POINT point) {
    RECT area;
    SetRect(&area, kPanelAreaLeft, kPanelAreaTop, kPanelAreaRight, kPanelAreaBottom);
    return PtInRect(&area, point) != 0;
}

}  // namespace

// ---- Messages ----------------------------------------------------------------------------------------------

LRESULT __stdcall SystemWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND hwnd = m_hwnd;
    HWND galaxyWindow = m_galaxy->m_hwnd;
    const int pointX = static_cast<int>(lParam & 0xffff);
    const int pointY = static_cast<int>(static_cast<uint32_t>(lParam) >> 16);

    if (message == WM_COMMAND) {
        switch (wParam & 0xffff) {
        case 7:
            if (m_renameField) {
                SetFocus(m_renameField->m_hwnd);
                return 0;
            }
            if (m_page == static_cast<int>(kFirstPage)) return 0;
            SetFocus(m_list->m_hwnd);
            return 0;
        case kSideId:
            PostMessageA(galaxyWindow, WM_COMMAND, (m_childId & 0xffff) | 0x4660000, 0);
            return 0;
        case kCloseId:
            PostCloseRequest();
            return 0;
        case kTabsId:
            SelectPage(static_cast<uint32_t>(wParam) >> 16);
            SetFocus(m_page == static_cast<int>(kFirstPage) ? m_hwnd : m_list->m_hwnd);
            return 0;
        case kSectorId: {
            GameObjNotifier* system = FUN_004a25c0(this);
            if (!system || !m_galaxy) return 0;
            ObjectKey sectorKey;
            if (static_cast<GameObj*>(system)->GetValidParentKey(&sectorKey)) m_galaxy->OpenSectorWindow(&sectorKey);
            return 0;
        }
        default:
            break;
        }
        const uint32_t notification = static_cast<uint32_t>(wParam) >> 16;
        if (notification == kDragNotify) {
            POINT point = {pointX, pointY};
            if (WindowFromPoint(point) != hwnd) {
                PostMessageA(galaxyWindow, WM_COMMAND, (m_childId & 0xffff) | (kDragNotify << 16), lParam);
            }
            return 0;
        }
        if (notification == kSelectionNotify) {
            if (m_page != static_cast<int>(kFirstPage)) PostMessageA(galaxyWindow, kGalaxyRefresh, reinterpret_cast<WPARAM>(hwnd), 0);
            return 0;
        }
        return CustomDialogBox::WindowProc(WM_COMMAND, wParam, lParam);
    }

    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        HPALETTE previousPalette = SelectPalette(dc, g_palette, 0);
        DrawBackground(dc);
        DrawContents(paint.rcPaint, dc);
        m_titleLabel->Draw(dc);
        SelectPalette(dc, previousPalette, 0);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_CLOSE:
        SendMessageA(galaxyWindow, kGalaxyRefresh, 0, 0);
        return 0;
    case 0x407:
        m_uiFlags &= ~8u;
        EndSlotRename(m_selectedSlot);
        return 0;
    case kGalaxyRefresh:
        InvalidateRect(hwnd, &m_titleRect, FALSE);
        if (static_cast<HWND>(reinterpret_cast<void*>(lParam)) != hwnd) ClearSlotHighlight(nullptr);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        POINT point = {pointX, pointY};
        if ((m_uiFlags & 8) != 0 && (m_uiFlags & 4) == 0 && m_selectedSlot) {
            m_uiFlags &= ~8u;
            EndSlotRename(m_selectedSlot);
        }
        SendMessageA(galaxyWindow, kGalaxyRefresh, reinterpret_cast<WPARAM>(hwnd), 0);
        RECT panels;
        SetRect(&panels, kPanelAreaLeft, kPanelAreaTop, kPanelAreaRight, kPanelAreaBottom);
        m_uiFlags &= ~4u;
        m_selectedSlot = nullptr;
        if (PtInRect(&panels, point) && m_page == static_cast<int>(kFirstPage)) {
            for (SystemSlot& slot : m_slots) {
                if (PtInRect(&slot.m_rect, point)) {
                    m_selectedSlot = &slot;
                    break;
                }
            }
            if (m_selectedSlot && (wParam & MK_CONTROL) == 0 && (m_selectedSlot->m_flags & 1) == 0) {
                ClearSlotHighlight(nullptr);
                HighlightSlot(m_selectedSlot);
            }
        }
        SetCaptureWindow(hwnd);
        if (m_selectedSlot) return 0;
        return CustomDialogBox::WindowProc(message, wParam, lParam);
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP: {
        POINT point = {pointX, pointY};
        if (message == WM_LBUTTONUP) SetFocus(m_page == static_cast<int>(kFirstPage) ? hwnd : m_list->m_hwnd);
        if (GetCapture() == hwnd) ReleaseCapture();
        RECT panels;
        SetRect(&panels, kPanelAreaLeft, kPanelAreaTop, kPanelAreaRight, kPanelAreaBottom);
        if ((m_uiFlags & 2) != 0) {
            m_uiFlags &= ~2u;
            m_dragImage->EndDrag();
            const int distanceX = m_dragStartX < pointX ? pointX - m_dragStartX : m_dragStartX - pointX;
            const int distanceY = m_dragStartY < pointY ? pointY - m_dragStartY : m_dragStartY - pointY;
            if (distanceX > kDragSlack || distanceY > kDragSlack) {
                PostMessageA(m_galaxy->m_hwnd, WM_COMMAND, (m_childId & 0xffff) | (kDragNotify << 16),
                             static_cast<LPARAM>(GetMessagePos()));
            }
            return 0;
        }
        if (PtInRect(&panels, point) && m_selectedSlot) {
            SystemSlot* slot = m_selectedSlot;
            if ((wParam & MK_CONTROL) != 0) {
                if ((slot->m_flags & 1) == 0) {
                    HighlightSlot(slot);
                } else {
                    ClearSlotHighlight(slot);
                }
            } else {
                if ((m_uiFlags & 4) != 0 && message == WM_LBUTTONUP && slot->m_statusLabel != nullptr) {
                    if ((slot->m_flags & 1) != 0) {
                        const RECT labelArea = slot->m_statusLabel->m_rect;
                        if (PtInRect(&labelArea, point) && m_dragPictures[0] != nullptr) {
                            m_uiFlags = (m_uiFlags & ~4u) | 8;
                            BeginSlotRename(m_selectedSlot);
                        }
                    }
                } else if (slot->m_statusLabel != nullptr) {
                    GameObjNotifier* manager = GetObjectInFamilies_a0_b0(m_galaxy->m_side, &slot->m_managerKey);
                    if (manager) {
                        StatusCode status;
                        static_cast<ManuMgr*>(manager)->CheckProductEvent215(1, m_galaxy->m_side, &status);
                        if (status.m_second == static_cast<uint32_t>(-1)) m_uiFlags |= 4;
                    }
                }
                ClearSlotHighlight(nullptr);
                HighlightSlot(m_selectedSlot);
            }
        }
        return CustomDialogBox::WindowProc(message, wParam, lParam);
    }
    case WM_MOUSEMOVE: {
        POINT point = {pointX, pointY};
        if ((wParam & MK_LBUTTON) != 0 && m_dragging == 0 && (m_uiFlags & 2) == 0) {
            if (InPanelArea(point)) {
                m_uiFlags |= 2;
                if ((wParam & MK_CONTROL) != 0) {
                    for (SystemSlot& slot : m_slots) {
                        if (PtInRect(&slot.m_rect, point) && (slot.m_flags & 1) == 0) {
                            m_selectedSlot = &slot;
                            HighlightSlot(&slot);
                            break;
                        }
                    }
                }
                m_dragStartX = pointX;
                m_dragStartY = pointY;
                if (m_dragImage && m_selectedCount != 0 && m_selectedCount <= 3) {
                    m_dragImage->SetImage(m_dragPictures[m_selectedCount - 1]);
                    m_dragImage->SetHotspot(3, 3);
                    MapWindowPoints(hwnd, galaxyWindow, &point, 1);
                    m_dragImage->BeginDrag(point);
                    return 0;
                }
                m_uiFlags &= ~2u;
                return 0;
            }
        }
        if ((m_uiFlags & 2) != 0) {
            MapWindowPoints(hwnd, galaxyWindow, &point, 1);
            m_dragImage->MoveTo(&point);
            return 0;
        }
        return CustomDialogBox::WindowProc(message, wParam, lParam);
    }
    default:
        return CustomDialogBox::WindowProc(message, wParam, lParam);
    }
}

// ---- Painting ----------------------------------------------------------------------------------------------

void SystemWindow::DrawContents(RECT paintRect, HDC dc) {
    GameObjNotifier* system = FUN_004a25c0(this);
    if (!system) return;
    const int left = m_titleRect.left;
    const int top = m_titleRect.top;
    DibBitmap* header = m_galaxy->m_activeChild == m_hwnd ? m_headerActive : m_headerInactive;
    header->DrawToDc(dc, 0x40, SRCCOPY, left, top, 0, 0, m_titleRect.right - left, m_titleRect.bottom - top);
    CoolWindow* tabWindow = m_tabs->m_children.FindById(SelectedChildId(m_tabs));
    if (m_page == static_cast<int>(kFirstPage)) {
        DibBitmap* backdrop = GetPaintBackgroundIfOwned();
        HDC memoryDc = CreateCompatibleDC(dc);
        for (SystemSlot& slot : m_slots) {
            RECT hit;
            if (!IntersectRect(&hit, &paintRect, &slot.m_rect)) continue;
            backdrop->Blit(m_panelCanvas, 0, 0, slot.m_rect.left, slot.m_rect.top, 0, 0);
            m_panelBackground->DrawKeyed(m_panelCanvas, 0, 0);
            DibBitmap* frame = (slot.m_flags & 1) == 0 ? m_panelNormal : m_panelSelected;
            frame->DrawKeyed(m_panelCanvas, 0, 0);
            HGDIOBJ previous = SelectObject(memoryDc, m_panelCanvas->DibSectionHandle());
            slot.m_headingLabel->Draw(memoryDc);
            if ((slot.m_productKey.m_value & 0xff000000) != 0) {
                slot.m_productPicture->DrawKeyed(m_panelCanvas, 0x28, 0xf);
                slot.m_countLabel->Draw(memoryDc);
            }
            slot.m_statusLabel->Draw(memoryDc);
            slot.m_characterLabel->Draw(memoryDc);
            SelectObject(memoryDc, previous);
            m_panelCanvas->DrawToDc(dc, 0x40, SRCCOPY, slot.m_rect.left, slot.m_rect.top, 0, 0, 0, 0);
        }
        DeleteDC(memoryDc);
        m_countsCanvas->DrawToDc(dc, 0x40, SRCCOPY, 6, 0x47, 0, 0, 0, 0);
        String text;
        struct CountRow {
            int (*m_count)(GameObj*, uint32_t, uint32_t);
            int m_y;
        };
        const CountRow rows[] = {{CountManufacturingFacility28Children, 0x77},
                                 {CountManufacturingFacility29Children, 200},
                                 {CountManufacturingFacility2AChildren, 0x118}};
        for (size_t i = 0; i < 3; ++i) {
            const int available = rows[i].m_count(static_cast<GameObj*>(system), m_controlSide, 1);
            text.AppendSigned(available);
            String separator(g_statusSeparator);
            text += separator;
            const int total = rows[i].m_count(static_cast<GameObj*>(system), m_controlSide, 3);
            text.AppendSigned(total);
            m_countsLabel->MoveTo(6, rows[i].m_y);
            m_countsLabel->SetText(text.m_text);
            m_countsLabel->Draw(dc);
            if (i < 2) text.RemoveRange(0, text.Length());
        }
        return;
    }
    if (m_page > static_cast<int>(kFirstPage) && m_page < 0x6d) {
        m_pageLabel->SetText(tabWindow->m_tooltipText);
        m_pageLabel->Draw(dc);
    }
}

// ---- Creation ----------------------------------------------------------------------------------------------

void SystemWindow::LoadTabArt() {
    HMODULE art = CoolAppGetResourceModule(kArtSlot);
    HDC window = GetDC(m_hwnd);
    DeleteBitmap(m_headerActive);
    DeleteBitmap(m_headerInactive);
    DeleteBitmap(m_tabIcon);
    DeleteBitmap(m_panelSelected);
    DeleteBitmap(m_panelNormal);
    for (DibBitmap*& picture : m_dragPictures) {
        DeleteBitmap(picture);
        picture = nullptr;
    }
    auto load = [&](uint32_t id) { return NewOrNull<DibBitmap>(art, id, 10u); };
    uint32_t normalId = 0;
    uint32_t pressedId = 0;
    if (m_controlSide == 1) {
        m_headerActive = load(0x283b);
        m_headerInactive = load(0x27d8);
        m_tabIcon = load(0x2816);
        m_panelSelected = load(0x2833);
        m_panelNormal = load(0x2834);
        normalId = 0x2848;
        pressedId = 0x2847;
        if (m_galaxy->m_side == 1) {
            m_dragPictures[0] = load(0x2841);
            m_dragPictures[1] = load(0x2842);
            m_dragPictures[2] = load(0x2843);
        }
    } else if (m_controlSide == 2) {
        m_headerActive = load(0x27d9);
        m_headerInactive = load(0x283e);
        m_tabIcon = load(0x2817);
        m_panelSelected = load(0x2835);
        m_panelNormal = load(0x2836);
        normalId = 0x284b;
        pressedId = 0x284a;
        if (m_galaxy->m_side == 2) {
            m_dragPictures[0] = load(0x2844);
            m_dragPictures[1] = load(0x2845);
            m_dragPictures[2] = load(0x2846);
        }
    } else if (m_controlSide == 3) {
        m_headerActive = load(0x283f);
        m_headerInactive = load(0x2840);
        m_tabIcon = load(0x2818);
        m_panelSelected = load(0x2837);
        m_panelNormal = load(0x2838);
        normalId = 0x284e;
        pressedId = 0x284d;
    }
    // BUG(original): for a system nobody controls the bitmap ids are never set.
    if (normalId == 0) {
        ReleaseDC(m_hwnd, window);
        return;
    }
    if (CoolWindow* tab = FindChild(kFirstPage)) {
        CoolStrobeButton* button = static_cast<CoolStrobeButton*>(tab);
        button->LoadStateBitmap(0, normalId);
        button->LoadStateBitmap(1, pressedId);
        button->Show(5);
    }
    ReleaseDC(m_hwnd, window);
}

void SystemWindow::OnCreate1() {
    GameObjNotifier* system = FUN_004a25c0(this);
    m_controlSide = (ControlBits(system) >> 6) & 3;
    String unusedText;
    ObjectKey unusedKeyA;
    ObjectKey unusedKeyB;
    HMODULE art = CoolAppGetResourceModule(kArtSlot);
    HMODULE strings = CoolAppGetResourceModule(kStringSlot);
    m_dragImage = NewOrNull<CoolDragImage>(m_galaxy);
    SetDragLimits(m_galaxy->m_panelArea, nullptr);
    SetDragMode(2);
    g_stringModule = strings;
    ObjectKeyIdNode* ownKey = NewOrNull<ObjectKeyIdNode>();
    ownKey->m_key = m_systemKey;
    ownKey->AddId(m_childId);
    m_objects.Add(ownKey, 0);

    int titleX = 5;
    if (DibBitmap* sectorImage = NewOrNull<DibBitmap>(art, 0x27e1u, 2u)) {
        titleX = sectorImage->Width() + 5;
        DeleteBitmap(sectorImage);
    }
    {
        String name;
        static_cast<GameObj*>(system)->GetNameCopy(&name);
        m_titleLabel->SetText(name.m_text);
    }
    m_titleLabel->SetFont(5);
    m_titleLabel->m_height = 0x10;
    m_titleLabel->m_width = 0xb3;
    m_titleLabel->MoveTo(titleX, 2);
    m_titleLabel->m_drawFlags = 0x24;
    m_titleLabel->SetColor(0x2000000, nullptr);
    SetRect(&m_titleRect, 2, 2, 0xe0, 0x12);
    PostMessageA(m_galaxy->m_hwnd, kGalaxyRefresh, reinterpret_cast<WPARAM>(m_hwnd), 0);

    HDC window = GetDC(m_hwnd);
    DibBitmap* background = NewOrNull<DibBitmap>(art, 0x2839u, 10u);
    SetPaintBackground(background, 1);
    m_panelBackground = NewOrNull<DibBitmap>(art, 0x2832u, 10u);
    m_panelCanvas = NewOrNull<DibBitmap>(static_cast<LONG>(m_panelBackground->Width()),
                                         static_cast<LONG>(m_panelBackground->Height()), m_panelBackground, window);
    LoadTabArt();
    m_pageLabel = NewOrNull<CoolLabel>(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    m_pageLabel->m_layoutFlags = 1;
    m_pageLabel->m_drawFlags = 1;
    m_pageLabel->m_width = m_clientWidth;
    m_pageLabel->SetFont(4);
    m_pageLabel->SetColor(0x2ffffff, nullptr);
    m_pageLabel->MoveTo(0, 0x3a);
    DibBitmap* counts = NewOrNull<DibBitmap>(art, 0x283au, 10u);
    m_countsCanvas = NewOrNull<DibBitmap>(counts->Width(), counts->Height(), counts);
    GetPaintBackgroundIfOwned()->Blit(m_countsCanvas, 0, 0, 6, 0x47, 0, 0);
    counts->DrawKeyed(m_countsCanvas, 0, 0);
    DeleteBitmap(counts);
    m_countsLabel = NewOrNull<CoolLabel>(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    m_countsLabel->SetColor(0x2ffffff, nullptr);
    m_countsLabel->m_drawFlags = 1;
    m_countsLabel->m_layoutFlags = 3;
    m_countsLabel->m_width = 0x2e;
    m_countsLabel->m_height = 0x10;
    ReleaseDC(m_hwnd, window);

    if (DibBitmap* closeImage = NewOrNull<DibBitmap>(art, 0x277cu, 2u)) {
        m_closeButton = NewButton(art, (m_width - closeImage->Width()) - 3, 3, closeImage->Width(), closeImage->Height(),
                                  this, kCloseId, 0x277c, 0x277d, GetPaintBackgroundIfOwned());
        m_closeButton->Create(0);
        DeleteBitmap(closeImage);
    }
    if (DibBitmap* sideImage = NewOrNull<DibBitmap>(art, 0x280du, 2u)) {
        m_sideButton = NewButton(art, m_closeButton->m_x - sideImage->Width(), 3, sideImage->Width(), sideImage->Height(),
                                 this, kSideId, 0x280d, 0x280e, GetPaintBackgroundIfOwned());
        m_sideButton->Create(0);
        DeleteBitmap(sideImage);
    }
    if (DibBitmap* sectorImage = NewOrNull<DibBitmap>(art, 0x27e1u, 2u)) {
        m_sectorButton = NewButton(art, 3, 3, sectorImage->Width(), sectorImage->Height(), this, kSectorId, 0x27e1, 0x27e0,
                                   GetPaintBackgroundIfOwned());
        if (m_sectorButton) m_sectorButton->Create(0);
        DeleteBitmap(sectorImage);
    }

    m_items = NewOrNull<CoolListItemList>(4);
    m_list = NewOrNull<CoolDragList>(art, 8, 0x4d, 0xde, 0xe1, this, GetPaintBackgroundIfOwned(), kListId, kItemWidth,
                                     kItemHeight, 0, 0, 0, 0, 5, 1, nullptr);
    if (m_list) {
        m_list->m_options.m_value = 0xa0000;
        m_list->SetFont(10);
        m_list->m_style &= ~kStyleVisible;
        m_list->Create(0);
        m_list->SetSortMode(4);
        m_list->SetItemList(m_items);
        m_children.Insert(m_list);
    }

    struct SlotSetup {
        int m_number;
        int m_barY;
        uint32_t m_barId;
        int m_managerValue;
    };
    const SlotSetup setups[] = {{1, 0x7f, 0x6d, 0}, {2, 0xd0, 0x6e, 2}, {3, 0x121, 0x6f, 1}};
    for (const SlotSetup& setup : setups) {
        BitmapStatusBar* bar = NewOrNull<BitmapStatusBar>(0x38, setup.m_barY, 0xa0, 4, this, 0x200f0f0, 0x2000000, nullptr, 0, 0);
        Slot(setup.m_number).m_progressBar = bar;
        bar->m_childId = setup.m_barId;
        bar->Create(0);
        GameObjNotifier* manager = static_cast<System*>(system)->FindManufacturingManagerBySlot123(setup.m_managerValue);
        ObjectKeyIdNode* node = NewOrNull<ObjectKeyIdNode>();
        ObjectKey managerKey;
        FUN_004025b0(manager, &managerKey);
        node->m_key = managerKey;
        node->AddId(m_childId);
        m_objects.Add(node, 0);
        InitSlot(setup.m_number, manager);
        if (static_cast<ManuMgr*>(manager)->m_remainingGameObjCount != 0) SetSlotProduction(setup.m_number, manager);
    }

    m_tabs = NewOrNull<CoolTabControl>(m_instance, 0, 0x14, 0xe2, 0x21, kTabsId, this, GetPaintBackgroundIfOwned());
    if (m_tabs) {
        m_tabs->Create(0);
        DibBitmap* tabBackdrop = m_tabs->m_background;
        uint32_t firstNormal = 0;
        uint32_t firstPressed = 0;
        if (m_controlSide == 1) {
            firstNormal = 0x2848;
            firstPressed = 0x2847;
        } else if (m_controlSide == 2) {
            firstNormal = 0x284b;
            firstPressed = 0x284a;
        } else if (m_controlSide == 3) {
            firstNormal = 0x284e;
            firstPressed = 0x284d;
        }
        CoolStrobeButton* firstTab = nullptr;
        if (m_controlSide >= 1 && m_controlSide <= 3) {
            firstTab = NewButton(art, 0, 0, 0x24, 0x21, m_tabs, kFirstPage, firstNormal, firstPressed, tabBackdrop);
        }
        if (firstTab) {
            firstTab->Create(0);
            const ResourceStringId firstTip = StringId(0x1835);
            AddToolFromString(firstTab, &firstTip);
            m_tabs->AddTab(firstTab, 1, 0);
        }

        auto addFacilityTab = [&](int x, uint32_t childId, int count, uint32_t withFacilities, uint32_t without,
                                  uint32_t pressed, uint16_t tooltip) {
            CoolStrobeButton* tab = NewButton(art, x, 0, 0x24, 0x21, m_tabs, childId, count == 0 ? without : withFacilities,
                                              pressed, tabBackdrop);
            tab->Create(0);
            const ResourceStringId tip = StringId(tooltip);
            AddToolFromString(tab, &tip);
            m_tabs->AddTab(tab, 0, 0);
        };
        {
            ProductionFacilityType2CIterator shipyards(system, 3, m_controlSide);
            const int shipyardCount = shipyards.Count();
            addFacilityTab(0xbe, 0x6c, shipyardCount, 0x2851, 0x2852, 0x2850, 0x1826);
            ProductionFacilityType2DIterator trainers(system, 3, m_controlSide);
            const int trainerCount = trainers.Count();
            addFacilityTab(0x98, 0x6b, trainerCount, 0x2854, 0x2855, 0x2853, 0x1827);
            const int mineCount = CountManufacturingFacility28Children(static_cast<GameObj*>(system),
                                                                      (ControlBits(system) >> 6) & 3, 3);
            addFacilityTab(0x27, 0x68, mineCount, 0x2857, 0x2858, 0x2856, 0x1828);
            const int refineryCount =
                CountManufacturingFacility29Children(static_cast<GameObj*>(system), m_controlSide, 3);
            addFacilityTab(0x4d, 0x69, refineryCount, 0x285a, 0x285b, 0x2859, 0x1830);
            const int constructionCount =
                CountManufacturingFacility2AChildren(static_cast<GameObj*>(system), m_controlSide, 3);
            addFacilityTab(0x73, 0x6a, constructionCount, 0x285d, 0x285e, 0x285c, 0x1832);
        }
    }
    m_tabs->SelectTab(kFirstPage, 1);
    if ((m_galaxy->m_mouseMode & 4) == 0) {
        SoundRequest request(0, 0, 0, 0, 0, 0);
        request.m_kind = 1;
        request.m_soundId = 0x25e;
        request.m_moduleIndex = 7;
        SoundManager::Play(&request);
    }
}

// ---- Pages -------------------------------------------------------------------------------------------------

int SystemWindow::SelectPage(int page) {
    HMODULE art = CoolAppGetResourceModule(kArtSlot);
    GameObjNotifier* system = FUN_004a25c0(this);
    if (!system) return 0;
    if (m_page == page) return 0;
    (void)m_children.First();
    if (page == static_cast<int>(kFirstPage)) {
        ShowProgressBars(0);
        m_list->Hide();
    } else {
        String unusedText;
        ShowProgressBars(1);
        for (CoolListItem* item = m_items->First(); item; item = item->Next()) {
            ObjectKey key;
            static_cast<ObjectListItem*>(item)->GetKey(&key);
            m_objects.RemoveKey(&key);
        }
        m_items->Clear();
        bool hasPictures = false;
        uint8_t colorKey = 0;
        auto addFacility = [&](GameObjNotifier* facility) {
            ObjectKey key;
            FUN_004025b0(facility, &key);
            ObjectListItem* item = NewOrNull<ObjectListItem>(&key);
            DibBitmap* picture = SystemObjectPicture(facility);
            DibBitmap* normal = NewOrNull<DibBitmap>(kItemWidth, kItemHeight, picture);
            DibBitmap* selected = NewOrNull<DibBitmap>(kItemWidth, kItemHeight, normal);
            colorKey = picture->m_colorKey;
            normal->FillRect(0, 0, kItemWidth, kItemHeight, colorKey);
            selected->FillRect(0, 0, kItemWidth, kItemHeight, colorKey);
            hasPictures = true;
            const int x = (kItemWidth - picture->Width()) / 2;
            const int y = (kItemHeight - picture->Height()) / 2;
            picture->Blit(normal, x, y, 0, 0, 0, 0);
            picture->Blit(selected, x, y, 0, 0, 0, 0);
            m_tabIcon->DrawKeyed(selected, x, y);
            item->SetImages(normal, selected);
            DeleteBitmap(picture);
            ObjectKeyIdNode* node = NewOrNull<ObjectKeyIdNode>(&key, 0u);
            m_objects.Add(node, 0);
            m_items->Insert(item);
        };
        auto addFacilities = [&](ChildIterator& facilities) {
            for (facilities.Begin(); facilities.Current(); facilities.Advance()) {
                GameObjNotifier* facility = facilities.Current();
                if ((static_cast<GameObj*>(facility)->m_stateFlags & 8) == 0) addFacility(facility);
            }
        };
        switch (page) {
        case 0x68: {
            ManufacturingFacility28Iterator facilities(system, 3, m_controlSide);
            addFacilities(facilities);
            break;
        }
        case 0x69: {
            ManufacturingFacility29Iterator facilities(system, 3, m_controlSide);
            addFacilities(facilities);
            break;
        }
        case 0x6a: {
            ManufacturingFacility2AIterator facilities(system, 3, m_controlSide);
            addFacilities(facilities);
            break;
        }
        case 0x6b: {
            ProductionFacilityType2DIterator facilities(system, 3, m_controlSide);
            addFacilities(facilities);
            break;
        }
        case 0x6c: {
            ProductionFacilityType2CIterator facilities(system, 3, m_controlSide);
            int shown = 0;
            for (facilities.Begin(); facilities.Current(); facilities.Advance()) {
                GameObjNotifier* facility = facilities.Current();
                ++shown;
                if ((static_cast<GameObj*>(facility)->m_stateFlags & 8) == 0) addFacility(facility);
            }
            for (; shown < static_cast<System*>(system)->m_rawMaterial; ++shown) {
                DibBitmap* placeholder = NewOrNull<DibBitmap>(art, 0x232du, 10u);
                ObjectKey unset;
                ObjectListItem* item = NewOrNull<ObjectListItem>(&unset);
                item->SetImages(placeholder, nullptr);
                item->m_state.m_value |= kListItemDisabled;
                m_items->Insert(item);
            }
            break;
        }
        }
        if (hasPictures) m_list->m_fillColor = colorKey;
        m_list->Refresh();
        m_list->Show(5);
    }
    RECT area;
    SetRect(&area, 0, 0x35, m_clientWidth, m_clientHeight + 0x14);
    InvalidateRect(m_hwnd, &area, TRUE);
    m_page = page;
    SetFocus(m_hwnd);
    return 1;
}
