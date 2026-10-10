#include "core/reb_abi.h"
#include "strategy_ui/troop_finder_window.h"

#include <new>

#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/vptr_store.h"
#include "fleet/capship.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/finder_helpers.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/visible_item_list.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_label.h"
#include "ui/cool_string_field.h"
#include "ui/cool_tab_control.h"
#include "ui/object_list_item.h"

extern const char g_gokresDllName[];  // 0x6a8708 "gokres.dll"

namespace {

constexpr uint32_t kStringSlot = 2;
constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kGokresSlot = 10;
constexpr uint32_t kHelperBitmapKind = 10;
constexpr int16_t kStringGroup = 2;
constexpr COLORREF kLabelColor = 0x2ffffff;
constexpr uint16_t kClickSound = 0x260;
constexpr int kAlliance = 1;
constexpr uint32_t kStateFilterExisting = 3;
constexpr int kCellCount = 5;

constexpr uint32_t kTabsId = 100;
constexpr uint32_t kCloseId = 200;
constexpr uint32_t kOpenId = 0xc9;
constexpr uint32_t kListId = 0xca;
constexpr uint32_t kNameFieldId = 0xcb;
constexpr uint32_t kAllianceTabId = 1;
constexpr uint32_t kEmpireTabId = 2;

constexpr uint32_t kAllianceBackground = 1;
constexpr uint32_t kEmpireBackground = 2;

ResourceStringId StringId(int16_t id) { return ResourceStringId{id, kStringGroup}; }

CoolStrobeButton* NewButton(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                            uint32_t childId, uint32_t normalId, uint32_t pressedId, DibBitmap* backdrop) {
    return NewOrNull<CoolStrobeButton>(instance, x, y, width, height, parent, childId, normalId, pressedId, backdrop,
                                       0, 0);
}

bool FamilyIn(const ObjectKey& key, uint32_t first, uint32_t last) {
    return key.Family() >= first && key.Family() < last;
}

// Makes `selected` (a system or a fleet that holds troop regiments) name its first regiment, so that the window that
// opens for it shows the regiment.
void SelectFirstRegiment(ObjectKey* selected, GameObjNotifier* holder) {
    if (FamilyIn(*selected, 0x90, 0x98)) {
        TroopRegIterator regiments(holder, kStateFilterExisting);
        regiments.Begin();
        ObjectKey key;
        regiments.Current()->GetKey(&key);
        *selected = key;
    } else if (FamilyIn(*selected, 8, 0x10)) {
        CapShipIterator ships(holder, kStateFilterExisting);
        bool found = false;
        for (ships.Begin(); ships.Current() && !found; ships.Advance()) {
            TroopRegIterator regiments(ships.Current(), kStateFilterExisting);
            regiments.Begin();
            found = regiments.Current() != nullptr;
            if (found) {
                ObjectKey key;
                regiments.Current()->GetKey(&key);
                *selected = key;
            }
        }
    }
}

}  // namespace

TroopFinderWindow::TroopFinderWindow(HINSTANCE instance, int x, int y, int width, int height, GalaxyWindow* parent,
                                     uint32_t childId)
    : CustomDialogBox(instance, x, y, width, height, parent, childId, nullptr) {
    m_stringSlot = kStringSlot;
    m_bitmapSlot = kBitmapSlot;
    m_flags |= kGameWindowPopup;
    m_tabs = nullptr;
    m_nameField = nullptr;
    m_list = nullptr;
    m_filterLabel = nullptr;
    m_galaxy = parent;
    m_filter = 0;
    m_items = NewOrNull<VisibleItemList>(kSortByLabel);
    m_resourceModule = FindResourceModule(kGokresSlot);
    if (!m_resourceModule) {
        char* dllLocation = GetDllLocation();
        m_resourceModule = LoadResourceModule(dllLocation, g_gokresDllName, kGokresSlot);
        if (dllLocation) operator delete(dllLocation);
    }
    m_dc = nullptr;
}

TroopFinderWindow::~TroopFinderWindow() {
    if (m_tabs) delete m_tabs;
    if (m_nameField) delete m_nameField;
    if (m_list) delete m_list;
    if (m_filterLabel) delete m_filterLabel;
    if (m_items) {
        ClearAllRows(m_items);
        delete m_items;
    }
    if (m_dc) {
        DeleteDC(m_dc);
        m_dc = nullptr;
    }
    for (HBRUSH& brush : m_brushes) {
        if (brush) DeleteObject(brush);
        brush = nullptr;
    }
    REB_KEEP_VPTR_STORE();
}

void TroopFinderWindow::DrawOverlay(HDC dc) { m_filterLabel->Draw(dc); }

void TroopFinderWindow::OnCreate1() {
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    CenterIn(m_galaxy->m_panelArea.left, m_galaxy->m_panelArea.top, m_galaxy->m_panelArea.right,
             m_galaxy->m_panelArea.bottom);

    HDC windowDc = GetDC(m_hwnd);
    m_dc = CreateCompatibleDC(windowDc);
    ReleaseDC(m_hwnd, windowDc);
    m_brushes[0] = CreateSolidBrush(0x20000ff);
    m_brushes[1] = CreateSolidBrush(0x200ff00);
    m_brushes[2] = CreateSolidBrush(0x808080);
    m_brushes[3] = CreateSolidBrush(0x80);
    m_brushes[4] = CreateSolidBrush(0x8000);

    DibBitmap* allianceTitlePanel = NewOrNull<DibBitmap>(art, 0x292cu, kHelperBitmapKind);
    DibBitmap* empireTitlePanel = NewOrNull<DibBitmap>(art, 0x292du, kHelperBitmapKind);
    DibBitmap* frame = nullptr;
    DibBitmap* rightEdge = nullptr;
    if (m_galaxy->m_side == kAlliance) {
        frame = NewOrNull<DibBitmap>(art, 0x285fu, kHelperBitmapKind);
        rightEdge = NewOrNull<DibBitmap>(art, 0x2958u, kHelperBitmapKind);
    } else if (m_galaxy->m_side == 2) {
        frame = NewOrNull<DibBitmap>(art, 0x2860u, kHelperBitmapKind);
        rightEdge = NewOrNull<DibBitmap>(art, 0x295cu, kHelperBitmapKind);
    }
    DibBitmap* allianceBackground =
        NewOrNull<DibBitmap>(static_cast<LONG>(frame->Width()), static_cast<LONG>(frame->Height()), frame, m_dc);
    DibBitmap* empireBackground =
        NewOrNull<DibBitmap>(static_cast<LONG>(frame->Width()), static_cast<LONG>(frame->Height()), frame, m_dc);
    allianceTitlePanel->Blit(allianceBackground, 0xc, 0xd, 0, 0, 0, 0);
    empireTitlePanel->Blit(empireBackground, 0xc, 0xd, 0, 0, 0, 0);
    frame->DrawKeyed(allianceBackground, 0, 0);
    frame->DrawKeyed(empireBackground, 0, 0);
    rightEdge->Blit(allianceBackground, 0x19c, 0, 0, 0, 0, 0);
    rightEdge->Blit(empireBackground, 0x19c, 0, 0, 0, 0, 0);

    const ResourceStringId titleText = StringId(0x1900);
    CoolLabel* title = NewOrNull<CoolLabel>(&titleText, 0x24, 0xe, 0, 0, 0u, 0u);
    title->SetFont(5);
    title->SetColor(kLabelColor, nullptr);
    title->m_width = 0x15e;
    title->m_layoutFlags = kLabelFixedWidth;
    title->m_drawFlags = 1;
    const ResourceStringId searchText = StringId(0x1901);
    CoolLabel* search = NewOrNull<CoolLabel>(&searchText, 0x24, 0x30, 0, 0, 0u, 0u);
    search->SetFont(4);
    search->SetColor(kLabelColor, nullptr);
    DibBitmap* const backgrounds[] = {allianceBackground, empireBackground};
    for (DibBitmap* background : backgrounds) {
        HGDIOBJ saved = SelectObject(m_dc, background->DibSectionHandle());
        title->Draw(m_dc);
        search->Draw(m_dc);
        SelectObject(m_dc, saved);
    }
    if (title) delete title;
    if (search) delete search;
    SetPaintBackground(allianceBackground, kAllianceBackground);
    AddOverlay(empireBackground, kEmpireBackground);
    DibBitmap* const sources[] = {allianceTitlePanel, empireTitlePanel, frame, rightEdge};
    for (DibBitmap* bitmap : sources)
        if (bitmap) delete bitmap;

    m_tabs = NewOrNull<CoolTabControl>(m_instance, 0x24, 0x48, 0x99, 0x29, kTabsId, this, GetPaintBackgroundIfOwned());
    if (m_tabs) {
        m_tabs->Create(0);
        CoolStrobeButton* allianceTab =
            NewButton(art, 0, 0, 0x31, 0x29, m_tabs, kAllianceTabId, 0x2906, 0x2907, m_tabs->m_background);
        if (allianceTab) {
            allianceTab->Create(0);
            allianceTab->SetClickSound(kClickSound);
            const ResourceStringId tip = StringId(0x1950);
            AddToolFromString(allianceTab, &tip);
            m_tabs->AddTab(allianceTab, 1, 0);
        }
        CoolStrobeButton* empireTab =
            NewButton(art, 0x34, 0, 0x31, 0x29, m_tabs, kEmpireTabId, 0x2909, 0x290a, m_tabs->m_background);
        if (empireTab) {
            empireTab->Create(0);
            empireTab->SetClickSound(kClickSound);
            const ResourceStringId tip = StringId(0x1951);
            AddToolFromString(empireTab, &tip);
            m_tabs->AddTab(empireTab, 0, 0);
        }
    }

    int closeX;
    int openY;
    uint32_t openId;
    uint32_t openPressedId;
    if (m_galaxy->m_side == kAlliance) {
        AddButton(0x1ab, 0x19, 0x2912, 0x2913, kCloseId, 0x1954, 0, 0, 0, 0, 0, 0);
        closeX = 0x1ab;
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

    m_nameField = NewOrNull<CoolStringField>(m_instance, 0x8f, 0x2d, 0xfa, 0x12, this, GetPaintBackgroundIfOwned(), 0u);
    m_nameField->m_childId = kNameFieldId;
    m_nameField->m_textColor = 0xffffff;
    m_nameField->Create(0);
    m_nameField->SetText(nullptr);
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

    m_list = NewOrNull<CoolDragList>(art, 0x25, 0x90, 0x15d, 0x9f, this, GetPaintBackgroundIfOwned(), kListId, 0x14a,
                                     0x19, 0x287f, 0x2880, 0x287d, 0x287e, 10, 1, nullptr);
    if (m_list) {
        m_list->m_options.m_value = kDragListSingleSelect | kDragListNoDrag;
        m_list->Create(0);
        m_list->m_textFormat = DT_SINGLELINE;
        CoolDragList* list = m_list;
        list->CreateVerticalScrollBar(m_instance, list, 7, 0x299d, list->m_clientHeight, 0, list->m_clientHeight, 0,
                                      nullptr);
        m_list->m_textOffsetX = 5;
        m_list->SetItemList(m_items);
    }
    if (m_tabs) m_tabs->SelectTab(m_galaxy->m_side == kAlliance ? kAllianceTabId : kEmpireTabId, 1);
    SetFocus(m_nameField->m_hwnd);
}

void TroopFinderWindow::OnCommand(int, int command, int) {
    const uint32_t code = static_cast<uint32_t>(command) >> 16;
    switch (command & 0xffff) {
    case kTabsId: {
        SetFilter(static_cast<int>(code), 0);
        const uint32_t id = m_list->SelectByPrefix(&m_nameField->m_text, 0, 0);
        if (id) {
            ObjectKey key;
            static_cast<ObjectListItem*>(m_items->FindById(id))->GetKey(&key);
            m_selected = key;
        }
        SetFocus(m_list->m_hwnd);
        break;
    }
    case kCloseId:
        PostCloseRequest();
        break;
    case kOpenId:
        if (m_selected.IsComplete()) {
            GameObjNotifier* holder = GetObjectInFamilies_01_ff(m_galaxy->m_side, &m_selected);
            TroopRegIterator regiments(holder, kStateFilterExisting);
            regiments.Begin();
            if (regiments.Current()) SelectFirstRegiment(&m_selected, holder);
            m_galaxy->OpenWindowForObject(&m_selected);
            PostCloseRequest();
        }
        break;
    case kListId: {
        CoolListItemList selection(kSortById);
        m_list->CopySelectionTo(&selection);
        CoolListItem* first = static_cast<CoolListItem*>(selection.m_head);
        if (!first) break;
        ObjectListItem* item = static_cast<ObjectListItem*>(m_items->FindById(first->m_id));
        ObjectKey key;
        item->GetKey(&key);
        m_selected = key;
        if (static_cast<uint16_t>(code) == kDragListActivated && item) {
            if (GameObjNotifier* holder = GetObjectInFamilies_01_ff(m_galaxy->m_side, &m_selected))
                SelectFirstRegiment(&m_selected, holder);
            m_galaxy->OpenWindowForObject(&m_selected);
            PostCloseRequest();
        } else if (static_cast<uint16_t>(code) == kDragListSelectionChanged) {
            m_nameField->SetText(first->m_text.m_text);
        }
        break;
    }
    }
}

LRESULT REB_STDCALL TroopFinderWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT result = 0;
    if (message < 0x11) {
        if (message == WM_CLOSE) {
            if (m_dc) {
                DeleteDC(m_dc);
                m_dc = nullptr;
            }
            for (HBRUSH& brush : m_brushes) {
                DeleteObject(brush);
                brush = nullptr;
            }
            return CoolGameWindow::WindowProc(WM_CLOSE, wParam, lParam);
        }
        if (message == WM_SETFOCUS) {
            SetFocus(m_nameField->m_hwnd);
            return 0;
        }
        return CoolGameWindow::WindowProc(message, wParam, lParam);
    }
    if (message == kFieldReturnMessage) {
        const ObjectKey unset;
        if (static_cast<int16_t>(wParam) == kNameFieldId && m_selected.m_value != unset.m_value) {
            GameObjNotifier* holder = GetObjectInFamilies_01_ff(m_galaxy->m_side, &m_selected);
            TroopRegIterator regiments(holder, kStateFilterExisting);
            regiments.Begin();
            if (regiments.Current()) SelectFirstRegiment(&m_selected, holder);
            m_galaxy->OpenWindowForObject(&m_selected);
            PostCloseRequest();
        }
    } else if (message == kFieldChangedMessage) {
        if (static_cast<int16_t>(wParam) != kNameFieldId) return 0;
        CoolStringField* field = m_nameField;
        ObjectListItem* item = nullptr;
        if (field->m_text.GetBufferSize() != 0) {
            const uint32_t id = m_list->SelectByPrefix(&field->m_text, 0, 0);
            item = static_cast<ObjectListItem*>(m_items->FindById(id));
        }
        if (!item) {
            m_selected = ObjectKey();
        } else {
            ObjectKey key;
            item->GetKey(&key);
            m_selected = key;
        }
    } else {
        result = CoolGameWindow::WindowProc(message, wParam, lParam);
    }
    return result;
}

void TroopFinderWindow::OnKeyDown(WPARAM key, LPARAM lParam) {
    FinderOnKeyDown(this, m_tabs, m_nameField, m_list, key, lParam, false);
}

void TroopFinderWindow::SetFilter(int filter, int) {
    if (m_filter == filter) return;
    const uint32_t side = filter == 1 ? 1 : 2;
    SetPaintBackgroundById(side);
    m_list->SetBackground(GetPaintBackgroundIfOwned(), m_list->m_x, m_list->m_y);
    m_items->Clear();
    HDC windowDc = GetDC(m_hwnd);
    HDC dc = CreateCompatibleDC(windowDc);
    ReleaseDC(m_hwnd, windowDc);
    int rowCount = 0;
    auto addRow = [&](GameObj* object) {
        ObjectKey key;
        ObjectListItem* row = NewOrNull<ObjectListItem>(static_cast<uint32_t>(++rowCount), object->GetKey(&key));
        row->m_state.m_value = kListItemDisabled;
        row->m_text = object->GetName()->m_text;
        DibBitmap* picture =
            NewOrNull<DibBitmap>(static_cast<LONG>(0x14a), static_cast<LONG>(0x19), GetPaintBackgroundIfOwned(), dc);
        picture->FillRect(0, 0, picture->Width(), picture->Height(), 0);
        HGDIOBJ saved = SelectObject(dc, picture->DibSectionHandle());
        DrawTroopCounts(object, side, dc);
        SelectObject(dc, saved);
        row->SetImages(picture, nullptr);
        m_items->Insert(row);
    };
    for (GameObjNotifier* system = GetFirstObjectInFamilies_90_98(m_galaxy->m_side); system;
         system = static_cast<GameObj*>(system)->FindNextSibling(&family::kSystems, 1)) {
        TroopRegIterator regiments(system, kStateFilterExisting, side);
        if (regiments.Count() != 0) addRow(static_cast<GameObj*>(system));
        FleetIterator fleets(system, kStateFilterExisting, side);
        for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
            GameObj* fleet = static_cast<GameObj*>(fleets.Current());
            if (fleet->ControlSide() != side) continue;
            bool holdsRegiments = false;
            CapShipIterator ships(fleet, kStateFilterExisting);
            for (ships.Begin(); ships.Current(); ships.Advance()) {
                TroopRegIterator shipRegiments(ships.Current(), kStateFilterExisting);
                if (shipRegiments.Count() != 0) holdsRegiments = true;
            }
            if (holdsRegiments) addRow(fleet);
        }
    }
    DeleteDC(dc);
    m_filter = filter;
    m_list->Refresh();
    m_filterLabel->SetText(m_tabs->m_children.FindById(filter)->m_tooltipText);
    RECT area = m_filterLabel->m_rect;
    InvalidateRect(m_hwnd, &area, FALSE);
}

int TroopFinderWindow::DrawTroopCounts(GameObj* object, int side, HDC dc) {
    ForceCell cells[kCellCount];
    for (ForceCell& cell : cells) {
        cell.m_key.m_value = 0;
        cell.m_count = 0;
        cell.m_flags = 0;
    }
    CoolLabel label(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    String text;
    const uint32_t troopFamily = 0x10000000;
    const uint32_t allianceKinds[kCellCount] = {2, 1, 5, 3, 4};
    const uint32_t empireKinds[kCellCount] = {8, 0xa, 7, 6, 9};
    for (int i = 0; i < kCellCount; ++i)
        cells[i].m_key.m_value = troopFamily | (side == kAlliance ? allianceKinds[i] : empireKinds[i]);
    auto countRegiments = [&](TroopRegIterator& regiments) {
        for (regiments.Begin(); regiments.Current(); regiments.Advance())
            CountIntoCell(cells, kCellCount, static_cast<GameObj*>(regiments.Current()));
    };
    if (object->GetFamily() == family::kFleet) {
        CapShipIterator ships(object, kStateFilterExisting);
        for (ships.Begin(); ships.Current(); ships.Advance()) {
            TroopRegIterator regiments(ships.Current(), kStateFilterExisting, side);
            countRegiments(regiments);
        }
    } else {
        TroopRegIterator regiments(object, kStateFilterExisting, side);
        countRegiments(regiments);
    }
    DrawForceCells(cells, kCellCount, &label, &text, 0xc2, side, m_brushes, dc);
    return 0;
}
