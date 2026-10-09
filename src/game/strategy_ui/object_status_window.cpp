#include "strategy_ui/object_status_window.h"

#include <cstdint>
#include <new>

#include "ai/ai_common.h"
#include "app/app_cdcheck_version.h"
#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "core/strstream_io.h"
#include "core/vptr_store.h"
#include "data/dat_record.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gameobj/unit_obj_base.h"
#include "gfx/dib_bitmap.h"
#include "mission/mission.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"
#include "ui/resource_dll.h"

extern const char g_gokresDllName[];     // 0x6a8708 "gokres.dll"
extern const char g_attachedText[];      // 0x6a8738 "Attached:"
extern const char g_statusText[];        // 0x6a8744 "Status:"
extern const char g_tallySeparator[];    // 0x6a874c "  "

// The record key of an object (0x402d80) and its key (0x4025b0, GameObjRoot::GetKey out of line).
// The picture of an object for its status window (galaxy view routes, 0x42c3b0).
DibBitmap* CreateObjectPicture(HMODULE module, const ObjectKey* key, GameObjNotifier* object, int a, int b);

namespace {

constexpr uint32_t kSpriteSlot = 7;
constexpr uint32_t kGokresSlot = 10;
constexpr uint32_t kBitmapKind = 2;
constexpr uint32_t kCloseButton = 0x65;
constexpr uint32_t kEncyclopediaButton = 0x66;
constexpr uint32_t kListId = 0x67;
constexpr uint32_t kStyleVisible = 0x10000000;
constexpr uint32_t kStyleChild = 0x46000000;
constexpr UINT kCloseMessage = 0x40d;
constexpr int kRowHeight = 10;
constexpr int kLabelX = 0;
constexpr int kValueX = 0x67;
constexpr int kPanelWidth = 0xcf;
constexpr int kRowFont = 10;
constexpr int kTitleFont = 5;

class ScopedDc {
public:
    explicit ScopedDc(HWND hwnd) {
        HDC window = GetDC(hwnd);
        m_dc = CreateCompatibleDC(window);
        ReleaseDC(hwnd, window);
    }
    HDC get() const { return m_dc; }

private:
    HDC m_dc;
};

bool IsRootFamily(uint32_t family) { return family >= 0xf1 && family < 0xf2; }
bool InFamilies(uint32_t family, uint32_t first, uint32_t last) { return family >= first && family < last; }

void SetRowLabelStyle(CoolLabel* label) {
    label->SetFont(kRowFont);
    label->SetColor(0xffffff, nullptr);
    label->m_drawFlags = 0x10;
    label->m_layoutFlags = kLabelFixedWidth;
    label->m_width = kValueX;
}

CoolListItem* NewRowItem(uint32_t id) {
    void* memory = ::operator new(sizeof(CoolListItem));
    return memory ? new (memory) CoolListItem(id) : nullptr;
}

void AddRowText(CoolListItem* item, const String& text, CoolListItemList* list) {
    item->m_text = text.m_text;
    list->Insert(item);
}

CoolListItemList* NewItemList(int sortMode) { return NewOrNull<CoolListItemList>(sortMode); }

}  // namespace

// ---- Construction and window procedure -----------------------------------------------------------------

ObjectStatusWindow::ObjectStatusWindow(HINSTANCE instance, GalaxyWindow* parent, uint32_t childId, const ObjectKey* key)
    : CoolGameWindow(instance, 0, 0, 0x17b, 0x110, parent, 2, kSpriteSlot, 0, childId, 0, 1, 0, 0),
      m_key(),
      m_typeKey{0},
      m_titleLabel(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u),
      m_unusedLabel(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u),
      m_nameLabel(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u) {
    m_galaxy = parent;
    m_style = kStyleChild;
    m_unknown120 = nullptr;
    m_key = *key;
    m_object = GetObjectInFamilies_01_ff(m_galaxy->m_side, &m_key);
    if (m_object) {
        ObjectKey recordKey;
        m_typeKey.m_value = GetObjectRecordKey(m_object, &recordKey)->m_value;
    }
    m_gokres = FindResourceModule(kGokresSlot);
    if (!m_gokres) {
        char* dllLocation = GetDllLocation();
        m_gokres = LoadResourceModule(dllLocation, g_gokresDllName, kGokresSlot);
        if (dllLocation) operator delete(dllLocation);
    }
    m_items = nullptr;
}

ObjectStatusWindow::~ObjectStatusWindow() {
    REB_KEEP_VPTR_STORE();
    if (m_unknown120) {
        m_unknown120->~DibBitmap();
        operator delete(m_unknown120);
    }
    if (m_items) {
        m_items->Clear();
        delete m_items;
    }
}

LRESULT __stdcall ObjectStatusWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CREATE) {
        WPARAM self = reinterpret_cast<WPARAM>(m_hwnd);
        PostMessageA(GetGalaxyWindow()->m_hwnd, kGalaxyActivateChild, self, 0);
        CoolGameWindow::WindowProc(WM_CREATE, wParam, lParam);
        return 0;
    }
    if (message != kCloseMessage) return CoolGameWindow::WindowProc(message, wParam, lParam);
    PostCloseRequest();
    return 0;
}

void ObjectStatusWindow::OnCreate3() {
    AddButton(0x102, 0xda, 0x2d20, 0x2d21, kEncyclopediaButton, 0x1505, 0, 0, 0, 0, 0, 1);
    CoolStrobeButton* encyclopedia = static_cast<CoolStrobeButton*>(m_children.FindById(kEncyclopediaButton));
    bool noEncyclopedia = true;
    if (!IsRootFamily(m_key.Family()) && m_key.IsSet()) noEncyclopedia = false;
    if (noEncyclopedia && encyclopedia) {
        encyclopedia->LoadStateBitmap(kButtonDisabled, 0x2d5c);
        encyclopedia->SetStateFlags(kButtonDisabled);
    }
    AddButton(0x144, 0xda, 0x2882, 0x2883, kCloseButton, 0x1954, 0, 0, 0, 0, 0, 1);
}

void ObjectStatusWindow::DrawOverlay(HDC dc) {
    m_titleLabel.Draw(dc);
    m_nameLabel.Draw(dc);
}

void ObjectStatusWindow::OnKeyDown(WPARAM key, LPARAM lParam) {
    if (key != VK_ESCAPE) {
        m_parent->WindowProc(WM_KEYDOWN, key, lParam);
        return;
    }
    PostCloseRequest();
}

void ObjectStatusWindow::OnCommand(int a, int b, int c) {
    const uint32_t command = static_cast<uint32_t>(a) & 0xffff;
    if (command == kCloseButton) {
        PostCloseRequest();
    } else if (command == kEncyclopediaButton) {
        const NullableKey none = {0};
        ObjectKey unset;
        if (m_key.m_value != unset.m_value) {
            if (!IsRootFamily(m_key.Family())) {
                PostCloseRequest();
                ObjectKey noObject;
                UiOpenEncyclopedia(&noObject, &m_typeKey);
            }
        }
    }
}

void ObjectStatusWindow::OnCreate1() {
    HMODULE module = CoolAppGetResourceModule(kSpriteSlot);
    CoolAppGetResourceModule(2);
    const int side = m_galaxy->m_side;
    DibBitmap* picture = nullptr;
    uint32_t backdropId;
    if (!m_key.IsSet() || IsRootFamily(m_key.Family())) {
        backdropId = (side != 1 ? 4 : 0) + 0x2d22;
    } else {
        const uint32_t owner = (static_cast<GameObjRoot*>(m_object)->m_flags >> 6) & 3;
        if (owner == 1) {
            backdropId = (side != 1 ? 3 : 0) + 0x2d22;
        } else if (owner == 2) {
            backdropId = (side != 1 ? 3 : 0) + 0x2d23;
        } else {
            backdropId = (side != 1 ? 3 : 0) + 0x2d24;
        }
    }
    DibBitmap* backdrop = NewOrNull<DibBitmap>(module, backdropId, kBitmapKind);
    ScopedDc scoped(m_hwnd);
    HDC dc = scoped.get();
    const int backdropHeight = backdrop->Height();
    const int backdropWidth = backdrop->Width();
    DibBitmap* surface = NewOrNull<DibBitmap>(backdropWidth, backdropHeight, backdrop, dc);
    backdrop->Blit(surface, 0, 0, 0, 0, 0, 0);
    if (backdrop) {
        backdrop->~DibBitmap();
        operator delete(backdrop);
    }
    HGDIOBJ previous = SelectObject(dc, surface->DibSectionHandle());
    SelectObject(dc, previous);
    SetPaintBackground(surface, 10);
    SetFocus(m_hwnd);

    m_list = NewOrNull<CoolDragList>(module, 0x12, 0x2f, 0xd0, 0xcc, this, GetPaintBackgroundIfOwned(), kListId, 0xd0,
                                     0x23, 0, 0, 0, 0, 10, 1, nullptr);
    if (m_list) {
        m_items = NewItemList(4);
        m_list->m_style &= ~kStyleVisible;
        m_list->m_options.m_value = kDragListFlowLayout | kDragListNoDrag;
        m_list->SetFont(4);
        m_list->m_textFormat = 9;
        m_list->SetTextColor(0x2ffffff);
        m_list->SetDragTarget(m_galaxy->m_hwnd);
        m_list->Create(0);
        m_list->SetItemList(m_items);
        CoolDragList* list = m_list;
        list->CreateVerticalScrollBar(m_instance, list, 7, 0x299a, list->m_clientHeight, 0, list->m_clientHeight, 0,
                                      nullptr);
        m_children.Insert(m_list);
    }
    m_titleLabel.MoveTo(0xf, 0x12);
    m_titleLabel.m_width = 0xd3;
    m_titleLabel.m_height = 0x12;
    m_titleLabel.m_layoutFlags = 3;
    m_titleLabel.m_drawFlags = 0x21;
    m_titleLabel.SetColor(0xffffff, nullptr);
    m_titleLabel.SetFont(kTitleFont);
    m_nameLabel.MoveTo(0xf2, 0x89);
    m_nameLabel.m_width = 0x82;
    m_nameLabel.m_height = 0x2c;
    m_nameLabel.m_layoutFlags = 3;
    m_nameLabel.m_drawFlags = 0x11;
    m_nameLabel.SetColor(0xffffff, nullptr);
    m_nameLabel.SetFont(kTitleFont);
    CenterIn(m_galaxy->m_panelArea.left, m_galaxy->m_panelArea.top, m_galaxy->m_panelArea.right,
             m_galaxy->m_panelArea.bottom);

    if (!m_key.IsSet()) {
        AddNameRow(0, nullptr, nullptr);
        ShowSidePage(module, &picture);
    } else if (IsRootFamily(m_key.Family())) {
        IdAvlTree tally;
        AddNameRow(0, nullptr, nullptr);
        ReplaceItemList();
        picture = NewOrNull<DibBitmap>(module, 0x2d46u, kBitmapKind);
        const int view = m_galaxy->m_side;
        struct Census {
            GameObjNotifier* (*m_first)(int);
            uint32_t m_firstFamily;
            uint32_t m_lastFamily;
        };
        static const Census kCensus[] = {
            {GetFirstObjectInFamilies_20_22, 0x20, 0x22}, {GetFirstObjectInFamilies_2a_2b, 0x2a, 0x2b},
            {GetFirstObjectInFamilies_18_1c, 0x18, 0x1c}, {GetFirstObjectInFamilies_25_26, 0x25, 0x26},
            {GetFirstObjectInFamilies_1c_20, 0x1c, 0x20}, {GetFirstObjectInFamilies_22_23, 0x22, 0x23},
            {GetFirstObjectInFamilies_2c_2d, 0x2c, 0x2d}, {GetFirstObjectInFamilies_24_25, 0x24, 0x25},
            {GetFirstObjectInFamilies_2d_2e, 0x2d, 0x2e}, {GetFirstObjectInFamilies_28_29, 0x28, 0x29},
            {GetFirstObjectInFamilies_3c_40, 0x3c, 0x40}, {GetFirstObjectInFamilies_14_18, 0x14, 0x18},
            {GetFirstObjectInFamilies_29_2a, 0x29, 0x2a}, {GetFirstObjectInFamilies_10_14, 0x10, 0x14},
            {GetFirstObjectInFamilies_23_24, 0x23, 0x24},
        };
        for (const Census& census : kCensus) {
            GameObjNotifier* first = census.m_first(view);
            const FamilyRange range(census.m_firstFamily, census.m_lastFamily);
            TallyUnitsByType(&tally, first, &range);
            AddTallyItems(&tally);
        }
        CoolListItem* firstItem = m_items->First();
        if (firstItem && firstItem->m_image) m_list->m_fillColor = firstItem->m_image->m_colorKey;
    } else {
        if (InFamilies(m_key.Family(), 0x50, 0x80)) {
            if (GameObjNotifier* object = GetObjectInFamilies_01_ff(m_galaxy->m_side, &m_key)) {
                uint32_t pictureId = *reinterpret_cast<uint32_t*>(
                    reinterpret_cast<char*>(object->m_record) + offsetof(DatRecord, m_small));
                if ((object->m_flags & 0xc0) == 0x40) {
                    pictureId &= 0xfff;
                } else {
                    pictureId = (pictureId & 0xfff) + 0x1000;
                }
                picture = NewOrNull<DibBitmap>(m_gokres, pictureId, kBitmapKind);
            }
        } else if (InFamilies(m_key.Family(), 0xa0, 0xb0)) {
            uint32_t pictureId;
            if (InFamilies(m_key.Family(), 0xa0, 0xa2)) {
                pictureId = 0x107;
            } else {
                pictureId = (InFamilies(m_key.Family(), 0xa2, 0xa4) ? 0xfffffffe : 0) + 0x108;
            }
            picture = NewOrNull<DibBitmap>(m_gokres, pictureId, kBitmapKind);
        } else {
            picture = CreateObjectPicture(m_gokres, &m_key, m_object, 1, 1);
        }
        CoolListItem* item = NewRowItem(1);
        if (item) {
            CoolListItemList labels(1);
            CoolListItemList values(1);
            const uint32_t family = m_key.Family();
            if (InFamilies(family, 8, 0x10)) {
                FillFleetPage(&labels, &values);
            } else if (InFamilies(family, 0x14, 0x1c)) {
                FillCapitalShipPage(&labels, &values);
            } else if (InFamilies(family, 0x30, 0x3c)) {
                FillCharacterPage(&labels, &values);
            } else if (InFamilies(family, 0x20, 0x28)) {
                FillFacilityPage(&labels, &values);
            } else if (InFamilies(family, 0x1c, 0x20)) {
                FillFighterSquadronPage(&labels, &values);
            } else if (InFamilies(family, 0x10, 0x14)) {
                FillTroopPage(&labels, &values);
            } else if (InFamilies(family, 0x28, 0x30)) {
                FillProductionFacilityPage(&labels, &values);
            } else if (InFamilies(family, 0x90, 0x98)) {
                FillSystemPage(&labels, &values);
            } else if (InFamilies(family, 0x50, 0x80)) {
                if (!static_cast<Mission*>(m_object)->GetRecordHidden()) FillMissionPage(&labels, &values);
            } else if (InFamilies(family, 0x3c, 0x40)) {
                FillSpecialForcesPage(&labels, &values);
            } else if (InFamilies(family, 0xa0, 0xb0)) {
                FillManufacturingPage(&labels, &values);
            } else {
                return;
            }
            const int height = LayoutStatusLabels(dc, &labels, &values);
            if (height != 0) {
                DibBitmap* rows = NewOrNull<DibBitmap>(kPanelWidth, height, GetPaintBackgroundIfOwned(), dc);
                const int rowsHeight = rows->Height();
                const int rowsWidth = rows->Width();
                rows->FillRect(0, 0, rowsWidth, rowsHeight, 0);
                HGDIOBJ old = SelectObject(dc, rows->DibSectionHandle());
                DrawStatusLabels(dc, &labels, &values);
                SelectObject(dc, old);
                const int copyHeight = rows->Height();
                const int copyWidth = rows->Width();
                DibBitmap* copy = NewOrNull<DibBitmap>(copyWidth, copyHeight, rows, dc);
                rows->Blit(copy, 0, 0, 0, 0, 0, 0);
                item->SetImages(rows, copy);
                m_list->SetItemSize(kPanelWidth, height);
            }
            m_items->Insert(item);
        }
    }
    if (picture) {
        if (picture->IsLoaded()) {
            const int x = 0x133 - picture->Width() / 2;
            const int y = 0x40 - picture->Height() / 2;
            picture->DrawKeyed(GetPaintBackgroundIfOwned(), x, y);
        }
        picture->~DibBitmap();
        operator delete(picture);
    }
    m_list->Refresh();
    m_list->Show(5);
    DeleteDC(dc);
}

void ObjectStatusWindow::ReplaceItemList() {
    if (m_items) delete m_items;
    m_items = NewItemList(1);
    m_list->SetItemList(m_items);
}

void ObjectStatusWindow::ShowSidePage(HMODULE module, DibBitmap** picture) {
    ReplaceItemList();
    *picture = NewOrNull<DibBitmap>(module, 0x2d46u, kBitmapKind);
    m_list->m_textFormat = 0x24;
    m_list->m_textOffsetX = 0x28;
    if (m_galaxy->m_side == 1) {
        FillAlliancePage();
    } else {
        FillEmpirePage();
    }
}

// ---- Rows --------------------------------------------------------------------------------------------------

void AddStatusRow(uint32_t id, String* text, CoolListItemList* list) {
    CoolListItem* item = NewRowItem(id);
    item->m_text = text->m_text;
    list->Insert(item);
}

void ObjectStatusWindow::AddNameRow(int unusedRow, CoolListItemList* labels, CoolListItemList* values) {
    if (!m_key.IsSet()) {
        const ResourceStringId text = {0x3114, 2};
        m_nameLabel.SetTextFromResource(&text);
        return;
    }
    if (IsRootFamily(m_key.Family())) {
        const ResourceStringId text = {0x3113, 2};
        m_nameLabel.SetTextFromResource(&text);
        return;
    }
    m_nameLabel.SetText(static_cast<GameObj*>(m_object)->GetName()->m_text);
}

void ObjectStatusWindow::AddAttachedRows(int row, CoolListItemList* labels, CoolListItemList* values) {
    String text;
    text.Clear();
    if (GetVersionCheckResult() == 0) {
        const ResourceStringId id = {static_cast<int16_t>(0x87b0), 2};
        text.SetFromResource(&id);
    } else {
        String attached(g_attachedText);
        text += attached;
    }
    AddRowText(NewRowItem(row), text, labels);
    text.Clear();
    String parentName(static_cast<GameObj*>(m_object->m_parent)->GetName()->m_text);
    text += parentName;
    AddRowText(NewRowItem(row), text, values);
}

void ObjectStatusWindow::AddStatusRows(int row, CoolListItemList* labels, CoolListItemList* values) {
    String text;
    text.Clear();
    if (GetVersionCheckResult() == 0) {
        const ResourceStringId id = {static_cast<int16_t>(0x8666), 2};
        text.SetFromResource(&id);
    } else {
        String status(g_statusText);
        text += status;
    }
    AddRowText(NewRowItem(row), text, labels);
    text.Clear();
    CoolListItem* item = NewRowItem(row);
    const uint32_t state = static_cast<GameObj*>(m_object)->m_stateFlags;
    int16_t stringId;
    if ((state & 0x10) == 0) {
        stringId = (state & 4) == 0 ? 0x8757 : 0x8763;
    } else {
        stringId = 0x8744;
    }
    const ResourceStringId id = {stringId, 2};
    text.SetFromResource(&id);
    AddRowText(item, text, values);
}

void ObjectStatusWindow::AddCombatValueRow(int row, CoolListItemList* labels, CoolListItemList* values) {
    String text;
    const ResourceStringId id = {static_cast<int16_t>(0x8651), 2};
    text.SetFromResource(&id);
    AddRowText(NewRowItem(row), text, labels);
    text.Clear();
    ObjectKey key;
    GetObjectKey(m_object, &key);
    const FamilyRange units(0x10, 0x40);
    uint32_t value = 0;
    if (key.Family() >= units.m_first && key.Family() < units.m_second) {
        value = static_cast<UnitObjBase*>(m_object)->GetRecordField4c();
    }
    text.AppendUnsigned(value);
    AddRowText(NewRowItem(row), text, values);
}

// ---- Layout of the rows -------------------------------------------------------------------------------------

int __stdcall LayoutStatusLabels(HDC dc, CoolListItemList* labels, CoolListItemList* values) {
    int total = 0;
    CoolLabel left(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    CoolLabel right(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    SetRowLabelStyle(&left);
    SetRowLabelStyle(&right);
    int leftY = 0;
    int rightY = 0;
    CoolListItem* label = labels->First();
    for (CoolListItem* value = values->First(); label && value; value = value->Next()) {
        left.MoveTo(kLabelX, leftY);
        left.SetText(label->m_text.m_text);
        left.Measure(dc);
        const int leftBottom = left.m_rect.bottom;
        const int leftTop = left.m_rect.top;
        right.MoveTo(kValueX, rightY);
        right.SetText(value->m_text.m_text);
        right.Measure(dc);
        label->m_x = kLabelX;
        label->m_y = leftY;
        const uint32_t leftHeight = leftBottom - leftTop;
        uint32_t rowHeight = right.m_rect.bottom - right.m_rect.top;
        value->m_x = kValueX;
        value->m_y = rightY;
        if (rowHeight < leftHeight) {
            value->m_y = (leftHeight - rowHeight) + rightY;
            rowHeight = leftHeight;
        }
        leftY += rowHeight;
        total += rowHeight;
        rightY += rowHeight;
        label = label->Next();
    }
    return total;
}

void __stdcall DrawStatusLabels(HDC dc, CoolListItemList* labels, CoolListItemList* values) {
    CoolLabel left(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    CoolLabel right(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    SetRowLabelStyle(&left);
    SetRowLabelStyle(&right);
    CoolListItem* label = labels->First();
    for (CoolListItem* value = values->First(); label && value; value = value->Next()) {
        left.MoveTo(label->m_x, label->m_y);
        left.SetText(label->m_text.m_text);
        left.Measure(dc);
        left.Draw(dc);
        right.MoveTo(value->m_x, value->m_y);
        right.SetText(value->m_text.m_text);
        right.Measure(dc);
        right.Draw(dc);
        label = label->Next();
    }
}

// ---- Census of the units of the player's side -------------------------------------------------------------

StatusTallyNode::StatusTallyNode() : m_total(0), m_count(0) {}

StatusTallyNode::~StatusTallyNode() { REB_KEEP_VPTR_STORE(); }

void ObjectStatusWindow::TallyUnitsByType(IdAvlTree* tally, GameObjNotifier* first, const FamilyRange* range) {
    tally->Clear();
    ObjectKey lastKey;
    GameObjNotifier* object = first;
    if (!object) return;
    GetObjectKey(object, &lastKey);
    do {
        if (static_cast<GameObj*>(object)->m_stateFlags & 0x40) {
            const int side = m_galaxy->m_side;
            const uint32_t sideView = side == 1 ? 1 : (side == 2 ? 2 : 0);
            if (sideView == ((object->m_flags >> 6) & 3)) {
                GetObjectKey(object, &lastKey);
                const uint32_t recordId = object->m_record->m_small & 0xffff;
                StatusTallyNode* node = static_cast<StatusTallyNode*>(tally->FindById(recordId));
                if (!node) {
                    node = NewOrNull<StatusTallyNode>();
                    if (node) node->m_key = lastKey;
                    node->SetId(recordId);
                    tally->AddWithId(node);
                }
                node->m_count += 1;
                const FamilyRange units(0x10, 0x40);
                if (lastKey.Family() >= units.m_first && lastKey.Family() < units.m_second) {
                    node->m_total += static_cast<UnitObjBase*>(object)->GetRecordField4c();
                }
            }
        }
        object = static_cast<GameObj*>(object)->FindNextSibling(range, 1);
    } while (object);
}

void ObjectStatusWindow::AddTallyItems(AvlTree* tally) {
    for (AvlNode* entry = tally->First(); entry; entry = entry->m_successor) {
        StatusTallyNode* node = static_cast<StatusTallyNode*>(entry);
        GameObjNotifier* object = GetObjectInFamilies_01_ff(m_galaxy->m_side, &node->m_key);
        if (!object) continue;
        const uint32_t typeBits = static_cast<GameObj*>(object)->GetRecordTypeBits();
        DibBitmap* image = NewOrNull<DibBitmap>(m_gokres, (typeBits & 0xfff) + 0x4000, 10u);
        ObjectKey key;
        GetObjectKey(object, &key);
        CoolListItem* item = NewRowItem(key.Family());
        item->m_state.m_value = 4;
        item->SetImages(image, nullptr);
        String text;
        StrStream stream;
        strstream_ctor(&stream, 1);
        ostream_seekp_pos(stream.Out(), 0);
        ClearIosState(&stream);
        char* ios = IosPartOf(&stream);
        ios[0x2c] = '0';
        *reinterpret_cast<int*>(ios + 0x30) = 3;
        ostream_operator_ls_int(stream.Out(), node->m_count);
        ostream_operator_ls_cstr(stream.Out(), g_tallySeparator);
        *reinterpret_cast<int*>(IosPartOf(&stream) + 0x30) = 4;
        ostream_operator_ls_int(stream.Out(), node->m_total);
        ostream_operator_ls_char(stream.Out(), 0);
        text = strstreambuf_str(StreambufOf(&stream));
        strstreambuf_freeze(StreambufOf(&stream), 0);
        item->m_text = text.m_text;
        m_items->Insert(item);
        strstream_dtor(stream.Ios());
        ios_dtor(stream.Ios());
    }
}
