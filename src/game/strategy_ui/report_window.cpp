#include "strategy_ui/report_window.h"

#include <cstdint>
#include <new>

#include "ai/ai_common.h"
#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"
#include "ui/cool_tab_control.h"
#include "ui/message_template.h"
#include "ui/object_list_item.h"
#include "ui/resource_dll.h"

extern const char g_gokresDllName[];       // 0x6a8708 "gokres.dll"
extern const int16_t g_stringGroup65d424;  // 0x65d424

DibBitmap* CreateObjectPicture(HMODULE module, const ObjectKey* key, GameObjNotifier* object, int a, int b);
// The sector window selects the unit of the given key (the report's object).
void __thiscall FUN_0045c8e0(CoolWindow* sectorWindow, const ObjectKey* key, int mode);
// The modal state of the report (the id of the window and of the message it shows).
void ModalReportClose(int windowId, int messageId);
void ModalReportReset(int windowId);

namespace {

constexpr uint32_t kArtSlot = 7;
constexpr uint32_t kGokresSlot = 10;
constexpr uint32_t kBitmapKind = 10;
constexpr uint32_t kStyleVisible = 0x10000000;
constexpr uint16_t kClickSound = 0x260;
constexpr int kAlliance = 1;
constexpr int kEmpire = 2;

enum ReportChild : uint32_t {
    kCloseId = 10,
    kNextId = 11,
    kPreviousId = 12,
    kPageTabsId = 100,
    kReportPage = 0x65,
    kTeamPage = 0x66,
    kOtherPage = 0x67,
    kOptionsPage = 0x68,
    kUnitTabsId = 0x96,
    kUnitTabFirst = 0x97,
    kUnitTabLast = 0x9c,
};

ResourceStringId StringId(uint16_t id) { return ResourceStringId{static_cast<int16_t>(id), g_stringGroup65d424}; }

COLORREF ReportColor(int side) {
    if (side == 1) return 0x20000ff;
    if (side == 2) return 0x200ff00;
    return 0x2ffff00;
}

CoolStrobeButton* NewButton(HMODULE art, int x, int y, int width, int height, CoolWindow* parent, uint32_t childId,
                            uint32_t normalId, uint32_t pressedId, DibBitmap* backdrop, int pixelHit = 0) {
    return NewOrNull<CoolStrobeButton>(art, x, y, width, height, parent, childId, normalId, pressedId, backdrop, 0,
                                       pixelHit);
}

void DeleteBitmap(DibBitmap* bitmap) {
    if (!bitmap) return;
    bitmap->~DibBitmap();
    ::operator delete(bitmap);
}

DibBitmap* NewSurface(DibBitmap* like, HDC dc) {
    return NewOrNull<DibBitmap>(static_cast<LONG>(like->Width()), static_cast<LONG>(like->Height()), like, dc);
}

uint32_t SelectedChildId(CoolTabControl* tabs) { return tabs->m_selected ? tabs->m_selected->m_childId : 0; }

struct UnitTab {
    uint32_t m_overlayId;
    int m_itemWidth;
    int m_itemHeight;
    uint32_t m_firstFamily;
    uint32_t m_endFamily;
    int m_threeColumns;
};

bool FindUnitTab(int subTabId, UnitTab* out) {
    static constexpr UnitTab kTabs[] = {
        {3, 0xa5, 0x46, 0x14, 0x1c, 0}, {3, 0xa5, 0x46, 0x1c, 0x20, 0}, {3, 0xa5, 0x46, 0x28, 0x30, 0},
        {3, 0xa5, 0x46, 0x22, 0x28, 0}, {3, 0xa5, 0x46, 0x10, 0x14, 0}, {4, 0x6e, 0x32, 0x30, 0x40, 1},
    };
    if (subTabId < static_cast<int>(kUnitTabFirst) || subTabId > static_cast<int>(kUnitTabLast)) return false;
    *out = kTabs[subTabId - kUnitTabFirst];
    return true;
}

void CopyFamilyRange(ObjectKeySet* from, uint32_t first, uint32_t end, ObjectKeySet* to) {
    for (ObjectKeyNode* node = from->First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
        ObjectKey key;
        node->GetKey(&key);
        if (first <= key.Family() && key.Family() < end) {
            ObjectKeyNode* copy = NewOrNull<ObjectKeyNode>(*node);
            to->Add(copy);
        }
    }
}

}  // namespace

// ---- Construction ------------------------------------------------------------------------------------------

ReportWindow::ReportWindow(HINSTANCE instance, GalaxyWindow* parent, uint32_t childId, ReportMessage* report)
    : CoolGameWindow(instance, 0, 0, 0x1d6, 0x14b, parent, 0, kArtSlot, 0, childId, 0, 1, 0, 0),
      m_label(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u) {
    m_style = 0x46000000;
    m_galaxy = parent;
    m_report = report;
    m_list = nullptr;
    m_items = nullptr;
    m_pageTabs = nullptr;
    m_unitTabs = nullptr;
    m_currentPage = 0;
    m_previousButton = nullptr;
    m_nextButton = nullptr;
    m_listKind = 0;
    m_nextItemId = 1;
    m_gokres = FindResourceModule(kGokresSlot);
    if (!m_gokres) {
        char* dllLocation = GetDllLocation();
        m_gokres = LoadResourceModule(dllLocation, g_gokresDllName, kGokresSlot);
        if (dllLocation) operator delete(dllLocation);
    }
}

ReportWindow::~ReportWindow() {
    REB_KEEP_VPTR_STORE();
    delete m_list;
    if (m_items) {
        m_items->Clear();
        delete m_items;
    }
    delete m_pageTabs;
    delete m_unitTabs;
    delete m_previousButton;
    delete m_nextButton;
    delete m_closeButton;
}

// ---- Window construction -----------------------------------------------------------------------------------

void ReportWindow::OnCreate1() {
    CoolLabel text(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    CoolLabel title(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    HINSTANCE host = m_instance;
    HMODULE art = CoolAppGetResourceModule(kArtSlot);
    const int side = m_galaxy->m_side;
    const bool alliance = side == kAlliance;

    DibBitmap* portrait = NewOrNull<DibBitmap>(art, static_cast<uint32_t>(m_report->m_bitmapId), kBitmapKind);
    DibBitmap* panel = NewOrNull<DibBitmap>(art, alliance ? 0x285fu : 0x2860u, kBitmapKind);
    DibBitmap* frame = NewOrNull<DibBitmap>(art, alliance ? 0x2a44u : 0x2a45u, kBitmapKind);
    DibBitmap* cornerA = NewOrNull<DibBitmap>(art, 0x2a09u, kBitmapKind);
    DibBitmap* cornerB = NewOrNull<DibBitmap>(art, 0x2a0au, kBitmapKind);
    DibBitmap* cornerC = NewOrNull<DibBitmap>(art, 0x2a0bu, kBitmapKind);

    HDC window = GetDC(m_hwnd);
    HDC dc = CreateCompatibleDC(window);
    ReleaseDC(m_hwnd, window);
    const int frameX = alliance ? 0x19c : 0x19f;
    const COLORREF color = ReportColor(m_report->m_side);

    auto composePage = [&](DibBitmap* corner) {
        DibBitmap* surface = NewSurface(panel, dc);
        corner->Blit(surface, 0xc, 0xd, 0, 0, 0, 0);
        panel->DrawKeyed(surface, 0, 0);
        frame->DrawKeyed(surface, frameX, 0);
        return surface;
    };

    DibBitmap* surface = NewSurface(panel, dc);
    portrait->Blit(surface, 0xc, 0xd, 0, 0, 0, 0);
    panel->DrawKeyed(surface, 0, 0);
    frame->DrawKeyed(surface, frameX, 0);
    HGDIOBJ previous = SelectObject(dc, surface->DibSectionHandle());
    title.SetColor(color, nullptr);
    title.MoveTo(0xc, 0xd);
    title.SetFont(0xb);
    title.m_drawFlags = 1;
    title.m_width = 400;
    title.m_layoutFlags = 1;
    title.SetText(m_report->m_title.m_text);
    title.Measure(dc);
    title.Draw(dc);
    text.SetFont(6);
    text.m_width = 0x15e;
    text.m_layoutFlags = 1;
    text.m_drawFlags = 0x110;
    text.SetText(m_report->m_caption.m_text);
    text.SetColor(0, nullptr);
    text.MoveTo(0x1a, 0xe2);
    text.Measure(dc);
    text.Draw(dc);
    text.SetColor(color, nullptr);
    text.MoveTo(0x19, 0xe1);
    text.Measure(dc);
    text.Draw(dc);
    SetPaintBackground(surface, 1);
    SelectObject(dc, previous);

    surface = composePage(cornerC);
    previous = SelectObject(dc, surface->DibSectionHandle());
    title.Draw(dc);
    String expanded;
    ExpandMessageTemplate(&expanded, 0x7013, nullptr, nullptr, nullptr, nullptr);
    text.SetColor(color, nullptr);
    text.SetText(expanded.m_text);
    text.MoveTo(0x19, 0xe1);
    text.Measure(dc);
    text.MoveTo(0x1d, (text.m_rect.top - text.m_rect.bottom) + 0xb1);
    text.Measure(dc);
    text.Draw(dc);
    SelectObject(dc, previous);
    AddOverlay(surface, 5);

    surface = composePage(cornerA);
    previous = SelectObject(dc, surface->DibSectionHandle());
    text.SetColor(0xffffff, nullptr);
    text.SetFont(4);
    text.m_drawFlags = 1;
    text.m_width = 0xa5;
    text.MoveTo(0x26, 0x75);
    ResourceStringId label = StringId(0x1932);
    text.SetTextFromResource(&label);
    text.Measure(dc);
    text.Draw(dc);
    text.m_width = 0xa5;
    text.MoveTo(0xcc, 0x75);
    label = StringId(0x1933);
    text.SetTextFromResource(&label);
    text.Measure(dc);
    text.Draw(dc);
    title.Draw(dc);
    SelectObject(dc, previous);
    AddOverlay(surface, 3);

    surface = composePage(cornerB);
    previous = SelectObject(dc, surface->DibSectionHandle());
    text.SetColor(0x2ffffff, nullptr);
    text.m_width = 0x6e;
    text.MoveTo(0x26, 0x75);
    label = StringId(0x1934);
    text.SetTextFromResource(&label);
    text.Measure(dc);
    text.Draw(dc);
    text.m_width = 0x6e;
    text.MoveTo(0xa5, 0x75);
    label = StringId(0x1935);
    text.SetTextFromResource(&label);
    text.Measure(dc);
    text.Draw(dc);
    text.m_width = 0x6e;
    text.MoveTo(0x113, 0x75);
    label = StringId(0x1936);
    text.SetTextFromResource(&label);
    text.Measure(dc);
    text.Draw(dc);
    title.Draw(dc);
    SelectObject(dc, previous);
    AddOverlay(surface, 4);
    DeleteDC(dc);
    DeleteBitmap(panel);
    DeleteBitmap(portrait);
    DeleteBitmap(cornerA);
    DeleteBitmap(cornerB);
    DeleteBitmap(cornerC);
    DeleteBitmap(frame);

    m_closeButton = alliance ? NewButton(art, 0x1a7, 0x19, 0x20, 0x1f, this, kCloseId, 0x2882, 0x2883,
                                         GetPaintBackgroundIfOwned())
                             : NewButton(art, 0x1aa, 0x15, 0x2c, 0x29, this, kCloseId, 0x2888, 0x2889,
                                         GetPaintBackgroundIfOwned());
    if (m_closeButton) {
        m_closeButton->Create(0);
        m_closeButton->SetClickSound(kClickSound);
    }

    const int tabWidth = alliance ? 0x29 : 0x2c;
    m_pageTabs = alliance ? NewOrNull<CoolTabControl>(host, 0x1a2, 0x58, 0x29, 0xcb, kPageTabsId, this,
                                                      GetPaintBackgroundIfOwned())
                          : NewOrNull<CoolTabControl>(host, 0x1aa, 0x59, 0x2c, 0xda, kPageTabsId, this,
                                                      GetPaintBackgroundIfOwned());
    if (m_pageTabs) {
        m_pageTabs->Create(0);
        DibBitmap* tabBackdrop = m_pageTabs->m_background;
        auto addPageTab = [&](int y, uint32_t childId, uint32_t normalId, uint32_t pressedId, int tooltip, int select,
                              int notify) {
            CoolStrobeButton* tab =
                NewButton(art, 0, y, tabWidth, 0x29, m_pageTabs, childId, normalId, pressedId, tabBackdrop);
            if (!tab) return;
            tab->Create(0);
            tab->SetClickSound(kClickSound);
            if (tooltip) {
                const ResourceStringId tip = StringId(static_cast<uint16_t>(tooltip));
                AddToolFromString(tab, &tip);
            }
            m_pageTabs->AddTab(tab, select, notify);
        };
        addPageTab(0, kReportPage, alliance ? 0x29e8 : 0x29f2, alliance ? 0x29e9 : 0x29f3, 0x1944, 1, 0);

        uint32_t teamNormal;
        uint32_t teamPressed;
        if (m_report->m_side == 1) {
            teamNormal = alliance ? 0x29ea : 0x29f4;
            teamPressed = alliance ? 0x29eb : 0x29f5;
        } else {
            teamNormal = alliance ? 0x29ec : 0x29f6;
            teamPressed = alliance ? 0x29ed : 0x29f7;
        }
        CoolStrobeButton* teamTab = NewButton(art, 0, alliance ? 0x35 : 0x3b, tabWidth, 0x29, m_pageTabs, kTeamPage,
                                              teamNormal, teamPressed, tabBackdrop);
        if (teamTab) {
            teamTab->Create(0);
            teamTab->SetClickSound(kClickSound);
            if (m_report->m_side == 1) {
                const ResourceStringId tip = StringId(0x1937);
                AddToolFromString(teamTab, &tip);
            } else if (m_report->m_side == 2) {
                const ResourceStringId tip = StringId(0x1938);
                AddToolFromString(teamTab, &tip);
            }
            m_pageTabs->AddTab(teamTab, 0, 1);
        }

        uint32_t otherNormal;
        uint32_t otherPressed;
        const int variant = m_report->m_secondSide;
        if (alliance) {
            otherNormal = variant == 1 ? 0x29ea : (variant == 2 ? 0x29ec : 0x29f0);
            otherPressed = variant == 1 ? 0x29eb : (variant == 2 ? 0x29ed : 0x29f1);
        } else {
            otherNormal = variant == 1 ? 0x29f4 : (variant == 2 ? 0x29f6 : 0x29fa);
            otherPressed = variant == 1 ? 0x29f5 : (variant == 2 ? 0x29f7 : 0x29fb);
        }
        addPageTab(alliance ? 0x6c : 0x76, kOtherPage, otherNormal, otherPressed, 0x1943, 0, 1);
        addPageTab(alliance ? 0xa2 : 0xb1, kOptionsPage, alliance ? 0x2a27 : 0x2918, alliance ? 0x2a28 : 0x2919, 0x1939,
                   1, 0);
    }

    m_unitTabs = NewOrNull<CoolTabControl>(host, 0x24, 0x3b, 0x169, 0x29, kUnitTabsId, this, FindOverlayBitmap(3));
    if (m_unitTabs) {
        m_unitTabs->m_style &= ~kStyleVisible;
        m_unitTabs->Create(0);
        DibBitmap* unitBackdrop = m_unitTabs->m_background;
        auto addUnitTab = [&](int x, uint32_t childId, uint32_t normalId, uint32_t pressedId, uint16_t tooltip,
                              int select, int notify) {
            CoolStrobeButton* tab = NewButton(art, x, 0, 0x31, 0x29, m_unitTabs, childId, normalId, pressedId,
                                              unitBackdrop);
            if (!tab) return;
            tab->Create(0);
            const ResourceStringId tip = StringId(tooltip);
            AddToolFromString(tab, &tip);
            m_unitTabs->AddTab(tab, select, notify);
        };
        addUnitTab(0, 0x97, alliance ? 0x286c : 0x2878, alliance ? 0x286b : 0x2877, 0x1928, m_report->m_showsSecondList == 0, 0);
        addUnitTab(0x3e, 0x98, alliance ? 0x2872 : 0x287c, alliance ? 0x2871 : 0x287b, 0x1929, 0, 1);
        addUnitTab(0x7c, 0x99, alliance ? 0x2868 : 0x2874, alliance ? 0x2867 : 0x2873, 0x1835, 0, 1);
        addUnitTab(0xba, 0x9a, 0x2866, 0x2865, 0x1946, 0, 1);
        addUnitTab(0xf8, 0x9b, alliance ? 0x2870 : 0x287a, alliance ? 0x286f : 0x2879, 0x1930, m_report->m_showsSecondList, 0);
        addUnitTab(0x138, 0x9c, alliance ? 0x286a : 0x2876, alliance ? 0x2869 : 0x2875, 0x1931, 0, 1);
    }

    m_items = NewOrNull<CoolListItemList>(4);
    m_list = NewOrNull<CoolDragList>(host, 0x25, 0x88, 0x15b, 0xa6, this, GetPaintBackgroundIfOwned(), 6, 0xa5, 0x46, 0,
                                     0, 0, 0, 10, 1, nullptr);
    if (m_list) {
        m_list->m_options.m_value = kDragListFlowLayout | kDragListNoDrag | kDragListSingleSelect;
        m_list->m_style &= ~kStyleVisible;
        m_list->Create(0);
        CoolDragList* list = m_list;
        list->CreateVerticalScrollBar(m_instance, list, 7, 0x29fc, list->m_clientHeight, 0, list->m_clientHeight, 0,
                                      nullptr);
        m_list->m_textFormat = DT_VCENTER;
        m_list->SetItemList(m_items);
        m_list->SetTextColor(0xffffff);
    }

    const uint32_t previousNormal = alliance ? 0x2a0d : 0x2a0e;
    const uint32_t previousPressed = alliance ? 0x2a0f : 0x2a10;
    m_previousButton = NewButton(art, 0xe2, 0xb6, 0xa9, 0x60, this, kPreviousId, previousNormal, previousPressed,
                                 FindOverlayBitmap(5), 1);
    if (m_previousButton) {
        DibBitmap* normal = m_previousButton->GetStateBitmap(0);
        DibBitmap* pressed = m_previousButton->GetStateBitmap(1);
        DibBitmap* combined = NewOrNull<DibBitmap>(normal->Width(), normal->Height(), normal);
        normal->Blit(combined, 0, 0, 0, 0, 0, 0);
        pressed->DrawKeyed(combined, 0, 0);
        m_previousButton->ReplaceBitmap(1, combined);
        m_previousButton->m_style &= ~kStyleVisible;
        const ResourceStringId previousLabel = StringId(0x1942);
        m_previousButton->SetLabelFromResource(&previousLabel);
        m_previousButton->SetStateColor(0, 0xffffff);
        m_previousButton->MoveLabel(0, m_previousButton->m_height / 2 - 0xc);
        m_previousButton->Create(0);
        m_previousButton->Hide();
    }

    m_nextButton = NewButton(art, 0x1d, 0xb6, 0xa9, 0x60, this, kNextId, 0x2a0c, (side != kAlliance) + 0x2a0f,
                             FindOverlayBitmap(5), 1);
    if (m_nextButton) {
        GameObjNotifier* reported = GetObjectInFamilies_01_ff(m_galaxy->m_side, &m_report->m_objectKey);
        DibBitmap* normal = m_nextButton->GetStateBitmap(0);
        DibBitmap* pressed = m_nextButton->GetStateBitmap(1);
        DibBitmap* combined = NewOrNull<DibBitmap>(normal->Width(), normal->Height(), normal);
        normal->Blit(combined, 0, 0, 0, 0, 0, 0);
        pressed->DrawKeyed(combined, 0, 0);
        m_nextButton->ReplaceBitmap(1, combined);
        // BUG(original): the style of the previous button is copied, not the next button's own.
        m_nextButton->m_style = m_previousButton->m_style & ~kStyleVisible;
        if (!reported) {
            const ResourceStringId nextLabel = StringId(0x1945);
            m_nextButton->SetLabelFromResource(&nextLabel);
        } else {
            m_nextButton->SetLabel(static_cast<GameObj*>(reported)->GetName()->m_text);
        }
        m_nextButton->SetStateColor(0, 0xffffff);
        m_nextButton->MoveLabel(0, m_nextButton->m_height / 2 - 0xc);
        m_nextButton->Create(0);
        m_nextButton->Hide();
    }

    CenterIn(m_galaxy->m_panelArea.left, m_galaxy->m_panelArea.top, m_galaxy->m_panelArea.right,
             m_galaxy->m_panelArea.bottom);
    m_pageTabs->SelectTab(kReportPage, 1);
    m_listKind = (m_report->m_showsSecondList != 0 ? 4 : 0) + 0x97;
}

// ---- Painting and input ------------------------------------------------------------------------------------

void ReportWindow::DrawOverlay(HDC dc) {
    if (!(m_currentPage > static_cast<int>(kReportPage) && m_currentPage < static_cast<int>(kOptionsPage))) return;
    CoolWindow* pageTab = m_pageTabs->m_children.FindById(SelectedChildId(m_pageTabs));
    m_label.SetColor(ReportColor(m_report->m_side), nullptr);
    m_label.MoveTo(0xc, 0x24);
    m_label.m_layoutFlags = 1;
    m_label.m_width = 0x15b;
    m_label.m_drawFlags = 1;
    m_label.SetFont(6);
    m_label.SetText(pageTab->m_tooltipText);
    m_label.Measure(dc);
    m_label.Draw(dc);
    CoolWindow* unitTab = m_unitTabs->m_children.FindById(SelectedChildId(m_unitTabs));
    m_label.SetColor(0x2ffffff, nullptr);
    m_label.MoveTo(0x26, 100);
    m_label.m_width = 400;
    m_label.m_drawFlags = 0;
    m_label.SetFont(4);
    m_label.SetText(unitTab->m_tooltipText);
    m_label.Measure(dc);
    m_label.Draw(dc);
}

void ReportWindow::OnCommand(int a, int b, int c) {
    switch (a & 0xffff) {
    case kCloseId:
        PostCloseRequest();
        ModalReportClose(m_commandId, m_report->m_id);
        break;
    case kNextId:
    case kPreviousId: {
        GameObjNotifier* reported = GetObjectInFamilies_01_ff(m_galaxy->m_side, &m_report->m_objectKey);
        if (reported) {
            ObjectKey sectorKey;
            GetObjectKey(reported->m_parent, &sectorKey);
            CoolWindow* sectorWindow = m_galaxy->OpenSectorWindow(&sectorKey);
            if (static_cast<int16_t>(a) == kPreviousId) {
                ObjectKey reportedKey;
                GetObjectKey(reported, &reportedKey);
                FUN_0045c8e0(sectorWindow, &reportedKey, 4);
            }
        }
        ModalReportReset(m_commandId);
        PostCloseRequest();
        break;
    }
    case kPageTabsId:
        SelectPage(static_cast<uint32_t>(b) >> 16);
        break;
    case kUnitTabsId:
        FillList(static_cast<uint32_t>(b) >> 16, 0);
        break;
    }
}

void ReportWindow::OnKeyDown(WPARAM key, LPARAM lParam) {
    if (key == VK_ESCAPE) {
        PostCloseRequest();
        ModalReportClose(m_commandId, m_report->m_id);
        return;
    }
    if (key != VK_LEFT && key != VK_RIGHT) {
        m_parent->WindowProc(WM_KEYDOWN, key, lParam);
        return;
    }
    const bool backwards = key == VK_LEFT;
    AvlNode* tab = m_unitTabs->m_children.FindById(SelectedChildId(m_unitTabs));
    tab = backwards ? tab->Predecessor() : tab->m_successor;
    if (tab) {
        for (; tab; tab = backwards ? tab->Predecessor() : tab->m_successor) {
            CoolWindow* window = static_cast<CoolWindow*>(tab);
            if (IsWindowVisible(window->m_hwnd)) {
                m_unitTabs->SelectTab(window->m_childId, 1);
                return;
            }
        }
        return;
    }
    if (AvlNode* first = m_unitTabs->m_children.First()) {
        m_unitTabs->SelectTab(static_cast<CoolWindow*>(first)->m_childId, 1);
    }
}

// ---- Pages and the participant list ------------------------------------------------------------------------

void ReportWindow::SelectPage(int tabId) {
    if (m_currentPage == tabId) return;
    switch (tabId) {
    case kReportPage:
        m_list->Hide();
        SetFocus(m_hwnd);
        m_unitTabs->Hide();
        m_previousButton->Hide();
        m_nextButton->Hide();
        SetPaintBackgroundById(1);
        m_currentPage = tabId;
        return;
    case kTeamPage:
    case kOtherPage:
        FillList(m_listKind, 1);
        m_list->Show(5);
        SetFocus(m_list->m_hwnd);
        m_unitTabs->Show(5);
        m_previousButton->Hide();
        m_nextButton->Hide();
        m_currentPage = tabId;
        return;
    case kOptionsPage:
        m_list->Hide();
        SetFocus(m_hwnd);
        m_unitTabs->Hide();
        m_previousButton->Show(5);
        m_nextButton->Show(5);
        SetPaintBackgroundById(5);
        break;
    }
    m_currentPage = tabId;
}

void ReportWindow::FillList(int subTabId, int force) {
    if (m_listKind == subTabId && force == 0) return;
    ObjectKeySet firstSet;
    ObjectKeySet secondSet;
    ObjectKeySet thirdSet;
    m_items->Clear();
    UnitTab unitTab;
    // BUG(original): any other tab id leaves the sizes uninitialised; the list is only filled for 0x97-0x9c.
    if (!FindUnitTab(subTabId, &unitTab)) return;

    if (FindOverlayBitmap(unitTab.m_overlayId) == GetPaintBackgroundIfOwned()) {
        RECT captions = {0xc, 0x24, 0x175, 0x3b};
        InvalidateRect(m_hwnd, &captions, FALSE);
    } else {
        DibBitmap* background = SetPaintBackgroundById(unitTab.m_overlayId);
        m_list->SetBackground(background, m_list->m_x, m_list->m_y);
        m_list->SetItemSize(static_cast<int16_t>(unitTab.m_itemWidth), static_cast<int16_t>(unitTab.m_itemHeight));
    }

    const uint32_t selectedPage = SelectedChildId(m_pageTabs);
    if (selectedPage != kTeamPage && selectedPage != kOtherPage) return;
    const bool team = selectedPage == kTeamPage;
    ObjectKeySet* source1 = team ? &m_report->m_sets[1] : &m_report->m_sets[0];
    ObjectKeySet* source2 = team ? &m_report->m_sets[3] : &m_report->m_sets[2];
    ObjectKeySet* source3 = team ? &m_report->m_sets[4] : &m_report->m_sets[5];
    const int threeColumns = unitTab.m_threeColumns;
    const int flag = unitTab.m_threeColumns;
    CopyFamilyRange(source1, unitTab.m_firstFamily, unitTab.m_endFamily, &firstSet);
    CopyFamilyRange(source2, unitTab.m_firstFamily, unitTab.m_endFamily, &secondSet);
    if (threeColumns) CopyFamilyRange(source3, unitTab.m_firstFamily, unitTab.m_endFamily, &thirdSet);

    ObjectKeyNode* first = firstSet.First();
    ObjectKeyNode* second = secondSet.First();
    ObjectKeyNode* third = threeColumns ? thirdSet.First() : nullptr;
    String heading;
    int firstDone = 0;
    int secondDone = 0;
    int thirdDone = threeColumns == 0;
    auto addColumnItem = [&](ObjectKeyNode*& node, int& done, uint16_t emptyId) {
        if (!node) {
            String* emptyHeading = nullptr;
            if (!done) {
                done = 1;
                const ResourceStringId id = StringId(emptyId);
                heading.SetFromResource(&id);
                emptyHeading = &heading;
            }
            AddListItem(nullptr, 0, emptyHeading);
            return;
        }
        ObjectKey key;
        node->GetKey(&key);
        GameObjNotifier* object = GetObjectInFamilies_01_ff(m_galaxy->m_side, &key);
        AddListItem(object, flag, nullptr);
        node = static_cast<ObjectKeyNode*>(node->m_successor);
        if (!node) done = 1;
    };
    while (!firstDone || !secondDone || !thirdDone) {
        addColumnItem(first, firstDone, 0x8526);
        if (threeColumns) addColumnItem(third, thirdDone, 0x8526);
        addColumnItem(second, secondDone, 0x1941);
    }
    m_list->Refresh();
    RECT captions = m_label.m_rect;
    InvalidateRect(m_hwnd, &captions, FALSE);
    m_listKind = subTabId;
}

void ReportWindow::AddListItem(GameObjNotifier* object, int flag, String* heading) {
    const int height = m_list->m_itemHeight;
    const int width = m_list->m_itemWidth;
    if (!object) {
        ObjectKey unset;
        ObjectListItem* item = NewOrNull<ObjectListItem>(static_cast<uint32_t>(m_nextItemId++), &unset);
        if (heading) {
            item->m_text = heading->m_text;
            item->m_fontSlot = 6;
            item->m_textOffsetY = 10;
            item->m_textFormat = 5;
        }
        m_items->Insert(item);
        return;
    }
    ObjectKey objectKey;
    GetObjectKey(object, &objectKey);
    DibBitmap* picture = CreateObjectPicture(m_gokres, &objectKey, object, flag == 0, 1);
    DibBitmap* surface = NewOrNull<DibBitmap>(width, height, picture);
    surface->FillRect(0, 0, width, height, picture->m_colorKey);
    const int pictureWidth = picture->Width();
    const int pictureHeight = picture->Height();
    const int top = (height - pictureHeight) / 2;
    picture->DrawKeyed(surface, (width - pictureWidth) / 2, top);
    void* memory = ::operator new(sizeof(ObjectListItem));
    ObjectListItem* item = nullptr;
    if (memory) {
        ObjectKey itemKey;
        GetObjectKey(object, &itemKey);
        const uint32_t itemId = static_cast<uint32_t>(m_nextItemId++);
        item = new (memory) ObjectListItem(itemId, &itemKey);
    }
    item->SetImages(surface, nullptr);
    item->m_state.m_value |= kListItemDisabled;
    item->m_text = static_cast<GameObj*>(object)->GetName()->m_text;
    item->m_fontSlot = 10;
    item->m_textOffsetY = picture->Height() + top;
    item->m_textFormat = 1;
    m_items->Insert(item);
    DeleteBitmap(picture);
}
