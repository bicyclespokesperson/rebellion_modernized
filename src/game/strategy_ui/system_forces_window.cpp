#include "strategy_ui/system_forces_window.h"

#include <new>

#include "app/rebapp_globals.h"
#include "audio/sound_manager.h"
#include "character/role.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/stream_io.h"
#include "core/vptr_store.h"
#include "data/dat_record.h"
#include "economy/defense_facility.h"
#include "fleet/fight_squad.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_label.h"
#include "ui/cool_tab_control.h"
#include "ui/object_list_item.h"
#include "ui/resource_dll.h"

extern const char g_gokresDllName[];  // 0x6a8708 "gokres.dll"
extern const char g_spaceString[];    // 0x6a7f40 " "

// The system the window shows (null, and the window closes itself, when it is gone).
GameObjNotifier* __thiscall FUN_004a25c0(void* window);

namespace {

constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kGokresSlot = 10;
constexpr uint32_t kPictureKind = 2;
constexpr uint32_t kHeaderKind = 10;
constexpr int16_t kStringGroup = 2;
constexpr uint32_t kHiddenStyle = 0x10000000;
constexpr uint32_t kOpenSound = 0x25e;
constexpr uint32_t kCommandMessage = 0x111;
constexpr uint32_t kRedrawHeaderMessage = 0x467;
constexpr uint32_t kRedrawHeaderCommand = 0x466;
constexpr uint32_t kBackgroundOverlayId = 0x32;

constexpr uint32_t kTabsId = 100;
constexpr uint32_t kCloseId = 200;
constexpr uint32_t kSideId = 0xc9;
constexpr uint32_t kSectorId = 0xca;
constexpr uint32_t kListId = 0xcb;

constexpr int kTabPersonnel = 1;
constexpr int kTabTroops = 2;
constexpr int kTabFighters = 3;
constexpr int kTabShields = 4;
constexpr int kTabWeapons = 5;

ResourceStringId StringId(uint16_t id) { return ResourceStringId{static_cast<int16_t>(id), kStringGroup}; }

void DeleteBitmap(DibBitmap* bitmap) {
    if (!bitmap) return;
    bitmap->~DibBitmap();
    ::operator delete(bitmap);
}

void DeleteLabel(CoolLabel* label) {
    if (!label) return;
    label->~CoolLabel();
    ::operator delete(label);
}

DibBitmap* LoadBitmap(HMODULE module, uint32_t id, uint32_t kind = kPictureKind) {
    return NewOrNull<DibBitmap>(module, id, kind);
}

bool FamilyIn(const ObjectKey& key, uint32_t first, uint32_t end) { return key.Family() >= first && key.Family() < end; }

GameObjNotifier* ParentOf(GameObjNotifier* object) { return object->m_parent; }

const char* NameOf(GameObjNotifier* object) { return static_cast<GameObj*>(object)->GetName()->m_text; }

// A picture of the set that has a greyed variant two ids lower.
uint32_t PictureIfAny(int count, uint32_t base) { return base - (count != 0 ? 2 : 0); }

bool NotOnVisibleMission(Role* role) {
    ObjectKey missionKey;
    role->GetMissionKey(&missionKey);
    return missionKey.IsSet() == 0 || (role->m_flags78 & role_flag::kOnHiddenMission) != 0;
}

CoolStrobeButton* NewTab(HMODULE art, CoolTabControl* tabs, int x, uint32_t id, uint32_t normal, uint32_t pressed) {
    return NewOrNull<CoolStrobeButton>(art, x, 0, 0x24, 0x21, tabs, id, normal, pressed, tabs->m_background, 0, 1);
}

CoolLabel* NewLabel(int x, int y, int width, int height, uint32_t drawFlags) {
    CoolLabel* label = NewOrNull<CoolLabel>(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    label->MoveTo(x, y);
    label->m_width = width;
    label->m_height = height;
    label->SetFont(4);
    label->m_layoutFlags = kLabelFixedWidth | kLabelFixedHeight;
    label->m_drawFlags = drawFlags;
    label->SetColor(0xffffff, nullptr);
    return label;
}

}  // namespace

// ---- Construction ---------------------------------------------------------------------------------------------

SystemForcesWindow::SystemForcesWindow(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                                       uint32_t childId, GameObjNotifier* system)
    : CustomDialogBox(instance, x, y, width, height, parent, childId, nullptr) {
    CoolGameWindow::m_flags |= kGameWindowDraggable;
    m_stringSlot = 2;
    m_bitmapSlot = 7;
    m_galaxy = static_cast<GalaxyWindow*>(parent);
    if (system) {
        ObjectKey key;
        system->GetKey(&key);
        m_systemKey = key;
    }
    m_tab = 0;
    m_headerActive = nullptr;
    m_headerInactive = nullptr;
    m_tabLabel = nullptr;
    m_infoLabel = nullptr;
    m_rowOverlay = nullptr;
    m_unused178 = nullptr;
    m_gokres = FindResourceModule(kGokresSlot);
    if (!m_gokres) {
        char* dllLocation = GetDllLocation();
        m_gokres = LoadResourceModule(dllLocation, g_gokresDllName, kGokresSlot);
        if (dllLocation) operator delete(dllLocation);
    }
}

SystemForcesWindow::~SystemForcesWindow() {
    REB_KEEP_VPTR_STORE();
    if (m_tabs) delete m_tabs;
    DeleteBitmap(m_headerActive);
    DeleteBitmap(m_headerInactive);
    DeleteLabel(m_tabLabel);
    DeleteLabel(m_infoLabel);
    DeleteBitmap(m_rowOverlay);
    DeleteBitmap(m_unused178);
    if (m_items) {
        m_items->Clear();
        if (m_items) delete m_items;
    }
}

void SystemForcesWindow::OnCreate1() {
    GameObjNotifier* system = FUN_004a25c0(this);
    if (!system) return;
    m_side = static_cast<int>(system->ControlSide());
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    CoolAppGetResourceModule(2);
    SetDragLimits(m_galaxy->m_panelArea, nullptr);
    SetDragMode(kDragOutline);
    DibBitmap* background = LoadBitmap(art, 0x2951, kHeaderKind);
    SetPaintBackground(background, kBackgroundOverlayId);
    DibBitmap* closeShape = LoadBitmap(art, 0x277c);
    int buttonsLeft = 0;
    if (closeShape) {
        buttonsLeft = m_width - closeShape->Width() - 3;
        AddButton(buttonsLeft, 3, 0x277c, 0x277d, kCloseId, 0, 0, 0, 0, 0, 0, 1);
        DeleteBitmap(closeShape);
    }
    DibBitmap* sideShape = LoadBitmap(art, 0x280d);
    if (sideShape) {
        AddButton(buttonsLeft - sideShape->Width(), 3, 0x280d, 0x280e, kSideId, 0, 0, 0, 0, 0, 0, 1);
        DeleteBitmap(sideShape);
    }
    DibBitmap* sectorShape = LoadBitmap(art, 0x27e1);
    AddButton(3, 3, 0x27e1, 0x27e0, kSectorId, 0, 0, 0, 0, 0, 0, 1);
    int titleLeft = 2;
    if (sectorShape) {
        titleLeft = sectorShape->Width() + 5;
        DeleteBitmap(sectorShape);
    }
    m_titleLabel->MoveTo(titleLeft, 2);
    m_titleLabel->m_height = 0x10;
    m_titleLabel->SetText(NameOf(system));
    m_titleLabel->SetFont(5);
    m_titleLabel->m_drawFlags = 0x24;
    SetRect(&m_titleRect, 2, 2, 0xe9, 0x12);

    m_tabLabel = NewLabel(2, 0x33, 0xe7, 0x10, 0x25);
    m_infoLabel = NewLabel(2, 0x3f, 0xe4, 0x11, 0x25);

    m_tabs = NewOrNull<CoolTabControl>(m_instance, 0, 0x14, 0x130, 0x21, kTabsId, this, GetPaintBackgroundIfOwned());
    if (m_tabs) {
        m_tabs->Create(0);
        CoolStrobeButton* weapons = NewTab(art, m_tabs, 0xac, kTabWeapons, 0x2936, 0x2937);
        weapons->Create(0);
        const ResourceStringId weaponsTip = StringId(0x1740);
        AddToolFromString(weapons, &weaponsTip);
        m_tabs->AddTab(weapons, 1, 0);
        CoolStrobeButton* shields = NewTab(art, m_tabs, 0x88, kTabShields, 0x2939, 0x293a);
        shields->Create(0);
        shields->LoadStateBitmap(0x40, 0x293b);
        shields->LoadStateBitmap(2, 0x2939);
        const ResourceStringId shieldsTip = StringId(0x1741);
        AddToolFromString(shields, &shieldsTip);
        m_tabs->AddTab(shields, 0, 1);
        const struct {
            int x;
            uint32_t id;
            uint32_t picture;
            uint16_t tooltip;
        } others[] = {{100, kTabFighters, 0x2942, 0x1742}, {0x40, kTabTroops, 0x2949, 0x1743}, {0x1c, kTabPersonnel, 0x2950, 0x1744}};
        for (const auto& tab : others) {
            CoolStrobeButton* button = NewTab(art, m_tabs, tab.x, tab.id, tab.picture, tab.picture);
            button->Create(0);
            const ResourceStringId tip = StringId(tab.tooltip);
            AddToolFromString(button, &tip);
            m_tabs->AddTab(button, 0, 1);
        }
    }
    m_list = NewOrNull<CoolDragList>(art, 7, 0x51, 0xde, 0xd2, this, background, kListId, 0x46, 0x46, 0, 0, 0, 0, 10, 1, nullptr);
    if (m_list) {
        m_items = NewOrNull<CoolListItemList>(4);
        m_list->m_options.m_value = 0xa0000;
        m_list->SetFont(10);
        m_list->m_textFormat = 0x10;
        m_list->m_textOffsetY = 0x1b;
        m_list->SetFont(10);
        m_list->SetTextColor(0x2ffffff);
        m_list->SetDragTarget(m_galaxy->m_hwnd);
        m_list->Create(0);
        m_list->SetSortMode(4);
        m_list->SetItemList(m_items);
        m_list->CreateVerticalScrollBar(m_instance, m_list, 7, 0x29cc, m_list->m_clientHeight, 0, m_list->m_clientHeight, 0, nullptr);
        m_children.Insert(m_list);
    }
    LoadHeaderBitmaps();
    m_tabs->SelectTab(1, 0);
    ShowTab(1);
    PostMessageA(m_galaxy->m_hwnd, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
    if ((m_galaxy->m_mouseMode & 4) == 0) {
        SoundRequest request(0, 0, 0, 0, 0, 0);
        request.m_kind = 1;
        request.m_soundId = kOpenSound;
        request.m_moduleIndex = kBitmapSlot;
        SoundManager::Play(&request);
    }
}

// ---- Messages -------------------------------------------------------------------------------------------------

LRESULT __stdcall SystemForcesWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message < 0x202) {
        if (message == WM_LBUTTONDOWN) {
            SendMessageA(m_galaxy->m_hwnd, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
            SetFocus(m_list->m_hwnd);
            CustomDialogBox::WindowProc(message, wParam, lParam);
            return 0;
        }
        if (message == WM_SETFOCUS) {
            SetFocus(m_list->m_hwnd);
            return 0;
        }
    } else {
        if (message == WM_RBUTTONDOWN) {
            SendMessageA(m_galaxy->m_hwnd, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
            SetFocus(m_list->m_hwnd);
            CustomDialogBox::WindowProc(message, wParam, lParam);
            return 0;
        }
        if (message == kRedrawHeaderMessage) {
            InvalidateRect(m_hwnd, &m_titleRect, 0);
            return 0;
        }
    }
    return CustomDialogBox::WindowProc(message, wParam, lParam);
}

void SystemForcesWindow::OnCommand(int, int command, int lParam) {
    HWND galaxyWindow = m_galaxy->m_hwnd;
    const uint32_t code = static_cast<uint32_t>(command) >> 16;
    switch (static_cast<uint32_t>(command) & 0xffff) {
    case kTabsId:
        SendMessageA(galaxyWindow, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
        ShowTab(static_cast<int>(code));
        SetFocus(m_list->m_hwnd);
        return;
    case kCloseId:
        PostCloseRequest();
        return;
    case kSideId:
        PostMessageA(galaxyWindow, kCommandMessage, (m_childId & 0xffff) | (kRedrawHeaderCommand << 16), 0);
        return;
    case kSectorId: {
        GameObjNotifier* system = FUN_004a25c0(this);
        if (system && m_galaxy) {
            ObjectKey sectorKey;
            if (static_cast<GameObj*>(system)->GetValidParentKey(&sectorKey) != 0) m_galaxy->OpenSectorWindow(&sectorKey);
        }
        return;
    }
    case kListId:
        if (code == kDragListDropped) {
            SendMessageA(galaxyWindow, kCommandMessage, (m_childId & 0xffff) | (kDragListDropped << 16), lParam);
        } else {
            SendMessageA(galaxyWindow, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
        }
        return;
    default:
        return;
    }
}

void SystemForcesWindow::OnKeyDown(WPARAM key, LPARAM lParam) {
    if (key == VK_ESCAPE) {
        PostCloseRequest();
        return;
    }
    if (key != VK_LEFT && key != VK_RIGHT) {
        m_parent->WindowProc(WM_KEYDOWN, key, lParam);
        return;
    }
    const uint32_t selectedId = m_tabs->m_selected ? m_tabs->m_selected->m_childId : 0;
    AvlNode* tab = m_tabs->m_children.FindById(selectedId);
    if (!tab) return;
    tab = key == VK_LEFT ? tab->Predecessor() : tab->m_successor;
    if (!tab) return;
    do {
        CoolWindow* window = static_cast<CoolWindow*>(tab);
        if (IsWindowVisible(window->m_hwnd) != 0) {
            m_tabs->SelectTab(window->m_childId, 1);
            tab = nullptr;
        } else {
            tab = key == VK_LEFT ? tab->Predecessor() : tab->m_successor;
        }
    } while (tab);
}

void SystemForcesWindow::SaveState(ostream* out) {
    ObjectKey key = m_systemKey;
    uint32_t childId = m_childId;
    key.Write(out);
    WriteValue32(out, &childId);
}

void SystemForcesWindow::LoadState(istream* in) {
    ObjectKey key;
    key.Read(in);
    m_systemKey = key;
    uint32_t childId;
    ReadValue32(in, &childId);
    m_childId = childId;
}

void SystemForcesWindow::DrawOverlay(HDC dc) {
    DibBitmap* header = m_galaxy->m_activeChild == m_hwnd ? m_headerActive : m_headerInactive;
    header->DrawToDc(dc, 0x40, 0xcc0020, m_titleRect.left, m_titleRect.top, 0, 0, 0, 0);
    m_tabLabel->Draw(dc);
    m_titleLabel->Draw(dc);
    m_infoLabel->Draw(dc);
}

ObjectKey SystemForcesWindow::GetKeyAt(int x, int y) {
    ObjectKey result = m_systemKey;
    if (m_list) {
        POINTS inList = {static_cast<SHORT>(x - 7), static_cast<SHORT>(y - 0x51)};
        if (CoolListItem* item = m_list->HitTest(m_list->ClientToList(inList))) {
            ObjectKey itemKey;
            static_cast<ObjectListItem*>(item)->GetKey(&itemKey);
            result = itemKey;
        }
    }
    return result;
}

ObjectKey SystemForcesWindow::GetDropKeyAt(int, int) { return m_systemKey; }

int SystemForcesWindow::Virtual29() {
    GameObjNotifier* system = FUN_004a25c0(this);
    if (!system) return 0;
    const uint32_t side = system->ControlSide();
    if (side != 1) {
        if (side != 2) return 0x2d0f;
        return 0x2d0e;
    }
    return 0x2d0d;
}

void SystemForcesWindow::ShowObjectPage(const ObjectKey* key) {
    int tab = 0;
    if (FamilyIn(*key, 0x22, 0x24)) {
        tab = kTabWeapons;
    } else if (FamilyIn(*key, 0x24, 0x26)) {
        tab = kTabShields;
    } else if (FamilyIn(*key, 0x1c, 0x20)) {
        tab = kTabFighters;
    } else if (FamilyIn(*key, 0x10, 0x14)) {
        tab = kTabTroops;
    } else if (FamilyIn(*key, 0x30, 0x3c)) {
        tab = kTabPersonnel;
    }
    if (tab != m_tab) m_tabs->SelectTab(static_cast<uint32_t>(tab), 1);
}

// ---- The rows -------------------------------------------------------------------------------------------------

void SystemForcesWindow::GetSelection(ObjectKeyValueSet* out, CoolWindow*) {
    CoolListItemList selection(1);
    if (!m_list) return;
    m_list->CopySelectionTo(&selection);
    for (CoolListItem* picked = static_cast<CoolListItem*>(selection.m_head); picked; picked = picked->Next()) {
        ObjectListItem* row = static_cast<ObjectListItem*>(m_items->FindById(picked->m_id));
        ObjectKey key;
        row->GetKey(&key);
        out->Add(NewOrNull<ObjectKeyValueNode>(key, 0));
    }
}

void SystemForcesWindow::OnChildAdded(const ObjectKey* key, GameObjNotifier* object) {
    GameObjNotifier* system = FUN_004a25c0(this);
    if (!system) return;
    ObjectKey unused;
    const bool isNew = m_objects.Find(key) == nullptr;
    if (!object) object = GetObjectInFamilies_01_ff(m_galaxy->m_side, key);
    if (!object) return;
    bool addKey = false;
    bool update = false;
    ObjectKey systemKey;
    system->GetKey(&systemKey);
    ObjectKey parentKey;
    ParentOf(object)->GetKey(&parentKey);
    if (parentKey.m_value == systemKey.m_value) {
        if (FamilyIn(*key, 0x22, 0x24)) {
            update = true;
            if (m_tab == kTabWeapons && isNew) addKey = true;
        } else if (FamilyIn(*key, 0x24, 0x26)) {
            update = true;
            if (m_tab == kTabShields && isNew) addKey = true;
        } else if (FamilyIn(*key, 0x1c, 0x20)) {
            update = true;
            if (m_tab == kTabFighters && isNew) addKey = true;
        } else if (FamilyIn(*key, 0x10, 0x14)) {
            update = true;
            if (m_tab == kTabTroops && isNew) addKey = true;
        } else if (FamilyIn(*key, 0x30, 0x40)) {
            if (NotOnVisibleMission(static_cast<Role*>(object)) && isNew) {
                update = true;
                if (m_tab == kTabPersonnel) addKey = true;
            }
        }
    }
    if (addKey) m_objects.Add(NewOrNull<ObjectKeyIdNode>(key, 0u), 0);
    if (update) OnObjectUpdated(key, object);
}

void SystemForcesWindow::OnChildMoved(const ObjectKey* key, GameObjNotifier* object, const ObjectKey*, GameObjNotifier*) {
    if (ObjectKeyNode* node = m_objects.Find(key)) {
        m_objects.Remove(static_cast<ObjectKeyIdNode*>(node));
        OnObjectUpdated(key, object);
    }
}

void SystemForcesWindow::OnObjectUpdated(const ObjectKey* key, GameObjNotifier* object) {
    GameObjNotifier* system = FUN_004a25c0(this);
    if (!system) return;
    DibBitmap* overlay = m_rowOverlay;
    int tabToRefresh = m_tab;
    bool rowAffected = false;
    uint32_t pictureId = 0;
    if (FamilyIn(*key, 0x90, 0x98) && m_side != static_cast<int>(object->ControlSide())) {
        m_side = static_cast<int>(object->ControlSide());
        LoadHeaderBitmaps();
        const int shown = m_tab;
        m_tab = 0;
        m_tabs->SelectTab(static_cast<uint32_t>(shown), 1);
        InvalidateRect(m_hwnd, nullptr, 0);
    } else {
        if (FamilyIn(*key, 0x22, 0x24)) {
            if (m_tab == kTabWeapons) rowAffected = true;
            const int count = CountDefenseFacilities22(static_cast<GameObj*>(system), m_side, 3) +
                              CountDefenseFacilities23(static_cast<GameObj*>(system), m_side, 3);
            overlay = m_rowOverlay;
            tabToRefresh = kTabWeapons;
            pictureId = PictureIfAny(count, 0x2938);
        } else if (FamilyIn(*key, 0x24, 0x26)) {
            if (m_tab == kTabShields) rowAffected = true;
            const int count = CountDefenseFacilities25(static_cast<GameObj*>(system), m_side, 3) +
                              CountDefenseFacilities24(static_cast<GameObj*>(system), m_side, 3);
            tabToRefresh = kTabShields;
            pictureId = PictureIfAny(count, 0x293b);
        } else if (FamilyIn(*key, 0x1c, 0x20)) {
            if (m_tab == kTabFighters) rowAffected = true;
            const int count = CountFightSquads(static_cast<GameObj*>(system), m_side, 3);
            if (m_side == 1) {
                tabToRefresh = kTabFighters;
                pictureId = PictureIfAny(count, 0x293e);
            } else if (m_side == 2) {
                tabToRefresh = kTabFighters;
                pictureId = PictureIfAny(count, 0x2941);
            } else {
                pictureId = 0x2942;
                tabToRefresh = kTabFighters;
            }
        } else if (FamilyIn(*key, 0x10, 0x14)) {
            if (m_tab == kTabTroops) rowAffected = true;
            const int count = CountTroopRegs(static_cast<GameObj*>(system), m_side, 3);
            if (m_side == 1) {
                tabToRefresh = kTabTroops;
                pictureId = PictureIfAny(count, 0x2945);
            } else if (m_side == 2) {
                tabToRefresh = kTabTroops;
                pictureId = PictureIfAny(count, 0x2948);
            } else {
                pictureId = 0x2949;
                tabToRefresh = kTabTroops;
            }
        } else if (FamilyIn(*key, 0x30, 0x40)) {
            if (m_tab == kTabPersonnel) rowAffected = true;
            int count = 0;
            RoleIterator roles(system, 3, static_cast<uint32_t>(m_side));
            for (roles.Begin(); roles.Current(); roles.Advance()) {
                if (NotOnVisibleMission(static_cast<Role*>(roles.Current()))) count++;
            }
            if (m_side == 1) {
                pictureId = PictureIfAny(count, 0x294c);
            } else if (m_side == 2) {
                pictureId = PictureIfAny(count, 0x294f);
            } else {
                pictureId = 0x2950;
            }
            tabToRefresh = kTabPersonnel;
        }
    }
    if (rowAffected) {
        CoolListItem* row = m_items->FindById(key->Id());
        if (!row) {
            AddRow(object, overlay);
        } else if (!m_objects.Find(key)) {
            CoolListItem* gone = m_items->FindById(key->Id());
            m_items->Remove(gone);
            if (gone) delete gone;
        } else {
            DibBitmap* image = row->m_image;
            DibBitmap* selected = row->m_selectedImage;
            DibBitmap* picture = CreateObjectPicture(m_gokres, key, nullptr, 0, 1);
            picture->Blit(image, 0, 0, 0, 0, 0, 0);
            picture->Blit(selected, 0, 0, 0, 0, 0, 0);
            row->m_text = NameOf(object);
            overlay->DrawKeyed(selected, 0, 0);
            DeleteBitmap(picture);
        }
        m_list->Refresh();
    }
    if (m_tab == kTabTroops && static_cast<uint32_t>(UiGetHumanSide()) == system->ControlSide()) {
        const ResourceStringId required = StringId(0x1947);
        String text(&required);
        text.AppendSigned(static_cast<System*>(system)->m_troopRegRequired);
        m_infoLabel->SetText(text.m_text);
        const RECT area = m_infoLabel->m_rect;
        InvalidateRect(m_hwnd, &area, 0);
    }
    if (tabToRefresh != 0 && static_cast<int16_t>(pictureId) != 0) {
        CoolStrobeButton* tab = static_cast<CoolStrobeButton*>(m_tabs->m_children.FindById(static_cast<uint32_t>(tabToRefresh)));
        if (static_cast<int16_t>(pictureId) != tab->GetStateResourceId(0)) {
            tab->LoadStateBitmap(0, pictureId);
            InvalidateRect(tab->m_hwnd, nullptr, 0);
        }
    }
}

void SystemForcesWindow::AddRow(GameObjNotifier* object, DibBitmap* overlay) {
    ObjectKey key;
    object->GetKey(&key);
    ObjectListItem* item = NewOrNull<ObjectListItem>(&key);
    ObjectKey pictureKey;
    object->GetKey(&pictureKey);
    DibBitmap* picture = CreateObjectPicture(m_gokres, &pictureKey, object, 0, 1);
    DibBitmap* selected = NewOrNull<DibBitmap>(picture->Width(), picture->Height(), picture);
    picture->Blit(selected, 0, 0, 0, 0, 0, 0);
    overlay->DrawKeyed(selected, 0, 0);
    item->SetImages(picture, selected);
    item->m_text = NameOf(object);
    m_list->m_fillColor = picture->m_colorKey;
    m_items->Insert(item);
    ObjectKey objectKey;
    object->GetKey(&objectKey);
    if (!m_objects.Find(&objectKey)) {
        ObjectKey nodeKey;
        object->GetKey(&nodeKey);
        m_objects.Add(NewOrNull<ObjectKeyIdNode>(&nodeKey, 0u), 0);
    }
}

int SystemForcesWindow::ShowTab(int tab) {
    CoolAppGetResourceModule(kBitmapSlot);
    GameObjNotifier* system = FUN_004a25c0(this);
    if (!system) return 0;
    if (m_tab == tab) return 0;
    m_items->Clear();
    m_objects.Clear();
    m_objects.Add(NewOrNull<ObjectKeyIdNode>(&m_systemKey, 0u), 0);
    const uint32_t side = static_cast<uint32_t>(m_side);
    switch (tab) {
    case kTabPersonnel: {
        m_list->m_options.m_value = (m_list->m_options.m_value & ~0x20000u) | 0x80000;
        RoleIterator roles(system, 3, side);
        for (roles.Begin(); roles.Current(); roles.Advance()) {
            if (NotOnVisibleMission(static_cast<Role*>(roles.Current()))) AddRow(roles.Current(), m_rowOverlay);
        }
        break;
    }
    case kTabTroops: {
        m_list->m_options.m_value = (m_list->m_options.m_value & ~0x20000u) | 0x80000;
        TroopRegIterator regiments(system, 3, side);
        for (regiments.Begin(); regiments.Current(); regiments.Advance()) AddRow(regiments.Current(), m_rowOverlay);
        break;
    }
    case kTabFighters: {
        m_list->m_options.m_value = (m_list->m_options.m_value & ~0x20000u) | 0x80000;
        FightSquadIterator squadrons(system, 3, side);
        for (squadrons.Begin(); squadrons.Current(); squadrons.Advance()) AddRow(squadrons.Current(), m_rowOverlay);
        break;
    }
    case kTabShields:
    case kTabWeapons: {
        m_list->m_options.m_value |= 0xa0000;
        DefenseFacilityIterator facilities(system, 3, side);
        const bool shields = tab == kTabShields;
        for (facilities.Begin(); facilities.Current(); facilities.Advance()) {
            GameObjNotifier* facility = facilities.Current();
            ObjectKey facilityKey;
            facility->GetKey(&facilityKey);
            if (shields ? FamilyIn(facilityKey, 0x24, 0x26) : FamilyIn(facilityKey, 0x22, 0x24)) AddRow(facility, m_rowOverlay);
        }
        break;
    }
    }
    const uint32_t selectedId = m_tabs->m_selected ? m_tabs->m_selected->m_childId : 0;
    CoolStrobeButton* selectedTab = static_cast<CoolStrobeButton*>(m_tabs->m_children.FindById(selectedId));
    m_tabLabel->SetText(selectedTab->GetTooltipText());
    const RECT labelArea = m_tabLabel->m_rect;
    InvalidateRect(m_hwnd, &labelArea, 0);
    m_list->Refresh();
    if (tab == kTabTroops && static_cast<uint32_t>(UiGetHumanSide()) == system->ControlSide()) {
        const ResourceStringId required = StringId(0x1947);
        String text(&required);
        text.AppendSigned(static_cast<System*>(system)->m_troopRegRequired);
        m_infoLabel->SetText(text.m_text);
        const RECT area = m_infoLabel->m_rect;
        InvalidateRect(m_hwnd, &area, 0);
    } else {
        m_infoLabel->SetText(g_spaceString);
        const RECT area = m_infoLabel->m_rect;
        InvalidateRect(m_hwnd, &area, 0);
    }
    m_tab = tab;
    return 1;
}

void SystemForcesWindow::LoadHeaderBitmaps() {
    ObjectKey unused;
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    GameObjNotifier* system = FUN_004a25c0(this);
    if (!system) return;
    DeleteBitmap(m_headerActive);
    DeleteBitmap(m_headerInactive);
    DeleteBitmap(m_rowOverlay);
    DeleteBitmap(m_unused178);
    switch (m_side) {
    case 1:
        m_headerActive = LoadBitmap(art, 0x283b, kHeaderKind);
        m_headerInactive = LoadBitmap(art, 0x27d8, kHeaderKind);
        m_rowOverlay = LoadBitmap(art, 0x2952, kHeaderKind);
        break;
    case 2:
        m_headerActive = LoadBitmap(art, 0x27d9, kHeaderKind);
        m_headerInactive = LoadBitmap(art, 0x283e, kHeaderKind);
        m_rowOverlay = LoadBitmap(art, 0x2953, kHeaderKind);
        break;
    case 3:
        m_headerActive = LoadBitmap(art, 0x283f, kHeaderKind);
        m_headerInactive = LoadBitmap(art, 0x2840, kHeaderKind);
        m_rowOverlay = LoadBitmap(art, 0x2953, kHeaderKind);
        break;
    default:
        return;
    }
    if (m_tabs) {
        GameObj* parent = static_cast<GameObj*>(system);
        int count = CountDefenseFacilities22(parent, m_side, 3) + CountDefenseFacilities23(parent, m_side, 3);
        CoolStrobeButton* tab = static_cast<CoolStrobeButton*>(m_tabs->m_children.FindById(kTabWeapons));
        tab->LoadStateBitmap(0, PictureIfAny(count, 0x2938));
        count = CountDefenseFacilities25(parent, m_side, 3) + CountDefenseFacilities24(parent, m_side, 3);
        tab = static_cast<CoolStrobeButton*>(m_tabs->m_children.FindById(kTabShields));
        tab->LoadStateBitmap(0, PictureIfAny(count, 0x293b));
        count = CountFightSquads(parent, m_side, 3);
        tab = static_cast<CoolStrobeButton*>(m_tabs->m_children.FindById(kTabFighters));
        uint32_t picture;
        uint32_t alternate;
        if (m_side == 1) {
            picture = PictureIfAny(count, 0x293e);
            alternate = 0x293d;
        } else if (m_side == 2) {
            picture = PictureIfAny(count, 0x2941);
            alternate = 0x2940;
        } else {
            picture = 0x2942;
            alternate = 0x2942;
        }
        tab->LoadStateBitmap(1, alternate);
        tab->LoadStateBitmap(4, alternate);
        tab->LoadStateBitmap(0, picture);
        count = CountTroopRegs(parent, m_side, 3);
        tab = static_cast<CoolStrobeButton*>(m_tabs->m_children.FindById(kTabTroops));
        if (m_side == 1) {
            picture = PictureIfAny(count, 0x2945);
            alternate = 0x2944;
        } else if (m_side == 2) {
            picture = PictureIfAny(count, 0x2948);
            alternate = 0x2947;
        } else {
            picture = 0x2949;
            alternate = 0x2949;
        }
        tab->LoadStateBitmap(1, alternate);
        tab->LoadStateBitmap(4, alternate);
        tab->LoadStateBitmap(0, picture);
        count = 0;
        RoleIterator roles(system, 3, static_cast<uint32_t>(m_side));
        for (roles.Begin(); roles.Current(); roles.Advance()) {
            if (NotOnVisibleMission(static_cast<Role*>(roles.Current()))) count++;
        }
        tab = static_cast<CoolStrobeButton*>(m_tabs->m_children.FindById(kTabPersonnel));
        if (m_side == 1) {
            alternate = 0x294b;
            picture = PictureIfAny(count, 0x294c);
        } else if (m_side == 2) {
            alternate = 0x294e;
            picture = PictureIfAny(count, 0x294f);
        } else {
            picture = 0x2950;
            alternate = 0x2950;
        }
        tab->LoadStateBitmap(1, alternate);
        tab->LoadStateBitmap(4, alternate);
        tab->LoadStateBitmap(0, picture);
    }
    if (m_list) {
        const COLORREF color = m_side == 1 ? 0x20000ff : (m_side == 2 ? 0x200ff00 : 0x2ffff00);
        m_list->SetSelectedTextColor(color);
    }
}
