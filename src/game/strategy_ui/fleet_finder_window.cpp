#include <string>
#include "strategy_ui/fleet_finder_window.h"

#include <new>

#include "core/checked_new.h"
#include "core/game_string.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/finder_helpers.h"
#include "strategy_ui/galaxy_window.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_label.h"
#include "ui/cool_string_field.h"
#include "ui/cool_tab_control.h"
#include "ui/object_list_item.h"

namespace {

constexpr uint32_t kStringSlot = 2;
constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kHelperBitmapKind = 10;
constexpr int16_t kStringGroup = 2;
constexpr COLORREF kLabelColor = 0x2ffffff;
constexpr uint16_t kClickSound = 0x260;
constexpr int kAlliance = 1;
constexpr uint32_t kGameObjExisting = 0x40;

constexpr uint32_t kFilterTabsId = 100;
constexpr uint32_t kCloseId = 200;
constexpr uint32_t kOpenId = 0xc9;
constexpr uint32_t kListId = 0xcc;
constexpr uint32_t kNameFieldId = 0xcd;
constexpr uint32_t kPageTabsId = 0xfa;
constexpr uint32_t kFleetsPageId = 1;
constexpr uint32_t kCapitalShipsPageId = 2;

constexpr uint32_t kFleetsBackground = 1;
constexpr uint32_t kCapitalShipsBackground = 2;

ResourceStringId StringId(int16_t id) { return ResourceStringId{id, kStringGroup}; }

CoolStrobeButton* NewButton(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                            uint32_t childId, uint32_t normalId, uint32_t pressedId, DibBitmap* backdrop) {
    return NewOrNull<CoolStrobeButton>(instance, x, y, width, height, parent, childId, normalId, pressedId, backdrop,
                                       0, 0);
}

bool IsOnSideRoot(GameObjNotifier* location) {
    ObjectKey key;
    location->GetKey(&key);
    return key.Family() == family::kSideRoot;
}

bool PassesFilter(GameObj* object, int filter) {
    switch (filter) {
    case kFleetsAll: return true;
    case kFleetsAlliance: return object->ControlSide() == 1;
    case kFleetsEmpire: return object->ControlSide() == 2;
    default: return false;
    }
}

void OpenObjectWindow(GalaxyWindow* galaxy, const ObjectKey* key) { galaxy->OpenWindowForObject(key); }

}  // namespace

FleetFinderWindow::FleetFinderWindow(HINSTANCE instance, int x, int y, int width, int height, GalaxyWindow* parent,
                                     uint32_t childId)
    : CustomDialogBox(instance, x, y, width, height, parent, childId, nullptr) {
    m_stringSlot = kStringSlot;
    m_bitmapSlot = kBitmapSlot;
    m_flags |= kGameWindowPopup;
    m_filterTabs = nullptr;
    m_nameField = nullptr;
    m_list = nullptr;
    m_filterLabel = nullptr;
    m_galaxy = parent;
    m_filter = 0;
    m_previousFilter = 0;
    m_pageTabs = nullptr;
    m_mode = 0;
    m_items = NewOrNull<CoolListItemList>(kSortByLabel);
}

FleetFinderWindow::~FleetFinderWindow() {
    if (m_filterTabs) delete m_filterTabs;
    if (m_pageTabs) delete m_pageTabs;
    if (m_nameField) delete m_nameField;
    if (m_list) delete m_list;
    if (m_filterLabel) delete m_filterLabel;
    if (m_items) {
        m_items->Clear();
        delete m_items;
    }

}

void FleetFinderWindow::OnCreate1() {
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    CenterIn(m_galaxy->m_panelArea.left, m_galaxy->m_panelArea.top, m_galaxy->m_panelArea.right,
             m_galaxy->m_panelArea.bottom);

    DibBitmap* fleetsPanel;
    DibBitmap* capitalShipsPanel;
    DibBitmap* frame;
    DibBitmap* rightEdge;
    if (m_galaxy->m_side == kAlliance) {
        fleetsPanel = NewOrNull<DibBitmap>(art, 0x291eu, kHelperBitmapKind);
        capitalShipsPanel = NewOrNull<DibBitmap>(art, 0x291cu, kHelperBitmapKind);
        frame = NewOrNull<DibBitmap>(art, 0x285fu, kHelperBitmapKind);
        rightEdge = NewOrNull<DibBitmap>(art, 0x295au, kHelperBitmapKind);
    } else {
        fleetsPanel = NewOrNull<DibBitmap>(art, 0x291fu, kHelperBitmapKind);
        capitalShipsPanel = NewOrNull<DibBitmap>(art, 0x291du, kHelperBitmapKind);
        frame = NewOrNull<DibBitmap>(art, 0x2860u, kHelperBitmapKind);
        rightEdge = NewOrNull<DibBitmap>(art, 0x295eu, kHelperBitmapKind);
    }

    HDC windowDc = GetDC(m_hwnd);
    HDC dc = CreateCompatibleDC(windowDc);
    ReleaseDC(m_hwnd, windowDc);
    DibBitmap* fleetsBackground =
        NewOrNull<DibBitmap>(static_cast<LONG>(frame->Width()), static_cast<LONG>(frame->Height()), frame, dc);
    fleetsPanel->Blit(fleetsBackground, 0xc, 0xd, 0, 0, 0, 0);
    frame->DrawKeyed(fleetsBackground, 0, 0);
    rightEdge->Blit(fleetsBackground, 0x19c, 0, 0, 0, 0, 0);
    HGDIOBJ savedBitmap = SelectObject(dc, fleetsBackground->DibSectionHandle());

    const ResourceStringId fleetsTitle = StringId(0x1880);
    CoolLabel* title = NewOrNull<CoolLabel>(&fleetsTitle, 0x24, 0xe, 0, 0, 0u, 0u);
    title->SetFont(5);
    title->SetColor(kLabelColor, nullptr);
    title->m_width = 0x15e;
    title->m_layoutFlags = kLabelFixedWidth;
    title->m_drawFlags = 1;
    title->Draw(dc);
    const ResourceStringId searchText = StringId(0x1881);
    CoolLabel* search = NewOrNull<CoolLabel>(&searchText, 0x24, 0x30, 0, 0, 0u, 0u);
    search->SetFont(4);
    search->SetColor(kLabelColor, nullptr);
    search->Draw(dc);
    AddOverlay(fleetsBackground, kFleetsBackground);

    DibBitmap* shipsBackground =
        NewOrNull<DibBitmap>(static_cast<LONG>(frame->Width()), static_cast<LONG>(frame->Height()), frame, dc);
    capitalShipsPanel->Blit(shipsBackground, 0xc, 0xd, 0, 0, 0, 0);
    frame->DrawKeyed(shipsBackground, 0, 0);
    rightEdge->Blit(shipsBackground, 0x19c, 0, 0, 0, 0, 0);
    SelectObject(dc, shipsBackground->DibSectionHandle());
    const ResourceStringId shipsTitle = StringId(0x1885);
    title->SetTextFromResource(&shipsTitle);
    title->m_width = 0x15e;
    title->m_layoutFlags = kLabelFixedWidth;
    title->m_drawFlags = 1;
    title->Draw(dc);
    const ResourceStringId shipsSearchText = StringId(0x1886);
    search->SetTextFromResource(&shipsSearchText);
    search->Draw(dc);
    AddOverlay(shipsBackground, kCapitalShipsBackground);
    SetPaintBackgroundById(kFleetsBackground);
    SelectObject(dc, savedBitmap);
    DeleteDC(dc);
    if (fleetsPanel) delete fleetsPanel;
    if (capitalShipsPanel) delete capitalShipsPanel;
    if (frame) delete frame;
    if (rightEdge) delete rightEdge;
    if (title) delete title;
    if (search) delete search;

    m_filterTabs =
        NewOrNull<CoolTabControl>(m_instance, 0x24, 0x4e, 0x99, 0x29, kFilterTabsId, this, GetPaintBackgroundIfOwned());
    if (m_filterTabs) {
        m_filterTabs->Create(0);
        struct FilterTab {
            int x;
            uint32_t id;
            uint32_t normalId;
            uint32_t pressedId;
            int16_t tooltip;
        };
        const FilterTab tabs[] = {
            {0, kFleetsAll, 0x2904, 0x2905, 0x1882},
            {0x34, kFleetsAlliance, 0x2906, 0x2907, 0x1883},
            {0x68, kFleetsEmpire, 0x2909, 0x290a, 0x1884},
        };
        for (const FilterTab& tab : tabs) {
            CoolStrobeButton* button = NewButton(art, tab.x, 0, 0x31, 0x29, m_filterTabs, tab.id, tab.normalId,
                                                 tab.pressedId, m_filterTabs->m_background);
            if (!button) continue;
            button->Create(0);
            const ResourceStringId tip = StringId(tab.tooltip);
            AddToolFromString(button, &tip);
            m_filterTabs->AddTab(button, tab.id == kFleetsAll ? 1 : 0, 0);
        }
    }

    int closeX;
    int openY;
    uint32_t openId;
    uint32_t openPressedId;
    if (m_galaxy->m_side == kAlliance) {
        AddButton(0x1a7, 0x19, 0x2912, 0x2913, kCloseId, 0x1954, 0, 0, 0, 0, 0, 0);
        closeX = 0x1a7;
        openY = 0x5d;
        openId = 0x2916;
        openPressedId = 0x2917;
    } else {
        AddButton(0x1aa, 0x15, 0x2914, 0x2915, kCloseId, 0x1954, 0, 0, 0, 0, 0, 0);
        closeX = 0x1aa;
        openY = 0x59;
        openId = 0x2918;
        openPressedId = 0x2919;
    }
    AddButton(closeX, openY, openId, openPressedId, kOpenId, 0x1953, 0, 0, 0, 0, 0, 0);
    if (CoolStrobeButton* close = static_cast<CoolStrobeButton*>(m_children.FindById(kCloseId)))
        close->SetClickSound(kClickSound);
    if (CoolStrobeButton* open = static_cast<CoolStrobeButton*>(m_children.FindById(kOpenId)))
        open->SetClickSound(kClickSound);

    m_nameField = NewOrNull<CoolStringField>(m_instance, 0x8f, 0x2d, 0xfa, 0x10, this, GetPaintBackgroundIfOwned(), 0u);
    m_nameField->m_childId = kNameFieldId;
    m_nameField->m_textColor = 0xffffff;
    m_nameField->Create(0);
    m_nameField->Clear();
    SetCaretPos(0, 0);

    m_filterLabel = NewOrNull<CoolLabel>(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    if (m_filterLabel) {
        m_filterLabel->MoveTo(0x28, 0x77);
        m_filterLabel->m_width = 0x11b;
        m_filterLabel->m_height = 0x10;
        m_filterLabel->m_layoutFlags = kLabelFixedWidth | kLabelFixedHeight;
        m_filterLabel->m_drawFlags = 0;
        m_filterLabel->SetFont(5);
        m_filterLabel->SetColor(kLabelColor, nullptr);
        HDC labelDc = GetDC(m_hwnd);
        m_filterLabel->Measure(labelDc);
        ReleaseDC(m_hwnd, labelDc);
    }

    m_list = NewOrNull<CoolDragList>(art, 0x24, 0x8a, 0x15e, 0xa5, this, GetPaintBackgroundIfOwned(), kListId, 0x15e,
                                     0x14, 0x287f, 0x2880, 0x287d, 0x287e, 10, 1, nullptr);
    if (m_list) {
        m_list->m_options.m_value = kDragListSingleSelect | kDragListNoDrag;
        m_list->Create(0);
        m_list->SetSortMode(kSortByLabel);
        m_list->m_textFormat = DT_VCENTER | DT_SINGLELINE;
        CoolDragList* list = m_list;
        list->CreateVerticalScrollBar(m_instance, list, 7, 0x299d, list->m_clientHeight, 0, list->m_clientHeight, 0,
                                      nullptr);
        m_list->m_textOffsetX = 5;
        m_list->SetItemList(m_items);
    }

    if (m_galaxy->m_side == kAlliance)
        m_pageTabs = NewOrNull<CoolTabControl>(m_instance, 0x1a7, 0x93, 0x20, 0x55, kPageTabsId, this,
                                               GetPaintBackgroundIfOwned());
    else
        m_pageTabs = NewOrNull<CoolTabControl>(m_instance, 0x1aa, 0x8f, 0x2c, 0x88, kPageTabsId, this,
                                               GetPaintBackgroundIfOwned());
    if (m_pageTabs) {
        m_pageTabs->Create(0);
        CoolStrobeButton* shipsTab;
        if (m_galaxy->m_side == kAlliance)
            shipsTab = NewButton(art, 0, 0, 0x20, 0x1f, m_pageTabs, kCapitalShipsPageId, 0x2922, 0x2923,
                                 m_pageTabs->m_background);
        else
            shipsTab = NewButton(art, 0, 0, 0x2c, 0x29, m_pageTabs, kCapitalShipsPageId, 0x2926, 0x2927,
                                 m_pageTabs->m_background);
        if (shipsTab) {
            shipsTab->Create(0);
            shipsTab->SetClickSound(kClickSound);
            const ResourceStringId tip = StringId(0x1885);
            AddToolFromString(shipsTab, &tip);
            m_pageTabs->AddTab(shipsTab, 1, 0);
        }
        CoolStrobeButton* fleetsTab;
        if (m_galaxy->m_side == kAlliance)
            fleetsTab = NewButton(art, 0, 0x36, 0x20, 0x1f, m_pageTabs, kFleetsPageId, 0x2920, 0x2921,
                                  m_pageTabs->m_background);
        else
            fleetsTab = NewButton(art, 0, 0x36, 0x2c, 0x29, m_pageTabs, kFleetsPageId, 0x2924, 0x2925,
                                  m_pageTabs->m_background);
        if (fleetsTab) {
            fleetsTab->Create(0);
            fleetsTab->SetClickSound(kClickSound);
            const ResourceStringId tip = StringId(0x1880);
            AddToolFromString(fleetsTab, &tip);
            m_pageTabs->AddTab(fleetsTab, 0, 0);
        }
    }
    m_pageTabs->SelectTab(kFleetsPageId, 0);
    m_mode = kFleetFinderFleets;
    SetFilter(kFleetsAll, 1);
    SetFocus(m_nameField->m_hwnd);
}

void FleetFinderWindow::DrawOverlay(HDC dc) { m_filterLabel->Draw(dc); }

void FleetFinderWindow::OnCommand(int, int command, int) {
    const uint32_t code = static_cast<uint32_t>(command) >> 16;
    switch (command & 0xffff) {
    case kFilterTabsId: {
        SetFilter(static_cast<int>(code), 0);
        const uint32_t id = m_list->SelectByPrefix(&m_nameField->m_text, 0, 0);
        if (id) {
            ObjectKey key;
            static_cast<ObjectListItem*>(m_items->FindById(id))->GetKey(&key);
            m_selected = key;
        }
        break;
    }
    case kOpenId:
        OpenObjectWindow(m_galaxy, &m_selected);
        PostCloseRequest();
        break;
    case kCloseId:
        PostCloseRequest();
        break;
    case kListId: {
        CoolListItemList selection(kSortById);
        m_list->CopySelectionTo(&selection);
        CoolListItem* first = static_cast<CoolListItem*>(selection.m_head);
        if (!first) return;
        ObjectListItem* item = static_cast<ObjectListItem*>(m_items->FindById(first->m_id));
        ObjectKey key;
        item->GetKey(&key);
        m_selected = key;
        if (static_cast<uint16_t>(code) == kDragListActivated) {
            OpenObjectWindow(m_galaxy, &m_selected);
            PostCloseRequest();
        } else if (static_cast<uint16_t>(code) == kDragListSelectionChanged) {
            m_nameField->SetText(first->m_text.c_str());
        }
        break;
    }
    case kPageTabsId:
        SetMode(static_cast<int>(code));
        break;
    }
}

LRESULT FleetFinderWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT result = 0;
    if (message == WM_SETFOCUS) {
        SetFocus(m_nameField->m_hwnd);
    } else if (message == kFieldReturnMessage) {
        const ObjectKey unset;
        if (static_cast<int16_t>(wParam) == kNameFieldId && m_selected.m_value != unset.m_value) {
            OpenObjectWindow(m_galaxy, &m_selected);
            PostCloseRequest();
        }
    } else if (message == kFieldChangedMessage) {
        if (static_cast<int16_t>(wParam) == kNameFieldId) {
            ObjectListItem* item = nullptr;
            const std::string* typed = &m_nameField->m_text;
            if (!typed->empty()) {
                const uint32_t id = m_list->SelectByPrefix(typed, 0, 0);
                item = static_cast<ObjectListItem*>(m_items->FindById(id));
            }
            if (!item) {
                m_selected = ObjectKey();
            } else {
                ObjectKey key;
                item->GetKey(&key);
                m_selected = key;
            }
        }
    } else {
        result = CoolGameWindow::WindowProc(message, wParam, lParam);
    }
    return result;
}

void FleetFinderWindow::SetFilter(int filter, int) {
    const int view = m_galaxy->m_side;
    if (m_filter == filter) return;
    const ObjectKey unset;
    m_items->Clear();
    const bool fleets = m_mode == kFleetFinderFleets;
    const FamilyRange& range = fleets ? family::kFleets : family::kCapitalShips;
    for (GameObjNotifier* object = fleets ? GetFirstObjectInFamilies_08_10(view) : GetFirstObjectInFamilies_14_1c(view);
         object; object = static_cast<GameObj*>(object)->FindNextSibling(&range, 1)) {
        GameObj* ship = static_cast<GameObj*>(object);
        if (!(ship->m_stateFlags & kGameObjExisting)) continue;
        GameObjNotifier* location = fleets ? ship->m_parent : ship->m_parent->m_parent;
        if (IsOnSideRoot(location) || !PassesFilter(ship, filter)) continue;
        ObjectKey key;
        ObjectListItem* item = NewOrNull<ObjectListItem>(ship->GetKey(&key));
        if (!item) continue;
        {
            std::string name;
            ship->GetNameCopy(&name);
            item->m_text = name.c_str();
        }
        m_items->Insert(item);
    }
    m_filter = filter;
    m_selected = unset;
    m_list->Refresh();
    const bool fleetPage = m_mode == kFleetFinderFleets;
    const int16_t tips[] = {fleetPage ? int16_t(0x1882) : int16_t(0x1887), fleetPage ? int16_t(0x1883) : int16_t(0x1888),
                            fleetPage ? int16_t(0x1884) : int16_t(0x1889)};
    for (int tab = 1; tab <= 3; ++tab) {
        const ResourceStringId tip = StringId(tips[tab - 1]);
        UpdateToolFromString(m_filterTabs->m_children.FindById(tab), &tip);
    }
    m_filterLabel->SetText(m_filterTabs->m_children.FindById(filter)->m_tooltipText);
    RECT area = m_filterLabel->m_rect;
    InvalidateRect(m_hwnd, &area, FALSE);
}

void FleetFinderWindow::SetMode(int mode) {
    if (m_mode == mode) return;
    m_nameField->SetText(nullptr);
    SetCaretPos(0, 0);
    m_mode = mode;
    if (mode == kFleetFinderFleets)
        SetPaintBackgroundById(kFleetsBackground);
    else if (mode == kFleetFinderCapitalShips)
        SetPaintBackgroundById(kCapitalShipsBackground);
    m_list->SetBackground(GetPaintBackgroundIfOwned(), m_list->m_x, m_list->m_y);
    const int filter = m_filter;
    m_previousFilter = filter;
    m_filter = 0;
    SetFilter(filter, 0);
}

void FleetFinderWindow::OnKeyDown(WPARAM key, LPARAM lParam) {
    FinderOnKeyDown(this, m_filterTabs, m_nameField, m_list, key, lParam);
}
