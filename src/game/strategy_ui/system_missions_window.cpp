#include "strategy_ui/system_missions_window.h"

#include <new>

#include "app/rebapp_globals.h"
#include "audio/sound_manager.h"
#include "character/role.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/stream_io.h"
#include "core/vptr_store.h"
#include "data/dat_record.h"
#include "data/int_pair_node.h"
#include "galaxy/system.h"
#include "gameobj/child_iterator.h"
#include "gameobj/key_int_set.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "mission/mission.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/sector_window.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_tab_control.h"
#include "ui/object_list_item.h"
#include "ui/resource_dll.h"

extern const char g_gokresDllName[];  // 0x6a8708 "gokres.dll"

DatRecord* FUN_0051cab0(const ObjectKey* key);  // the data record of a key (via the object-type database)

namespace {

constexpr uint32_t kStringSlot = 2;
constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kGokresSlot = 10;
constexpr uint32_t kPictureKind = 2;
constexpr uint32_t kHeaderKind = 10;
constexpr int16_t kStringGroup = 2;
constexpr int kShow = 5;
constexpr uint32_t kHiddenStyle = 0x10000000;
constexpr uint32_t kOpenSound = 0x25e;
constexpr uint32_t kCommandMessage = 0x111;
constexpr uint32_t kRedrawHeaderMessage = 0x467;
constexpr uint32_t kRedrawHeaderCommand = 0x466;

constexpr uint32_t kCloseId = 0x14;
constexpr uint32_t kSideId = 0x15;
constexpr uint32_t kTabsId = 0x16;
constexpr uint32_t kAgentsTabId = 0x17;
constexpr uint32_t kDecoysTabId = 0x18;
constexpr uint32_t kMissionListId = 10;
constexpr uint32_t kCharacterListId = 0xb;
constexpr uint32_t kSectorId = 0xca;
constexpr uint32_t kBackgroundOverlayId = 0xf;

constexpr uint32_t kAgentsFamilyFirst = 0x30;
constexpr uint32_t kCharacterFamilyEnd = 0x40;
constexpr uint32_t kSystemFamilyFirst = 0x90;
constexpr uint32_t kSystemFamilyEnd = 0x98;

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

bool FamilyIn(const ObjectKey& key, uint32_t first, uint32_t end) { return key.Family() >= first && key.Family() < end; }

int ViewSideOf(int side) { return side == 1 ? 1 : (side == 2 ? 2 : 0); }

DibBitmap* LoadBitmap(HMODULE module, uint32_t id, uint32_t kind) { return NewOrNull<DibBitmap>(module, id, kind); }

CoolStrobeButton* NewButton(HMODULE art, int x, int y, int width, int height, CoolWindow* parent, uint32_t id,
                            uint32_t normal, uint32_t pressed, DibBitmap* backdrop, int repeatRate, int pixelHit) {
    return NewOrNull<CoolStrobeButton>(art, x, y, width, height, parent, id, normal, pressed, backdrop, repeatRate, pixelHit);
}

Role* AsRole(GameObjNotifier* object) { return static_cast<Role*>(object); }

uint32_t ControlSideOf(GameObjNotifier* object) { return object->ControlSide(); }

bool CountsAsMission(Role* role) {
    ObjectKey missionKey;
    role->GetMissionKey(&missionKey);
    return missionKey.IsSet() != 0 && (role->m_flags78 & role_flag::kOnHiddenMission) == 0;
}

CoolListItem* FirstItem(CoolListItemList* list) { return list->First(); }

}  // namespace

// ---- Construction ---------------------------------------------------------------------------------------------

SystemMissionsWindow::SystemMissionsWindow(HINSTANCE instance, int x, int y, CoolWindow* parent, uint32_t childId,
                                           GameObjNotifier* system)
    : CustomDialogBox(instance, x, y, 0xeb, 0x130, parent, childId, nullptr),
      m_titleLabel(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u) {
    CoolGameWindow::m_flags |= kGameWindowDraggable;
    m_stringSlot = kStringSlot;
    m_bitmapSlot = kBitmapSlot;
    m_galaxy = static_cast<GalaxyWindow*>(parent);
    ObjectKey key;
    if (system) system->GetKey(&key);
    m_systemKey = key;
    m_closeButton = nullptr;
    m_sideButton = nullptr;
    m_sectorButton = nullptr;
    m_tab = 0;
    m_missions = nullptr;
    m_missionList = nullptr;
    m_characterList = nullptr;
    m_characters = nullptr;
    m_objectivePicture = nullptr;
    m_titleLeft = 0;
    m_objectiveLabel = nullptr;
    m_captionLabel = nullptr;
    m_headerInactive = nullptr;
    m_headerActive = nullptr;
    m_selectedMission = nullptr;
    m_gokres = FindResourceModule(kGokresSlot);
    if (!m_gokres) {
        char* dllLocation = GetDllLocation();
        m_gokres = LoadResourceModule(dllLocation, g_gokresDllName, kGokresSlot);
        if (dllLocation) operator delete(dllLocation);
    }
}

SystemMissionsWindow::~SystemMissionsWindow() {
    REB_KEEP_VPTR_STORE();
    delete m_closeButton;
    delete m_sideButton;
    delete m_sectorButton;
    delete m_missions;
    delete m_missionList;
    delete m_characterList;
    delete m_characters;
    DeleteBitmap(m_objectivePicture);
    DeleteLabel(m_objectiveLabel);
    DeleteLabel(m_captionLabel);
    DeleteBitmap(m_headerInactive);
    DeleteBitmap(m_headerActive);
}

void SystemMissionsWindow::OnCreate1() {
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    GameObjNotifier* system = GetSystem();
    if (!system) return;

    SetDragLimits(m_galaxy->m_panelArea, nullptr);
    SetDragMode(kDragOutline);
    SetPaintBackground(LoadBitmap(art, 0x2b9d, kPictureKind), kBackgroundOverlayId);
    m_headerInactive = nullptr;
    m_headerActive = nullptr;
    LoadHeaderBitmaps();
    m_titleLeft = 2;

    if (DibBitmap* shape = LoadBitmap(art, 0x27e1, kPictureKind)) {
        m_sectorButton = NewButton(art, 3, 3, shape->Width(), shape->Height(), this, kSectorId, 0x27e1, 0x27e0,
                                   GetPaintBackgroundIfOwned(), 0, 1);
        if (m_sectorButton) {
            m_sectorButton->Create(0);
            m_titleLeft = shape->Width() + 5;
        }
        DeleteBitmap(shape);
    }
    if (DibBitmap* shape = LoadBitmap(art, 0x277c, kPictureKind)) {
        m_closeButton = NewButton(art, m_width - shape->Width() - 3, 3, shape->Width(), shape->Height(), this, kCloseId,
                                  0x277c, 0x277d, GetPaintBackgroundIfOwned(), 0, 1);
        m_closeButton->Create(0);
        DeleteBitmap(shape);
    }
    if (DibBitmap* shape = LoadBitmap(art, 0x280d, kPictureKind)) {
        m_sideButton = NewButton(art, m_closeButton->m_x - shape->Width(), 3, shape->Width(), shape->Height(), this,
                                 kSideId, 0x280d, 0x280e, GetPaintBackgroundIfOwned(), 0, 1);
        m_sideButton->Create(0);
        DeleteBitmap(shape);
    }

    m_missions = NewOrNull<CoolListItemList>(4);
    m_missionList = NewOrNull<CoolDragList>(art, 5, 0x18, 0x5e, 0x113, this, GetPaintBackgroundIfOwned(), kMissionListId,
                                            0x5a, 0x32, 0, 0, 0, 0, 10, 1, nullptr);
    if (m_missionList) {
        m_missionList->m_style &= ~kHiddenStyle;
        m_missionList->Create(0);
        m_missionList->m_options.m_value |= 0x20000;
        m_missionList->CreateVerticalScrollBar(m_instance, m_missionList, 7, 0x29fc, m_missionList->m_clientHeight, 0,
                                               m_missionList->m_clientHeight, 0, nullptr);
        m_missionList->SetTextColor(0xffffff);
        m_missionList->m_textFormat = 0x10;
        m_missionList->SetFont(10);
        m_missionList->SetItemList(m_missions);
    }

    m_characters = NewOrNull<CoolListItemList>(4);
    m_characterList = NewOrNull<CoolDragList>(art, 0x6b, 0x91, 0x75, 0x93, this, GetPaintBackgroundIfOwned(),
                                              kCharacterListId, 0x73, 0x2b, 0, 0, 0, 0, 10, 1, nullptr);
    if (m_characterList) {
        m_characterList->m_style &= ~kHiddenStyle;
        m_characterList->Create(0);
        m_characterList->m_options.m_value |= 0xa0000;
        m_characterList->CreateVerticalScrollBar(m_instance, m_characterList, 7, 0x29fc, m_characterList->m_clientHeight, 0,
                                                 m_characterList->m_clientHeight, 0, nullptr);
        m_characterList->SetTextColor(0xffffff);
        m_characterList->m_textFormat = 1;
        m_characterList->SetItemList(m_characters);
    }

    m_tabs = NewOrNull<CoolTabControl>(m_instance, 0x69, 0x7f, 0x7a, 0x10, kTabsId, this, GetPaintBackgroundIfOwned());
    if (m_tabs) {
        m_tabs->Create(0);
        m_children.Insert(m_tabs);
        CoolStrobeButton* agents = NewButton(art, 0, 0, 0x3d, 0x2b, m_tabs, kAgentsTabId, 0x2d28, 0x2d29,
                                             GetPaintBackgroundIfOwned(), 0, 0);
        agents->Create(0);
        const ResourceStringId agentsTip = StringId(0x8500);
        AddToolFromString(agents, &agentsTip);
        m_tabs->AddTab(agents, 1, 0);
        CoolStrobeButton* decoys = NewButton(art, 0x3d, 0, 0x3d, 0x2b, m_tabs, kDecoysTabId, 0x2d2a, 0x2d2b,
                                             GetPaintBackgroundIfOwned(), 0, 0);
        decoys->Create(0);
        const ResourceStringId decoysTip = StringId(0x8501);
        AddToolFromString(decoys, &decoysTip);
        m_tabs->AddTab(decoys, 0, 1);
    }

    m_objectiveLabel = NewOrNull<CoolLabel>(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    m_objectiveLabel->MoveTo(0x6d, 0x5b);
    m_objectiveLabel->SetColor(0xffffff, nullptr);
    m_objectiveLabel->SetFont(5);
    m_objectiveLabel->m_width = 0x71;
    m_objectiveLabel->m_height = 0x20;
    m_objectiveLabel->m_drawFlags = 0x11;
    m_objectiveLabel->m_layoutFlags = kLabelFixedWidth | kLabelFixedHeight;

    m_captionLabel = NewOrNull<CoolLabel>(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    m_captionLabel->MoveTo(0x6d, 0x19);
    m_captionLabel->SetColor(0xffffff, nullptr);
    m_captionLabel->SetFont(5);
    m_captionLabel->m_width = 0x71;
    m_captionLabel->m_height = 0x10;
    m_captionLabel->m_drawFlags = 1;
    m_captionLabel->m_layoutFlags = kLabelFixedWidth | kLabelFixedHeight;

    m_titleLabel.SetFont(5);
    m_titleLabel.SetColor(0, nullptr);
    m_titleLabel.m_width = 0x7a;
    m_titleLabel.m_layoutFlags = kLabelFixedWidth;

    ObjectKey systemKey;
    system->GetKey(&systemKey);
    m_systemKey = systemKey;
    m_objects.Add(NewOrNull<ObjectKeyIdNode>(&m_systemKey, 0u), 0);

    if (FillMissionList() != 0) {
        m_missionList->Show(kShow);
        // BUG(original): crashes when no mission row could be made (the record of a mission may be missing).
        CoolListItem* first = m_missions->First();
        m_missionList->SelectById(first->m_id, 1);
    }
    SetFocus(m_missionList->m_hwnd);
    if ((m_galaxy->m_mouseMode & 4) == 0) {
        SoundRequest request(0, 0, 0, 0, 0, 0);
        request.m_kind = 1;
        request.m_soundId = kOpenSound;
        request.m_moduleIndex = kBitmapSlot;
        SoundManager::Play(&request);
    }
}

// ---- Messages -------------------------------------------------------------------------------------------------

LRESULT __stdcall SystemMissionsWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message != WM_LBUTTONDOWN && message != WM_RBUTTONDOWN) {
        if (message != kRedrawHeaderMessage) return CustomDialogBox::WindowProc(message, wParam, lParam);
        InvalidateRect(m_hwnd, &m_titleRect, 0);
        return 0;
    }
    PostMessageA(m_parent->m_hwnd, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
    return CustomDialogBox::WindowProc(message, wParam, lParam);
}

void SystemMissionsWindow::OnCommand(int, int command, int) {
    HWND galaxyWindow = m_galaxy->m_hwnd;
    const uint32_t code = static_cast<uint32_t>(command) >> 16;
    switch (static_cast<uint32_t>(command) & 0xffff) {
    case kMissionListId: {
        CoolListItemList unusedSelection(1);
        if (code == kDragListSelectionChanged) SelectMission(GetSelectedMission(), 0);
        PostMessageA(m_galaxy->m_hwnd, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
        return;
    }
    case kCharacterListId:
        SetFocus(m_missionList->m_hwnd);
        PostMessageA(m_galaxy->m_hwnd, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
        return;
    case kCloseId:
        PostCloseRequest();
        return;
    case kSideId:
        PostMessageA(galaxyWindow, kCommandMessage, (m_childId & 0xffff) | (kRedrawHeaderCommand << 16), 0);
        return;
    case kTabsId:
        SendMessageA(galaxyWindow, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
        FillCharacterList(static_cast<int>(code), 0);
        SetFocus(m_characterList->m_hwnd);
        return;
    case kSectorId: {
        GameObjNotifier* system = GetSystem();
        if (system && m_galaxy) {
            ObjectKey sectorKey;
            if (static_cast<GameObj*>(system)->GetValidParentKey(&sectorKey) != 0) m_galaxy->OpenSectorWindow(&sectorKey);
        }
        return;
    }
    default:
        return;
    }
}

void SystemMissionsWindow::OnKeyDown(WPARAM key, LPARAM) {
    if (key == VK_ESCAPE) PostCloseRequest();
}

void SystemMissionsWindow::SaveState(ostream* out) {
    ObjectKey key = m_systemKey;
    uint32_t childId = m_childId;
    key.Write(out);
    WriteValue32(out, &childId);
}

void SystemMissionsWindow::LoadState(istream* in) {
    ObjectKey key;
    key.Read(in);
    m_systemKey = key;
    uint32_t childId;
    ReadValue32(in, &childId);
    m_childId = childId;
}

void SystemMissionsWindow::GetSelection(ObjectKeyValueSet* out, CoolWindow* source) {
    if (!source) return;
    CoolListItemList selection(1);
    CoolListItemList* items = nullptr;
    if (source->m_childId == m_characterList->m_childId && IsWindowVisible(m_characterList->m_hwnd) != 0) {
        m_characterList->CopySelectionTo(&selection);
        items = m_characters;
    } else if (source->m_childId == m_missionList->m_childId) {
        m_missionList->CopySelectionTo(&selection);
        items = m_missions;
    }
    for (CoolListItem* picked = static_cast<CoolListItem*>(selection.m_head); picked; picked = picked->Next()) {
        ObjectListItem* row = static_cast<ObjectListItem*>(items->FindById(picked->m_id));
        if (!row) continue;
        ObjectKey key;
        row->GetKey(&key);
        out->Add(NewOrNull<ObjectKeyValueNode>(key, 0));
    }
}

void SystemMissionsWindow::OnChildAdded(const ObjectKey* key, GameObjNotifier* object) {
    if (!object) object = GetObjectInFamilies_01_ff(m_galaxy->m_side, key);
    if (!object) return;
    if (!FamilyIn(*key, kAgentsFamilyFirst, kCharacterFamilyEnd)) return;
    Role* role = AsRole(object);
    ObjectKey missionKey;
    role->GetMissionKey(&missionKey);
    if (!missionKey.IsSet() || (role->m_flags78 & role_flag::kOnHiddenMission) != 0) return;
    CoolListItem* row = m_missions->FindById(missionKey.Id());
    if (!row) {
        FillMissionList();
        ObjectKey refreshedKey;
        role->GetMissionKey(&refreshedKey);
        row = m_missions->FindById(refreshedKey.Id());
        if (!row) return;
    }
    if ((row->m_state.m_value & kListItemSelected) != 0) SelectMission(static_cast<ObjectListItem*>(row), 1);
}

void SystemMissionsWindow::OnChildMoved(const ObjectKey* key, GameObjNotifier* object, const ObjectKey* otherKey,
                                        GameObjNotifier*) {
    // BUG(original): the object found here is never used.
    if (!object) GetObjectInFamilies_01_ff(m_galaxy->m_side, key);
    GameObjNotifier* system = GetSystem();
    if (!system) return;
    if (!FamilyIn(*key, kAgentsFamilyFirst, kCharacterFamilyEnd)) return;
    if (!otherKey) return;
    if (otherKey->Family() < 0x40 || otherKey->Family() >= 0x80) return;
    CoolListItem* row = m_missions->FindById(otherKey->Id());
    if (!row) return;
    if ((row->m_state.m_value & kListItemSelected) != 0) UpdateTabs();
    RoleIterator roles(system, 3);
    roles.Begin();
    while (roles.Current()) {
        ObjectKey missionKey;
        AsRole(roles.Current())->GetMissionKey(&missionKey);
        if (missionKey.m_value == otherKey->m_value) return;
        roles.Advance();
    }
    FillMissionList();
}

void SystemMissionsWindow::OnObjectUpdated(const ObjectKey* key, GameObjNotifier* object) {
    if (!object) object = GetObjectInFamilies_01_ff(m_galaxy->m_side, key);
    if (!object) return;
    if (!FamilyIn(*key, kAgentsFamilyFirst, kCharacterFamilyEnd)) return;
    Role* role = AsRole(object);
    ObjectKey missionKey;
    role->GetMissionKey(&missionKey);
    if (!missionKey.IsSet() || (role->m_flags78 & role_flag::kOnHiddenMission) != 0) return;
    CoolListItem* row = m_characters->FindById(role->m_id);
    if (!row) return;
    row->SetImages(CreateObjectPicture(m_gokres, key, object, 0, 1), nullptr);
    row->m_text = static_cast<GameObj*>(object)->GetName()->m_text;
    m_characterList->Refresh();
    UpdateTabs();
}

ObjectKey SystemMissionsWindow::GetKeyAt(int x, int y) {
    ObjectKey result = m_systemKey;
    if (m_objectivePicture) {
        const int bottom = (0x32 - m_objectivePicture->Height()) / 2 + 0x25;
        const int right = (0x7a - m_objectivePicture->Width()) / 2 + 0x6c;
        RECT area;
        SetRect(&area, 0x6c, 0x25, right, bottom);
        POINT point = {x, y};
        if (PtInRect(&area, point) != 0) result = m_objectiveKey;
    }
    if (m_characterList) {
        POINTS inList = {static_cast<SHORT>(x - 0x6b), static_cast<SHORT>(y - 0x91)};
        CoolListItem* hit = m_characterList->HitTest(m_characterList->ClientToList(inList));
        if (hit) {
            ObjectKey itemKey;
            static_cast<ObjectListItem*>(hit)->GetKey(&itemKey);
            result = itemKey;
        }
    }
    return result;
}

void SystemMissionsWindow::ShowObjectPage(const ObjectKey* key) {
    if (FamilyIn(*key, kAgentsFamilyFirst, kCharacterFamilyEnd)) {
        GameObjNotifier* character = GetObjectInFamilies_30_40(m_galaxy->m_side, key);
        if (character) {
            FillCharacterList((AsRole(character)->m_flags78 & role_flag::kDecoy) == 0 ? kAgentsTabId : kDecoysTabId, 1);
            ObjectKey characterKey;
            character->GetKey(&characterKey);
            if (CoolListItem* row = m_characterList->FindItem(characterKey.Id())) {
                row->m_state.m_value |= kListItemSelected;
                m_characterList->Refresh();
            }
        }
    }
    if (key->Family() >= 0x50 && key->Family() < 0x80) {
        if (CoolListItem* row = m_missions->FindById(key->Id())) SelectMission(static_cast<ObjectListItem*>(row), 1);
    }
}

ObjectKey SystemMissionsWindow::GetDropKeyAt(int, int) { return m_systemKey; }

int SystemMissionsWindow::Virtual29() {
    GameObjNotifier* system = GetSystem();
    if (!system) return 0;
    switch (DominantMissionSide(system, m_galaxy->m_side)) {
    case 1: return 0x2d13;
    case 2: return 0x2d14;
    default: return 0x2d15;
    }
}

// ---- Drawing --------------------------------------------------------------------------------------------------

void SystemMissionsWindow::DrawOverlay(HDC dc) {
    String title;
    GameObjNotifier* system = GetSystem();
    if (!system) return;
    if (m_objectivePicture) {
        const int width = m_objectivePicture->Width();
        const int height = m_objectivePicture->Height();
        m_objectivePicture->DrawToDc(dc, 0x40, 0xcc0020, (0x7a - width) / 2 + 0x6c, (0x32 - height) / 2 + 0x25, 0, 0, 0, 0);
    }
    m_objectiveLabel->Draw(dc);
    m_captionLabel->Draw(dc);
    DibBitmap* header = m_galaxy->m_activeChild == m_hwnd ? m_headerActive : m_headerInactive;
    header->DrawToDc(dc, 0x40, 0xcc0020, m_titleRect.left, m_titleRect.top, 0, 0, 0, 0);
    const ResourceStringId prefix = StringId(0x8525);
    title.SetFromResource(&prefix);
    {
        String name(static_cast<GameObj*>(system)->GetName()->m_text);
        title += name;
    }
    m_titleLabel.m_height = 0x10;
    m_titleLabel.m_width = m_titleLeft * -3 + 0xeb;
    m_titleLabel.MoveTo(m_titleLeft + m_titleRect.left, m_titleRect.top);
    m_titleLabel.SetText(title.m_text);
    m_titleLabel.m_drawFlags = 0x24;
    m_titleLabel.m_layoutFlags = kLabelFixedWidth | kLabelFixedHeight;
    m_titleLabel.SetFont(5);
    m_titleLabel.Draw(dc);
}

void SystemMissionsWindow::LoadHeaderBitmaps() {
    GameObjNotifier* system = GetSystem();
    if (!system) return;
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    DeleteBitmap(m_headerInactive);
    DeleteBitmap(m_headerActive);
    m_headerInactive = nullptr;
    m_headerActive = nullptr;
    switch (DominantMissionSide(system, m_galaxy->m_side)) {
    case 1:
        m_headerActive = LoadBitmap(art, 0x283b, kHeaderKind);
        m_headerInactive = LoadBitmap(art, 0x27d8, kHeaderKind);
        break;
    case 2:
        m_headerActive = LoadBitmap(art, 0x27d9, kHeaderKind);
        m_headerInactive = LoadBitmap(art, 0x283e, kHeaderKind);
        break;
    default:
        m_headerActive = LoadBitmap(art, 0x283f, kHeaderKind);
        m_headerInactive = LoadBitmap(art, 0x2840, kHeaderKind);
        break;
    }
    const int bottom = m_headerActive->Height() + 2;
    const int right = m_headerInactive->Width() + 2;
    SetRect(&m_titleRect, 2, 2, right, bottom);
    InvalidateRect(m_hwnd, &m_titleRect, 0);
}

// ---- The lists ------------------------------------------------------------------------------------------------

GameObjNotifier* SystemMissionsWindow::GetSystem() {
    GameObjNotifier* system = GetObjectInFamilies_90_98(AppGetSide(), &m_systemKey);
    if (!system) PostCloseRequest();
    return system;
}

ObjectListItem* SystemMissionsWindow::GetSelectedMission() {
    CoolListItemList selection(1);
    ObjectListItem* row = nullptr;
    m_missionList->CopySelectionTo(&selection);
    if (selection.m_head) {
        row = static_cast<ObjectListItem*>(m_missions->FindById(static_cast<CoolListItem*>(selection.m_head)->m_id));
    }
    return row;
}

uint32_t SystemMissionsWindow::SelectedMissionSide() {
    GameObjNotifier* system = GetSystem();
    uint32_t side = 0;
    if (!system) return 0;
    if (m_selectedMission) {
        RoleIterator roles(system, 3);
        roles.Begin();
        while (roles.Current()) {
            ObjectKey selectedKey;
            m_selectedMission->GetKey(&selectedKey);
            ObjectKey missionKey;
            AsRole(roles.Current())->GetMissionKey(&missionKey);
            if (missionKey.m_value == selectedKey.m_value) break;
            roles.Advance();
        }
        if (roles.Current()) side = ControlSideOf(roles.Current());
    }
    return side;
}

void SystemMissionsWindow::ClearSelectedMission(ObjectListItem* row) {
    const uint32_t id = row ? row->m_id : 0;
    const uint32_t selectedId = m_selectedMission ? m_selectedMission->m_id : 0;
    if (id == selectedId) {
        m_selectedMission = nullptr;
        UpdateTabs();
    }
}

void SystemMissionsWindow::SelectMission(ObjectListItem* row, int force) {
    const uint32_t id = row ? row->m_id : 0;
    const uint32_t selectedId = m_selectedMission ? m_selectedMission->m_id : 0;
    if (id != selectedId || force != 0) {
        m_selectedMission = row;
        UpdateTabs();
    }
}

void SystemMissionsWindow::UpdateTabs() {
    int side = ViewSideOf(m_galaxy->m_side);
    if (m_selectedMission) {
        const int missionSide = static_cast<int>(SelectedMissionSide());
        if (missionSide != 0) side = missionSide;
        uint32_t agentsNormal = 0, agentsPressed = 0, decoysNormal = 0, decoysPressed = 0;
        if (side == 1) {
            agentsNormal = 0x2d28;
            agentsPressed = 0x2d29;
            decoysNormal = 0x2d2a;
            decoysPressed = 0x2d2b;
        } else if (side == 2) {
            agentsNormal = 0x2d2d;
            agentsPressed = 0x2d2e;
            decoysNormal = 0x2d2f;
            decoysPressed = 0x2d30;
        }
        CoolStrobeButton* agents = static_cast<CoolStrobeButton*>(m_tabs->m_children.FindById(kAgentsTabId));
        agents->LoadStateBitmap(0, agentsNormal);
        agents->LoadStateBitmap(1, agentsPressed);
        CoolStrobeButton* decoys = static_cast<CoolStrobeButton*>(m_tabs->m_children.FindById(kDecoysTabId));
        decoys->LoadStateBitmap(0, decoysNormal);
        decoys->LoadStateBitmap(1, decoysPressed);
        m_tabs->ReloadSelectedBitmaps();
        m_characterList->Show(kShow);
        m_tabs->Show(kShow);
        InvalidateRect(m_tabs->m_hwnd, nullptr, 0);
        FillCharacterList(kAgentsTabId, 1);
        UpdateObjective();
        return;
    }
    m_characterList->Hide();
    m_tabs->Hide();
    UpdateObjective();
}

void SystemMissionsWindow::AddCharacterRow(GameObjNotifier* object, int normal, String* heading) {
    if (!object) {
        ObjectKey blank;
        ObjectListItem* item = NewOrNull<ObjectListItem>(&blank);
        if (heading != nullptr) {
            item->m_text = heading->m_text;
            item->m_textFormat = 0x25;
            item->m_fontSlot = 10;
            item->m_textOffsetY = 10;
        }
        m_characters->Insert(item);
        return;
    }
    ObjectKey key;
    object->GetKey(&key);
    DibBitmap* picture = CreateObjectPicture(m_gokres, &key, object, normal == 0, 1);
    DibBitmap* image = NewOrNull<DibBitmap>(0x7a, 0x2b, picture);
    const int pictureWidth = picture->Width();
    const int imageWidth = image->Width();
    const int pictureHeight = picture->Height();
    const int spare = image->Height() - pictureHeight;
    image->FillRect(0, 0, image->Width(), image->Height(), picture->m_colorKey);
    picture->Blit(image, (imageWidth - pictureWidth) / 2, spare / 2, 0, 0, 0, 0);
    ObjectKey itemKey;
    object->GetKey(&itemKey);
    ObjectListItem* item = NewOrNull<ObjectListItem>(&itemKey);
    item->SetImages(image, nullptr);
    item->m_textOffsetY = picture->Height() + spare / 2;
    item->m_state.m_value |= kListItemDisabled;
    item->m_text = static_cast<GameObj*>(object)->GetName()->m_text;
    item->m_fontSlot = 10;
    item->m_textFormat = 0x21;
    m_characterList->m_fillColor = picture->m_colorKey;
    m_characters->Insert(item);
    DeleteBitmap(picture);
}

void SystemMissionsWindow::UpdateObjective() {
    GameObjNotifier* system = GetSystem();
    if (!system) return;
    if (m_objectiveKey.IsSet() != 0) {
        ObjectKey systemKey;
        system->GetKey(&systemKey);
        if (m_objectiveKey.m_value != systemKey.m_value) {
            if (m_objects.Find(&m_objectiveKey)) m_objects.RemoveKey(&m_objectiveKey);
        }
    }
    m_objectiveKey.Reset();
    if (m_objectivePicture) {
        DeleteBitmap(m_objectivePicture);
        m_objectivePicture = nullptr;
    }
    if (!m_selectedMission) {
        m_objectiveLabel->m_hidden = 1;
        m_captionLabel->m_hidden = 1;
    } else {
        const int viewSide = ViewSideOf(m_galaxy->m_side);
        if (static_cast<int>(SelectedMissionSide()) == viewSide) {
            ObjectKey selectedKey;
            m_selectedMission->GetKey(&selectedKey);
            if (GameObjNotifier* mission = GetObjectInFamilies_40_80(m_galaxy->m_side, &selectedKey))
                m_objectiveKey = static_cast<Mission*>(mission)->m_objectiveKey;
        } else {
            ObjectKey selectedKey;
            m_selectedMission->GetKey(&selectedKey);
            if (FamilyIn(selectedKey, 0x50, 0x60)) {
                ObjectKey systemKey;
                system->GetKey(&systemKey);
                m_objectiveKey = systemKey;
            }
        }
        GameObjNotifier* objective = nullptr;
        if (m_objectiveKey.OrNull()) {
            m_objects.Add(NewOrNull<ObjectKeyIdNode>(&m_objectiveKey, 0u), 0);
            objective = GetObjectInFamilies_01_ff(m_galaxy->m_side, &m_objectiveKey);
            if (objective) {
                DibBitmap* picture;
                if (FamilyIn(m_objectiveKey, kSystemFamilyFirst, kSystemFamilyEnd)) {
                    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
                    const uint32_t pictureId = SectorSystemPictureId(static_cast<System*>(objective)->GetRecordField44());
                    picture = LoadBitmap(art, pictureId, kPictureKind);
                } else {
                    const bool character = FamilyIn(m_objectiveKey, kAgentsFamilyFirst, kCharacterFamilyEnd);
                    picture = CreateObjectPicture(m_gokres, &m_objectiveKey, objective, !character, 1);
                }
                if (picture) {
                    DibBitmap* background = GetPaintBackgroundIfOwned();
                    m_objectivePicture = NewOrNull<DibBitmap>(picture->Width(), picture->Height(), background);
                    const int width = picture->Width();
                    const int height = picture->Height();
                    background->Blit(m_objectivePicture, 0, 0, (0x7a - width) / 2 + 0x6c, (0x32 - height) / 2 + 0x25, 0, 0);
                    picture->DrawKeyed(m_objectivePicture, 0, 0);
                    DeleteBitmap(picture);
                }
            }
        }
        m_objectiveLabel->m_hidden = 0;
        m_captionLabel->m_hidden = 0;
        if (!objective) {
            const ResourceStringId unknown = StringId(0x8527);
            m_objectiveLabel->SetTextFromResource(&unknown);
        } else {
            m_objectiveLabel->SetText(static_cast<GameObj*>(objective)->GetName()->m_text);
        }
        const ResourceStringId caption = StringId(0x8521);
        m_captionLabel->SetTextFromResource(&caption);
        m_captionLabel->SetColor(0xffffff, nullptr);
    }
    RECT area;
    SetRect(&area, 0x6c, 0x25, 0xe6, 0x57);
    InvalidateRect(m_hwnd, &area, 0);
    m_captionLabel->Invalidate(m_hwnd, 0);
    m_objectiveLabel->Invalidate(m_hwnd, 0);
}

int SystemMissionsWindow::FillMissionList() {
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    GameObjNotifier* system = GetSystem();
    if (!system) return 0;
    DibBitmap sideMark(art, static_cast<uint32_t>(m_galaxy->m_side != 1) + 0x2b77, kPictureKind);
    int processed = 0;
    uint8_t colorKey = 0;
    ObjectKeyIntSet seeds;
    ObjectKeyValueSet sides;
    RoleIterator roles(system, 3);
    for (roles.Begin(); roles.Current(); roles.Advance()) {
        Role* role = AsRole(roles.Current());
        if (!CountsAsMission(role)) continue;
        ObjectKey missionKey;
        role->GetMissionKey(&missionKey);
        if (seeds.Find(&missionKey)) continue;
        ObjectKey seedKey;
        role->GetMissionSeedKey(&seedKey);
        ObjectKey key;
        role->GetMissionKey(&key);
        const int seed = static_cast<int>(seedKey.m_value);
        seeds.AddPair(key, seed);
        ObjectKey sideKey;
        role->GetMissionKey(&sideKey);
        sides.AddKeyValue(&sideKey, static_cast<int>(ControlSideOf(role)));
    }

    ObjectListItem* row = static_cast<ObjectListItem*>(m_missions->First());
    while (row) {
        ObjectListItem* next = static_cast<ObjectListItem*>(row->Next());
        ObjectKey rowKey;
        row->GetKey(&rowKey);
        if (!seeds.Find(&rowKey)) {
            ClearSelectedMission(row);
            m_missions->Remove(row);
            delete row;
            m_missionList->Refresh();
        }
        row = next;
    }

    IntPairNode* seedNode = static_cast<IntPairNode*>(seeds.First());
    ObjectKeyValueNode* sideNode = static_cast<ObjectKeyValueNode*>(sides.First());
    while (seedNode && sideNode) {
        processed = 1;
        ObjectKey missionKey;
        seedNode->GetKey(&missionKey);
        const int side = sideNode->m_value;
        ObjectKey seedKey;
        seedKey.m_value = static_cast<uint32_t>(seedNode->m_value);
        DatRecord* record = FUN_0051cab0(&seedKey);
        if (record && !m_missions->FindById(missionKey.Id())) {
            m_objects.Add(NewOrNull<ObjectKeyIdNode>(&missionKey, 0u), m_missionList->m_childId);
            ObjectListItem* item = NewOrNull<ObjectListItem>(&missionKey);
            const uint32_t pictureId = (record->m_small & 0xfff) + (side == 1 ? 0x4000 : 0x5000);
            DibBitmap* picture = NewOrNull<DibBitmap>(m_gokres, pictureId, kPictureKind);
            DibBitmap* normal = NewOrNull<DibBitmap>(picture->Width(), picture->Height(), picture);
            DibBitmap* selected = NewOrNull<DibBitmap>(picture->Width(), picture->Height(), picture);
            picture->Blit(normal, 0, 0, 0, 0, 0, 0);
            picture->Blit(selected, 0, 0, 0, 0, 0, 0);
            sideMark.DrawKeyed(selected, 0, 0);
            colorKey = picture->m_colorKey;
            item->m_text = record->m_name.m_text;
            item->SetImages(normal, selected);
            m_missions->Insert(item);
            DeleteBitmap(picture);
        }
        seedNode = static_cast<IntPairNode*>(seedNode->m_successor);
        sideNode = static_cast<ObjectKeyValueNode*>(sideNode->m_successor);
    }
    LoadHeaderBitmaps();
    if (processed != 0) {
        m_missionList->m_fillColor = colorKey;
        m_missionList->Refresh();
    }
    return processed;
}

void SystemMissionsWindow::FillCharacterList(int tab, int force) {
    GameObjNotifier* system = GetSystem();
    if (!system) return;
    if ((m_tab == tab && force == 0) || !m_selectedMission) return;
    m_tab = tab;
    m_tabs->SelectTab(static_cast<uint32_t>(tab), 1);
    const bool decoys = m_tab == static_cast<int>(kDecoysTabId);
    for (CoolListItem* row = m_characters->First(); row; row = row->Next()) {
        ObjectKey rowKey;
        static_cast<ObjectListItem*>(row)->GetKey(&rowKey);
        if (ObjectKeyNode* node = m_objects.Find(&rowKey)) m_objects.Remove(static_cast<ObjectKeyIdNode*>(node));
    }
    m_characters->Clear();
    RoleIterator roles(system, 3);
    ObjectKey selectedKey;
    m_selectedMission->GetKey(&selectedKey);
    for (roles.Begin(); roles.Current(); roles.Advance()) {
        Role* role = AsRole(roles.Current());
        ObjectKey roleKey;
        role->GetKey(&roleKey);
        ObjectKey missionKey;
        role->GetMissionKey(&missionKey);
        if (missionKey.m_value != selectedKey.m_value) continue;
        m_objects.Add(NewOrNull<ObjectKeyIdNode>(&roleKey, 0u), 0);
        if (((role->m_flags78 & role_flag::kDecoy) != 0) != decoys) continue;
        AddCharacterRow(GetObjectInFamilies_01_ff(m_galaxy->m_side, &roleKey), 1, nullptr);
    }
    m_characterList->Refresh();
}

int DominantMissionSide(GameObjNotifier* system, int viewSide) {
    if (!system) return 0;
    const uint32_t side = static_cast<uint32_t>(ViewSideOf(viewSide));
    const uint32_t otherSide = side == 3 ? 3 : 2 - (side != 1);
    ObjectKeyValueSet ownMissions;
    ObjectKeyValueSet otherMissions;
    RoleIterator roles(system, 3);
    for (roles.Begin(); roles.Current(); roles.Advance()) {
        Role* role = AsRole(roles.Current());
        if (!CountsAsMission(role)) continue;
        ObjectKeyValueSet* target = nullptr;
        if (ControlSideOf(role) == side) {
            target = &ownMissions;
        } else if (ControlSideOf(role) == otherSide) {
            target = &otherMissions;
        }
        if (!target) continue;
        ObjectKey missionKey;
        role->GetMissionKey(&missionKey);
        if (target->Find(&missionKey)) continue;
        ObjectKey addKey;
        role->GetMissionKey(&addKey);
        target->AddKeyValue(&addKey, 0);
    }
    const uint16_t ownCount = static_cast<uint16_t>(ownMissions.Count());
    uint16_t otherCount = static_cast<uint16_t>(otherMissions.Count());
    uint32_t result = otherSide;
    if (otherCount == 0) {
        otherCount = 0;
        if (ownCount != 0) {
            result = side;
            otherCount = ownCount;
        }
        if (otherCount == 0) result = 3;
    }
    return static_cast<int>(result);
}

// Key getters that the linker folded across classes (the Role and list-node ones among them): copy the key at +0x6c / +0x1c.
ObjectKey* __thiscall ObjectKeyAtOffset6c(const void* object, ObjectKey* out) {
    out->m_value = *reinterpret_cast<const uint32_t*>(static_cast<const char*>(object) + 0x6c);
    return out;
}

ObjectKey* __thiscall ObjectKeyAtOffset1c(const void* object, ObjectKey* out) {
    out->m_value = *reinterpret_cast<const uint32_t*>(static_cast<const char*>(object) + 0x1c);
    return out;
}
