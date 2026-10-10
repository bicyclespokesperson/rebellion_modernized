#include "core/reb_abi.h"
#include "strategy_ui/fleet_window.h"

#include <new>

#include "app/rebapp_globals.h"
#include "audio/sound_manager.h"
#include "character/role.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/iostream_lib.h"
#include "core/stream_io.h"
#include "core/strstream_io.h"
#include "core/vptr_store.h"
#include "fleet/capship.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/fleet_list_item.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/system_missions_window.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_label.h"
#include "ui/cool_string_field.h"
#include "ui/cool_tab_control.h"
#include "ui/resource_dll.h"

extern const char g_gokresDllName[];  // 0x6a8708 "gokres.dll"
extern POINT g_fleetRowLines[12];     // 0x6a87b8 six two-point lines
extern RECT g_fleetInfoRect;          // 0x6a8818 where the selection's picture and totals are drawn

namespace {

constexpr uint32_t kStringSlot = 0;
constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kGokresSlot = 10;
constexpr uint32_t kPictureKind = 2;
constexpr uint32_t kHeaderKind = 10;
constexpr int16_t kStringGroup = 2;
constexpr int kShow = 5;
constexpr uint32_t kHiddenStyle = 0x10000000;
constexpr uint32_t kClipChildrenStyle = 0x2000000;
constexpr uint32_t kOpenSound = 0x25e;
constexpr uint32_t kCommandMessage = 0x111;
constexpr uint32_t kRedrawHeaderMessage = 0x467;
constexpr uint32_t kRedrawHeaderCommand = 0x466;
constexpr uint32_t kBackgroundOverlayId = 1;
constexpr int kConnectorPenStyle = PS_DOT;

constexpr uint32_t kSideId = 100;
constexpr uint32_t kCloseId = 0x65;
constexpr uint32_t kSectorId = 0xca;
constexpr uint32_t kOwnTabsId = 0x6a;
constexpr uint32_t kOtherTabsId = 0x6b;
constexpr uint32_t kShipListId = 200;
constexpr uint32_t kFleetListId = 0xc9;

constexpr int kPageShips = 0x66;
constexpr int kPageFighters = 0x67;
constexpr int kPageTroops = 0x68;
constexpr int kPagePersonnel = 0x69;

constexpr uint32_t kContentsFighters = 1;
constexpr uint32_t kContentsTroops = 2;
constexpr uint32_t kContentsPersonnel = 4;
constexpr uint32_t kContentsEnroute = 8;
constexpr uint32_t kContentsOrders = 0x10;
constexpr uint32_t kContentsIncomplete = 0x20;
constexpr uint32_t kContentsStationary = 0x40;

constexpr uint32_t kStateEnroute = 0x10;
constexpr uint32_t kStateHasOrders = 0x200;
constexpr uint32_t kStateCompleted = 0x4;

constexpr int kSkinStep = 0x32;
constexpr int kRowWidth = 0x5b;
constexpr int kRowHeight = 0x32;

const POINT* RowLine(int index) { return &g_fleetRowLines[index * 2]; }
constexpr int kLineFleetToShip = 0;
constexpr int kLineDown = 1;
constexpr int kLineThrough = 2;
constexpr int kLineBend = 3;
constexpr int kLineShipStub = 4;
constexpr int kLineFleetStub = 5;

ResourceStringId StringId(uint16_t id) { return ResourceStringId{static_cast<int16_t>(id), kStringGroup}; }

void DeleteBitmap(DibBitmap* bitmap) {
    if (!bitmap) return;
    bitmap->~DibBitmap();
    ::operator delete(bitmap);
}

DibBitmap* LoadBitmap(HMODULE module, uint32_t id, uint32_t kind = kPictureKind) {
    return NewOrNull<DibBitmap>(module, id, kind);
}

CoolStrobeButton* NewButton(HMODULE art, int x, int y, int width, int height, CoolWindow* parent, uint32_t id,
                            uint32_t normal, uint32_t pressed, DibBitmap* backdrop, int repeatRate, int pixelHit) {
    return NewOrNull<CoolStrobeButton>(art, x, y, width, height, parent, id, normal, pressed, backdrop, repeatRate, pixelHit);
}

bool FamilyIn(const ObjectKey& key, uint32_t first, uint32_t end) { return key.Family() >= first && key.Family() < end; }

int ViewSideOf(int side) { return side == 1 ? 1 : (side == 2 ? 2 : 0); }

uint32_t StateFlags(GameObjNotifier* object) { return static_cast<GameObj*>(object)->m_stateFlags; }

const char* NameOf(GameObjNotifier* object) { return static_cast<GameObj*>(object)->GetName()->m_text; }

// The skin of the pictures of the side: the Alliance's, or the Empire's 0x32 further.
int SkinOffset(int side) { return side != 1 ? kSkinStep : 0; }

void InsertRowBefore(CoolListItemList* list, FleetListItem* item, FleetListItem* anchor) {
    LinkNode* previous = anchor->m_prev;
    anchor->m_prev = item;
    item->m_prev = previous;
    item->m_next = anchor;
    if (!previous) {
        list->RewindToFirst();
    } else {
        previous->m_next = item;
    }
}

void InsertShipRowBefore(FleetListItem* item, FleetListItem* anchor) {
    LinkNode* previous = anchor->m_prev;
    previous->m_next = item;
    anchor->m_prev = item;
    item->m_next = anchor;
    item->m_prev = previous;
}

void SetFlag(FleetListItem* item, uint32_t flag, bool on) {
    if (on) {
        item->m_flags.m_value |= flag;
    } else {
        item->m_flags.m_value &= ~flag;
    }
}

}  // namespace

// ---- Construction ---------------------------------------------------------------------------------------------

FleetWindow::FleetWindow(HINSTANCE instance, int x, int y, CoolWindow* parent, uint32_t childId, GameObjNotifier* system)
    : CustomDialogBox(instance, x, y, 0xeb, 0x130, parent, childId, nullptr), m_uiFlags(0) {
    CoolWindow::m_style |= kClipChildrenStyle;
    CoolGameWindow::m_flags |= kGameWindowDraggable;
    ObjectKey key;
    if (system) system->GetKey(&key);
    m_systemKey = key;
    m_galaxy = static_cast<GalaxyWindow*>(parent);
    m_ownTabs = nullptr;
    m_otherTabs = nullptr;
    m_fleetList = nullptr;
    m_shipList = nullptr;
    m_infoBitmap = nullptr;
    m_headerActive = nullptr;
    m_headerInactive = nullptr;
    m_page = kPageShips;
    m_fleetFont = nullptr;
    m_shipFont = nullptr;
    m_dc = nullptr;
    m_gokres = FindResourceModule(kGokresSlot);
    if (!m_gokres) {
        char* dllLocation = GetDllLocation();
        m_gokres = LoadResourceModule(dllLocation, g_gokresDllName, kGokresSlot);
        if (dllLocation) operator delete(dllLocation);
    }
}

FleetWindow::~FleetWindow() {
    REB_KEEP_VPTR_STORE();
    delete m_ownTabs;
    delete m_otherTabs;
    delete m_fleetList;
    delete m_shipList;
    DeleteBitmap(m_infoBitmap);
    delete m_fleetItems;
    delete m_shipItems;
    DeleteBitmap(m_headerActive);
    DeleteBitmap(m_headerInactive);
    if (m_selectionLabel) {
        m_selectionLabel->~CoolLabel();
        ::operator delete(m_selectionLabel);
    }
    DeleteBitmap(m_infoBackdropOwn);
    DeleteBitmap(m_infoBackdropOther);
    if (m_dc) DeleteDC(m_dc);
    if (m_pen) DeleteObject(m_pen);
}

namespace {

struct KindTab {
    int x;
    uint32_t id;
    uint32_t normal;
    uint32_t pressed;
    uint32_t stateFour;
    uint32_t stateTwo;
    uint16_t tooltip;
    bool addTabFirst;
};

void AddKindTab(FleetWindow* window, HMODULE art, CoolTabControl* tabs, const KindTab& tab) {
    CoolStrobeButton* button = NewButton(art, tab.x, 0, 0x1f, 0x1c, tabs, tab.id, tab.normal, tab.pressed,
                                         tabs->m_background, 0, 0);
    button->LoadStateBitmap(4, tab.stateFour);
    if (tab.stateTwo != 0) button->LoadStateBitmap(2, tab.stateTwo);
    button->Create(0);
    const ResourceStringId tip = StringId(tab.tooltip);
    if (tab.addTabFirst) {
        tabs->AddTab(button, 0, 0);
        window->AddToolFromString(button, &tip);
    } else {
        window->AddToolFromString(button, &tip);
        tabs->AddTab(button, 0, 0);
    }
}

}  // namespace

void FleetWindow::OnCreate1() {
    GameObjNotifier* system = reinterpret_cast<SystemMissionsWindow*>(this)->SystemMissionsWindow::GetSystem();
    if (!system) return;
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    DibBitmap* shape = LoadBitmap(art, 0x2a12);
    m_controlSide = static_cast<int>(system->ControlSide());
    ObjectKey systemKey;
    system->GetKey(&systemKey);
    m_systemKey = systemKey;
    SetDragMode(kDragOutline);
    SetDragLimits(m_galaxy->m_panelArea, nullptr);
    HDC dc = GetDC(m_hwnd);
    m_galaxySide = m_galaxy->m_side;
    m_viewSide = (m_galaxy->m_side != 1) + 1;
    m_fleetItems = NewOrNull<CoolListItemList>(4);
    m_shipItems = NewOrNull<CoolListItemList>(4);
    DibBitmap* background = NewOrNull<DibBitmap>(static_cast<LONG>(shape->Width()), static_cast<LONG>(shape->Height()), shape, dc);
    shape->Blit(background, 0, 0, 0, 0, 0, 0);
    AddOverlay(background, kBackgroundOverlayId);
    DeleteBitmap(shape);
    SetPaintBackgroundById(kBackgroundOverlayId);
    DibBitmap* paint = m_paintBackground;
    const int skin = SkinOffset(m_viewSide);
    m_infoBackdropOwn = LoadBitmap(art, skin + 0x28a7);
    m_infoBitmap = NewOrNull<DibBitmap>(0x7d, 0x31, m_infoBackdropOwn, dc);
    m_infoBackdropOther = LoadBitmap(art, 0x28d9 - skin);

    m_ownTabs = NewOrNull<CoolTabControl>(m_gokres, 99, 0x60, 0x83, 0x1c, kOwnTabsId, this, background);
    m_ownTabs->m_style &= ~kHiddenStyle;
    m_ownTabs->Create(0);
    m_otherTabs = NewOrNull<CoolTabControl>(m_gokres, 99, 0x60, 0x83, 0x1c, kOtherTabsId, this, background);
    m_otherTabs->m_style = m_ownTabs->m_style & ~kHiddenStyle;
    m_otherTabs->Create(0);
    m_selectionLabel = NewOrNull<CoolLabel>(static_cast<const char*>(nullptr), 100, 0x1d, 0x80, 0x19, 0u, 0x13u);
    m_selectionLabel->SetFont(5);
    m_selectionLabel->SetColor(0xffffff, nullptr);

    const KindTab ownTabs[] = {
        {1, kPageShips, uint32_t(skin + 0x28a9), uint32_t(skin + 0x28b0), uint32_t(skin + 0x28b0), 0, 0x3823, false},
        {0x21, kPageFighters, uint32_t(skin + 0x28aa), uint32_t(skin + 0x28b1), uint32_t(skin + 0x28b1),
         uint32_t(skin + 0x28ad), 0x3820, false},
        {0x41, kPageTroops, uint32_t(skin + 0x28ab), uint32_t(skin + 0x28b2), uint32_t(skin + 0x28b2),
         uint32_t(skin + 0x28ae), 0x3821, true},
        {0x61, kPagePersonnel, uint32_t(skin + 0x28ac), uint32_t(skin + 0x28b3), uint32_t(skin + 0x28b3),
         uint32_t(skin + 0x28af), 0x3822, false},
    };
    const KindTab otherTabs[] = {
        {1, kPageShips, uint32_t(0x28db - skin), uint32_t(0x28e2 - skin), uint32_t(0x28e2 - skin), 0, 0x3823, false},
        {0x21, kPageFighters, uint32_t(0x28dc - skin), uint32_t(0x28e3 - skin), uint32_t(0x28e3 - skin),
         uint32_t(0x28df - skin), 0x3820, false},
        {0x41, kPageTroops, uint32_t(0x28dd - skin), uint32_t(0x28e4 - skin), uint32_t(0x28e4 - skin),
         uint32_t(0x28e0 - skin), 0x3821, false},
        {0x61, kPagePersonnel, uint32_t(0x28de - skin), uint32_t(0x28e5 - skin), uint32_t(0x28e5 - skin),
         uint32_t(0x28e1 - skin), 0x3822, false},
    };
    for (const KindTab& tab : ownTabs) AddKindTab(this, art, m_ownTabs, tab);
    for (const KindTab& tab : otherTabs) AddKindTab(this, art, m_otherTabs, tab);

    int titleLeft = 2;
    if (DibBitmap* sectorShape = LoadBitmap(art, 0x27e1)) {
        CoolStrobeButton* sector = NewButton(art, 3, 3, sectorShape->Width(), sectorShape->Height(), this, kSectorId, 0x27e1,
                                             0x27e0, m_paintBackground, 0, 1);
        if (sector) {
            sector->SetStateFlags(0x80);
            sector->Create(0);
            m_buttons.Insert(sector);
            titleLeft = sectorShape->Width() + 5;
        }
        DeleteBitmap(sectorShape);
    }
    m_titleLabel->m_height = 0x10;
    m_titleLabel->MoveTo(titleLeft, 2);
    m_titleLabel->SetText(NameOf(system));
    m_titleLabel->SetFont(5);
    m_titleLabel->m_drawFlags = 0x24;
    m_titleLabel->SetColor(0x2000000, nullptr);
    m_titleLabel->Measure(dc);
    m_titleRect.bottom = m_titleRect.top + m_titleLabel->m_rect.bottom - m_titleLabel->m_rect.top;
    m_fleetFont = CoolGetFont(10);
    m_shipFont = CoolGetFont(10);
    m_pen = CreatePen(kConnectorPenStyle, 1, 0x2ffffff);

    DibBitmap* closeShape = LoadBitmap(art, 0x277c);
    CoolStrobeButton* close = NewButton(art, m_width - closeShape->Width() - 3, 3, closeShape->Width(), closeShape->Height(),
                                        this, kCloseId, 0x277c, 0x277d, m_paintBackground, 0, 1);
    if (close) {
        close->SetStateFlags(0x80);
        close->Create(0);
        m_buttons.Insert(close);
    }
    DeleteBitmap(closeShape);
    DibBitmap* sideShape = LoadBitmap(art, 0x280d);
    if (sideShape && close) {
        CoolStrobeButton* side = NewButton(art, close->m_x - sideShape->Width(), 3, sideShape->Width(), sideShape->Height(),
                                           this, kSideId, 0x280d, 0x280e, m_paintBackground, 0, 1);
        side->SetStateFlags(0x80);
        side->Create(0);
        m_buttons.Insert(side);
        DeleteBitmap(sideShape);
    }

    if (m_viewSide == 1) {
        m_headerActive = LoadBitmap(art, 0x27d9, kHeaderKind);
        m_headerInactive = LoadBitmap(art, 0x283e, kHeaderKind);
        m_controlSide = 2;
    } else {
        m_controlSide = 1;
        m_headerActive = LoadBitmap(art, 0x283b, kHeaderKind);
        m_headerInactive = LoadBitmap(art, 0x27d8, kHeaderKind);
    }

    m_fleetList = NewOrNull<CoolDragList>(art, 4, 0x1d, 0x5b, 0x10a, this, paint, kFleetListId, 0x5b, 0x32, 7, 0x299a, 0, 0, 0,
                                          1, nullptr);
    if (m_fleetList) {
        m_fleetList->SetDragTarget(m_galaxy->m_hwnd);
        m_fleetList->Create(0);
        m_fleetList->SetSelectedTextColor(0xffffff);
        m_fleetList->SetTextColor(0xffffff);
        m_fleetList->SetItemList(m_fleetItems);
        m_fleetList->Refresh();
        m_fleetList->SetFont(10);
        m_fleetList->m_textFormat = 0x20;
        m_fleetList->m_textOffsetX = 8;
        m_fleetList->m_textOffsetY = 3;
        m_fleetList->CreateVerticalScrollBar(art, m_fleetList, 7, 0x29cb, 0x10a, 0, 0x10a, 0, nullptr);
    }
    m_shipList = NewOrNull<CoolDragList>(art, 0x65, 0x7f, 0x85, 0xa4, this, paint, kShipListId, 0x7d, 0x32, 0, 0, 0, 0, 0, 1,
                                         nullptr);
    if (m_shipList) {
        m_shipList->SetDragTarget(m_galaxy->m_hwnd);
        m_shipList->Create(0);
        m_shipList->SetSelectedTextColor(0xffffff);
        m_shipList->SetTextColor(0xffffff);
        m_shipList->SetItemList(m_shipItems);
        m_shipList->SetFont(10);
        m_shipList->m_textFormat = 0x29;
        m_shipList->CreateVerticalScrollBar(art, m_shipList, 7, 0x29cb, 0xa4, 0, 0xa4, 0, nullptr);
    }
    SyncFleetRows();
    ReleaseDC(m_hwnd, dc);
    if ((m_galaxy->m_mouseMode & 4) == 0) {
        SoundRequest request(0, 0, 0, 0, 0, 0);
        request.m_kind = 1;
        request.m_soundId = kOpenSound;
        request.m_moduleIndex = kBitmapSlot;
        SoundManager::Play(&request);
    }
}

// ---- Messages -------------------------------------------------------------------------------------------------

LRESULT REB_STDCALL FleetWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND hwnd = m_hwnd;
    if (message < 8) {
        if (message == WM_SETFOCUS) {
            if (!m_renameField) {
                SetFocus(m_fleetList->m_hwnd);
            } else {
                SetFocus(m_renameField->m_hwnd);
            }
            return 0;
        }
        if (message == WM_CREATE) {
            HDC windowDc = GetDC(hwnd);
            m_dc = CreateCompatibleDC(windowDc);
            ReleaseDC(hwnd, windowDc);
            CustomDialogBox::WindowProc(WM_CREATE, wParam, lParam);
            PostMessageA(m_parent->m_hwnd, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(hwnd), 0);
            return 0;
        }
        return CustomDialogBox::WindowProc(message, wParam, lParam);
    }
    if (message > WM_COMMAND) {
        if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN) {
            PostMessageA(m_parent->m_hwnd, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(hwnd), 0);
        } else if (message == kRedrawHeaderMessage) {
            InvalidateRect(hwnd, &m_titleRect, 0);
            return 0;
        }
        return CustomDialogBox::WindowProc(message, wParam, lParam);
    }
    if (message == WM_COMMAND) return OnWmCommand(WM_COMMAND, wParam, lParam);
    if (message != WM_PAINT) return CustomDialogBox::WindowProc(message, wParam, lParam);

    PAINTSTRUCT paint;
    HDC dc = BeginPaint(hwnd, &paint);
    DibBitmap* header = hwnd == m_galaxy->m_activeChild ? m_headerActive : m_headerInactive;
    header->Blit(m_paintBackground, 2, 2, 0, 0, m_clientWidth - 4, 0x12);
    SelectPalette(dc, g_palette, 0);
    DrawBackground(dc);
    DibBitmap* info = nullptr;
    if ((m_uiFlags.m_value & kFleetOwnSelected) != 0) {
        info = m_infoBackdropOwn;
    } else if ((m_uiFlags.m_value & kFleetOtherSelected) != 0) {
        info = m_infoBackdropOther;
    }
    if (info) info->DrawToDc(dc, 0x40, 0xcc0020, 0x61, 0x1d, 0, 0, 0, 0);
    if (m_infoBitmap) m_infoBitmap->DrawToDc(dc, 0x40, 0xcc0020, 100, 0x2a, 0, 0, 0, 0);
    if (m_titleLabel) m_titleLabel->Draw(dc);
    if (m_selectionLabel) m_selectionLabel->Draw(dc);
    EndPaint(hwnd, &paint);
    return 0;
}

LRESULT FleetWindow::OnWmCommand(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND galaxyWindow = m_galaxy->m_hwnd;
    GameObjNotifier* system = reinterpret_cast<SystemMissionsWindow*>(this)->SystemMissionsWindow::GetSystem();
    if (!system) return 0;
    LRESULT result = 0;
    const uint32_t code = static_cast<uint32_t>(wParam) >> 16;
    switch (static_cast<uint32_t>(wParam) & 0xffff) {
    case kSideId:
        PostMessageA(galaxyWindow, kCommandMessage, (m_childId & 0xffff) | (kRedrawHeaderCommand << 16), 0);
        break;
    case kCloseId:
        PostCloseRequest();
        break;
    default:
        result = CustomDialogBox::WindowProc(message, wParam, lParam);
        break;
    case kOwnTabsId:
    case kOtherTabsId:
        m_page = static_cast<int>(code);
        PostMessageA(m_parent->m_hwnd, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
        UpdateShipList();
        break;
    case kShipListId:
        m_uiFlags.m_value |= kFleetShipListActive;
        if (code == kDragListDropped) {
            PostMessageA(m_galaxy->m_hwnd, kCommandMessage, (m_childId & 0xffff) | (code << 16), lParam);
        } else if (code == kDragListSelectionChanged) {
            PostMessageA(m_parent->m_hwnd, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
        }
        break;
    case kFleetListId:
        m_uiFlags.m_value &= ~kFleetShipListActive;
        if (code == kDragListDropped) {
            PostMessageA(m_galaxy->m_hwnd, kCommandMessage, (m_childId & 0xffff) | (code << 16), lParam);
            return result;
        }
        if (code != kDragListSelectionChanged) {
            if (code != kDragListActivated) return 0;
            FleetListItem* next = static_cast<FleetListItem*>(m_fleetItems->First());
            while (next) {
                FleetListItem* position = next;
                next = static_cast<FleetListItem*>(position->Next());
                if ((position->m_flags.m_value & kFleetItemFleet) == 0) continue;
                if ((position->m_state.m_value & kListItemSelected) == 0) continue;
                GameObjNotifier* fleet = GetObjectInFamilies_08_10(m_galaxy->m_side, &position->m_key);
                if (!fleet) continue;
                position->m_flags.m_value ^= kFleetItemExpanded;
                SyncShipRows(fleet, &position);
                m_fleetList->Refresh();
            }
            return result;
        }
        PostMessageA(m_parent->m_hwnd, kRedrawHeaderMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
        UpdateShipList();
        break;
    case kSectorId:
        if (m_galaxy) {
            ObjectKey sectorKey;
            if (static_cast<GameObj*>(system)->GetValidParentKey(&sectorKey) != 0) m_galaxy->OpenSectorWindow(&sectorKey);
        }
        break;
    }
    return result;
}

int FleetWindow::OnRightButtonUp(int x, int y, CoolWindow* sender) {
    if (sender->m_childId == kShipListId) {
        m_uiFlags.m_value |= kFleetShipListActive;
    } else if (sender->m_childId == kFleetListId) {
        m_uiFlags.m_value &= ~kFleetShipListActive;
    }
    return CustomDialogBox::OnRightButtonUp(x, y, sender);
}

void FleetWindow::OnKeyDown(WPARAM key, LPARAM lParam) {
    if (key != VK_ESCAPE) {
        m_parent->WindowProc(WM_KEYDOWN, key, lParam);
        return;
    }
    PostCloseRequest();
}

void FleetWindow::SaveState(ostream* out) {
    WriteValue32(out, &m_controlSide);
    WriteValue32(out, &m_viewSide);
    WriteValue32(out, &m_galaxySide);
    m_systemKey.Write(out);
    WriteValue32(out, &m_commandId);
    WriteValue32(out, &m_page);
}

void FleetWindow::LoadState(istream* in) {
    ReadValue32(in, &m_controlSide);
    ReadValue32(in, &m_viewSide);
    ReadValue32(in, &m_galaxySide);
    m_systemKey.Read(in);
    ReadValue32(in, &m_commandId);
    m_childId = m_commandId;
    ReadValue32(in, &m_page);
}

void FleetWindow::Virtual35() {
    if ((m_uiFlags.m_value & kFleetNeedsRefresh) != 0) {
        m_uiFlags.m_value &= ~kFleetNeedsRefresh;
        SyncFleetRows();
    }
}

namespace {

bool ChangesFleetRows(const ObjectKey* key) {
    return FamilyIn(*key, 0x14, 0x20) || FamilyIn(*key, 8, 0x10) || FamilyIn(*key, 0x10, 0x14) || FamilyIn(*key, 0x30, 0x40);
}

}  // namespace

void FleetWindow::OnChildMoved(const ObjectKey* key, GameObjNotifier*, const ObjectKey*, GameObjNotifier*) {
    if (ChangesFleetRows(key)) m_uiFlags.m_value |= kFleetNeedsRefresh;
}

void FleetWindow::OnChildAdded(const ObjectKey* key, GameObjNotifier*) {
    if (ChangesFleetRows(key)) m_uiFlags.m_value |= kFleetNeedsRefresh;
}

void FleetWindow::OnObjectUpdated(const ObjectKey* key, GameObjNotifier*) {
    if (ChangesFleetRows(key)) m_uiFlags.m_value |= kFleetNeedsRefresh;
}

void FleetWindow::GetSelection(ObjectKeyValueSet* out, CoolWindow* source) {
    out->Clear();
    CoolListItemList* items = nullptr;
    if (source == m_fleetList || (!source && (m_uiFlags.m_value & kFleetShipListActive) == 0)) {
        items = m_fleetList->m_items;
    } else if (source == m_shipList || (!source && (m_uiFlags.m_value & kFleetShipListActive) != 0)) {
        items = m_shipList->m_items;
    }
    CoolListItem* row = items ? items->First() : nullptr;
    for (; row; row = row->Next()) {
        FleetListItem* item = static_cast<FleetListItem*>(row);
        if ((item->m_state.m_value & kListItemSelected) == 0) continue;
        out->Add(NewOrNull<ObjectKeyValueNode>(item->m_key, 0));
    }
}

ObjectKey FleetWindow::GetKeyAt(int x, int y) {
    ObjectKey result = m_systemKey;
    CoolListItem* item;
    if (x < 4 || x >= 0x60 || y < 0x1d || y >= 0x128) {
        if (x < 0x65 || x >= 0xeb || y < 0x7f || y >= 0x124) return result;
        POINTS inList = {static_cast<SHORT>(x - 0x65), static_cast<SHORT>(y - 0x7f)};
        item = m_shipList->HitTest(m_shipList->ClientToList(inList));
        if (!item) {
            CoolListItemList selection(1);
            CoolListItem* row = nullptr;
            m_fleetList->CopySelectionTo(&selection);
            if (selection.m_head) row = m_fleetItems->FindById(static_cast<CoolListItem*>(selection.m_head)->m_id);
            if (row) result = static_cast<FleetListItem*>(row)->m_key;
            return result;
        }
    } else {
        POINTS inList = {static_cast<SHORT>(x - 4), static_cast<SHORT>(y - 0x1d)};
        item = m_fleetList->HitTest(m_fleetList->ClientToList(inList));
        if (!item) return result;
    }
    result = static_cast<FleetListItem*>(item)->m_key;
    return result;
}

ObjectKey FleetWindow::GetDropKeyAt(int x, int y) {
    ObjectKey result;
    CoolListItem* row = m_fleetItems->First();
    if (!row) result = m_systemKey;
    bool nothingOwn = true;
    bool haveSelectedOwn = false;
    while (row) {
        CoolListItem* next = row->Next();
        FleetListItem* item = static_cast<FleetListItem*>(row);
        nothingOwn = nothingOwn && (item->m_flags.m_value & kFleetItemOwn) == 0;
        if ((item->m_state.m_value & kListItemSelected) != 0 && (item->m_flags.m_value & kFleetItemOwn) != 0) {
            if (!haveSelectedOwn) {
                result = item->m_key;
                haveSelectedOwn = true;
            } else {
                result = ObjectKey();
            }
        }
        row = next;
    }
    if (nothingOwn) result = m_systemKey;
    CoolListItem* hit;
    if (x < 4 || x >= 0x60 || y < 0x1d || y >= 0x128) {
        if (x < 0x65 || x >= 0xeb || y < 0x7f || y >= 0x124 || m_page != kPageShips) return result;
        POINTS inList = {static_cast<SHORT>(x - 0x65), static_cast<SHORT>(y - 0x7f)};
        hit = m_shipList->HitTest(m_shipList->ClientToList(inList));
    } else {
        result = m_systemKey;
        POINTS inList = {static_cast<SHORT>(x - 4), static_cast<SHORT>(y - 0x1d)};
        hit = m_fleetList->HitTest(m_fleetList->ClientToList(inList));
    }
    if (hit) result = static_cast<FleetListItem*>(hit)->m_key;
    return result;
}

RECT* FleetWindow::GetRenameRect(RECT* out) {
    RECT rect;
    CustomDialogBox::GetRenameRect(&rect);
    CoolListItemList* items;
    CoolDragList* list;
    if ((m_uiFlags.m_value & kFleetShipListActive) == 0) {
        items = m_fleetItems;
        list = m_fleetList;
    } else {
        items = m_shipItems;
        list = m_shipList;
    }
    CoolListItem* row = items->First();
    if (row) {
        while ((row->m_state.m_value & kListItemSelected) == 0) {
            row = row->Next();
            if (!row) break;
        }
        if (row) {
            const RECT textRect = list->GetItemTextRect(row);
            rect.left = textRect.left + list->m_x;
            rect.top = textRect.top + list->m_y;
            rect.bottom = textRect.bottom + list->m_y;
            rect.right = list->m_width - 8 + list->m_x;
        }
    }
    *out = rect;
    return out;
}

void FleetWindow::ShowObjectPage(const ObjectKey* key) {
    GameObjNotifier* ship = nullptr;
    GameObjNotifier* fleet = nullptr;
    GameObjNotifier* object = GetObjectInFamilies_01_ff(m_galaxy->m_side, key);
    if (FamilyIn(*key, 0x14, 0x1c)) {
        ship = object;
        if (object) fleet = object->m_parent;
    } else if (FamilyIn(*key, 8, 0x10)) {
        fleet = object;
    } else if (object) {
        ship = object->m_parent;
        if (ship) fleet = ship->m_parent;
    }
    FleetListItem* row = nullptr;
    if (fleet) {
        m_uiFlags.m_value &= ~kFleetShipListActive;
        ObjectKey fleetKey;
        fleet->GetKey(&fleetKey);
        row = static_cast<FleetListItem*>(m_fleetItems->FindById(fleetKey.Id()));
        // BUG(original): crashes when the fleet has no row.
        row->m_state.m_value |= kListItemSelected;
        m_fleetList->Refresh();
    }
    if (!row || !ship) return;
    CoolTabControl* tabs;
    int page;
    const int viewSide = ViewSideOf(m_galaxy->m_side);
    if (ship != object) {
        m_uiFlags.m_value &= ~kFleetShipListActive;
        row->m_flags.m_value ^= kFleetItemExpanded;
        FleetListItem* position = row;
        SyncShipRows(fleet, &position);
        m_fleetList->Refresh();
        m_uiFlags.m_value |= kFleetShipListActive;
        tabs = static_cast<int>(fleet->ControlSide()) == viewSide ? m_ownTabs : m_otherTabs;
        if (FamilyIn(*key, 0x1c, 0x20)) {
            page = kPageFighters;
        } else if (FamilyIn(*key, 0x10, 0x14)) {
            page = kPageTroops;
        } else if (FamilyIn(*key, 0x30, 0x40)) {
            page = kPagePersonnel;
        } else {
            return;
        }
    } else {
        m_uiFlags.m_value |= kFleetShipListActive;
        page = kPageShips;
        tabs = static_cast<int>(fleet->ControlSide()) == viewSide ? m_ownTabs : m_otherTabs;
    }
    tabs->SelectTab(static_cast<uint32_t>(page), 1);
}

int FleetWindow::Virtual29() {
    GameObjNotifier* system = reinterpret_cast<SystemMissionsWindow*>(this)->SystemMissionsWindow::GetSystem();
    if (!system) return 0;
    uint32_t side = static_cast<uint32_t>(ViewSideOf(m_galaxy->m_side));
    if (CountFleets(static_cast<GameObj*>(system), side, 3) == 0) {
        side = static_cast<uint32_t>(m_galaxy->m_side == 1 ? 2 : (m_galaxy->m_side == 2 ? 1 : 0));
        if (CountFleets(static_cast<GameObj*>(system), side, 3) == 0) side = system->ControlSide();
    }
    if (side != 1) {
        if (side != 2) return 0x2d12;
        return 0x2d11;
    }
    return 0x2d10;
}

// ---- The fleet rows -------------------------------------------------------------------------------------------

void FleetWindow::SyncFleetRows() {
    GameObjNotifier* system = reinterpret_cast<SystemMissionsWindow*>(this)->SystemMissionsWindow::GetSystem();
    if (!system) return;
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    FleetIterator fleets(system, 3);
    FleetListItem* position = static_cast<FleetListItem*>(m_fleetItems->First());
    const int previousSide = m_controlSide;
    m_objects.Clear();
    ObjectKeyIdNode* systemNode = NewOrNull<ObjectKeyIdNode>();
    systemNode->m_key = m_systemKey;
    systemNode->AddId(m_childId);
    m_objects.Add(systemNode, 0);
    fleets.Begin();
    if (!fleets.Current()) {
        m_controlSide = static_cast<int>(system->ControlSide());
    } else {
        m_controlSide = 2 - (m_viewSide != 1);
        do {
            GameObjNotifier* fleet = fleets.Current();
            if (static_cast<int>(fleet->ControlSide()) == m_viewSide) m_controlSide = m_viewSide;
            if (!position) {
                AddFleetRow(fleet, &position);
                fleets.Advance();
            } else {
                ObjectKey fleetKey;
                fleet->GetKey(&fleetKey);
                const uint32_t rowKey = position->m_key.m_value;
                if (fleetKey.m_value < rowKey) {
                    AddFleetRow(fleet, &position);
                    fleets.Advance();
                } else {
                    ObjectKey sameKey;
                    fleet->GetKey(&sameKey);
                    if (rowKey == sameKey.m_value) {
                        ObjectKeyIdNode* node = NewOrNull<ObjectKeyIdNode>();
                        ObjectKey nodeKey;
                        fleet->GetKey(&nodeKey);
                        node->m_key = nodeKey;
                        node->AddId(m_childId);
                        m_objects.Add(node, 0);
                        SyncShipRows(fleet, &position);
                        fleets.Advance();
                    } else {
                        ObjectKey laterKey;
                        fleet->GetKey(&laterKey);
                        if (rowKey < laterKey.m_value) RemoveRows(&position);
                    }
                }
            }
        } while (fleets.Current());
    }
    while (position) RemoveRows(&position);
    if (m_controlSide != previousSide) {
        DeleteBitmap(m_headerActive);
        DeleteBitmap(m_headerInactive);
        if (m_controlSide == 1) {
            m_headerActive = LoadBitmap(art, 0x283b, kHeaderKind);
            m_headerInactive = LoadBitmap(art, 0x27d8, kHeaderKind);
        } else if (m_controlSide == 2) {
            m_headerActive = LoadBitmap(art, 0x27d9, kHeaderKind);
            m_headerInactive = LoadBitmap(art, 0x283e, kHeaderKind);
        } else {
            m_headerActive = LoadBitmap(art, 0x283f, kHeaderKind);
            m_headerInactive = LoadBitmap(art, 0x2840, kHeaderKind);
        }
        if (m_titleLabel) m_titleLabel->SetColor(0, nullptr);
        InvalidateRect(m_hwnd, &m_titleRect, 0);
    }
    if (m_fleetList) m_fleetList->Refresh();
    UpdateShipList();
}

void FleetWindow::RemoveRows(FleetListItem** position) {
    FleetListItem* row = *position;
    FleetListItem* next = static_cast<FleetListItem*>(row->m_next);
    while (row) {
        m_fleetItems->Remove(row);
        if (*position) delete *position;
        *position = nullptr;
        if (next && (next->m_flags.m_value & kFleetItemFleet) == 0) {
            *position = next;
            next = static_cast<FleetListItem*>(next->m_next);
        }
        row = *position;
    }
    *position = next;
}

void FleetWindow::AddFleetRow(GameObjNotifier* fleet, FleetListItem** position) {
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    ObjectKeyIdNode* node = NewOrNull<ObjectKeyIdNode>();
    ObjectKey fleetKey;
    fleet->GetKey(&fleetKey);
    node->m_key = fleetKey;
    node->AddId(m_childId);
    m_objects.Add(node, 0);
    ObjectKey rowKey;
    fleet->GetKey(&rowKey);
    FleetListItem* item = NewOrNull<FleetListItem>(&rowKey);
    DibBitmap* image = NewOrNull<DibBitmap>(kRowWidth, kRowHeight, m_paintBackground, m_dc);
    if (image->DibSectionHandle() != 0) {
        int skin = 0;
        DibBitmap* frame;
        DibBitmap* selectedFrame;
        if (fleet->ControlSide() == 1) {
            frame = LoadBitmap(art, 0x28a0);
            selectedFrame = LoadBitmap(art, 0x28a1);
        } else {
            skin = kSkinStep;
            frame = LoadBitmap(art, 0x28d2);
            selectedFrame = LoadBitmap(art, 0x28d3);
        }
        const uint8_t colorKey = frame->m_colorKey;
        image->FillRect(0, 0, image->Width(), image->Height(), colorKey);
        HGDIOBJ previousBitmap = SelectObject(m_dc, image->DibSectionHandle());
        HPALETTE previousPalette = SelectPalette(m_dc, g_palette, 0);
        HGDIOBJ previousPen = SelectObject(m_dc, m_pen);
        HGDIOBJ previousFont = SelectObject(m_dc, m_fleetFont);
        if ((StateFlags(fleet) & kStateEnroute) != 0) {
            DibBitmap* badge = LoadBitmap(art, skin + 0x28b7);
            badge->DrawKeyed(image, 5, 5);
            DeleteBitmap(badge);
        }
        if ((StateFlags(fleet) & kStateHasOrders) != 0) {
            DibBitmap* badge = LoadBitmap(art, skin + 0x28b8);
            badge->DrawKeyed(image, 5, 5);
            DeleteBitmap(badge);
        }
        if (frame) {
            frame->DrawKeyed(image, 5, 5);
            DeleteBitmap(frame);
        }
        item->m_flags.m_value = fleet->ControlSide() == static_cast<uint32_t>(m_viewSide)
                                    ? item->m_flags.m_value | kFleetItemOwn
                                    : item->m_flags.m_value & ~kFleetItemOwn;
        Polyline(m_dc, RowLine(kLineFleetToShip), 2);
        item->m_text = NameOf(fleet);
        SelectPalette(m_dc, previousPalette, 0);
        SelectObject(m_dc, previousPen);
        SelectObject(m_dc, previousBitmap);
        SelectObject(m_dc, previousFont);
        DibBitmap* selected = NewOrNull<DibBitmap>(static_cast<LONG>(image->Width()), static_cast<LONG>(image->Height()), image, m_dc);
        image->Blit(selected, 0, 0, 0, 0, 0, 0);
        selectedFrame->DrawKeyed(selected, 0, 0);
        DeleteBitmap(selectedFrame);
        item->SetImages(image, selected);
    }
    item->m_flags.m_value = (item->m_flags.m_value & ~kFleetItemExpanded) | kFleetItemFleet;
    FleetListItem* anchor = *position;
    if (!anchor) {
        m_fleetItems->Insert(item);
    } else {
        InsertRowBefore(m_fleetItems, item, anchor);
    }
    *position = item;
    SyncShipRows(fleet, position);
}

void FleetWindow::SyncShipRows(GameObjNotifier* fleet, FleetListItem** position) {
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    FleetListItem* fleetRow = *position;
    FleetListItem* shipRow = nullptr;
    CapShipIterator ships(fleet, 3);
    const uint32_t fleetFlags = fleet->m_flags;
    const bool expanded = ((*position)->m_flags.m_value >> 1 & 1) != 0;
    uint32_t contents = 0;
    DibBitmap* marker = LoadBitmap(art, ((fleetFlags & 0xc0) != 0x40 ? kSkinStep : 0) + 0x28a1);
    DibBitmap* fleetImage = fleetRow->m_image;
    DibBitmap* fleetSelected = fleetRow->m_selectedImage;
    DibBitmap* scratch = NewOrNull<DibBitmap>(kRowWidth, kRowHeight, m_paintBackground, m_dc);
    scratch->FillRect(0, 0, kRowWidth, kRowHeight, 0);
    HGDIOBJ previousBitmap = SelectObject(m_dc, scratch->DibSectionHandle());
    HPALETTE previousPalette = SelectPalette(m_dc, g_palette, 0);
    HGDIOBJ previousPen = SelectObject(m_dc, m_pen);
    SelectObject(m_dc, m_shipFont);
    fleetRow->m_text = NameOf(fleet);
    if (!expanded) {
        fleetImage->FillRect(0, 0, 3, kRowHeight, fleetImage->m_colorKey);
        fleetSelected->FillRect(0, 0, 3, kRowHeight, fleetImage->m_colorKey);
    } else {
        SelectObject(m_dc, m_pen);
        Polyline(m_dc, RowLine(kLineDown), 2);
        scratch->DrawKeyed(fleetImage, 0, 0);
        scratch->DrawKeyed(fleetSelected, 0, 0);
    }
    ships.Begin();
    *position = static_cast<FleetListItem*>(fleetRow->m_next);

    auto drawThroughLine = [&](FleetListItem* row) {
        DibBitmap* image = row->m_image;
        DibBitmap* selected = row->m_selectedImage;
        scratch->FillRect(0, 0, kRowWidth, kRowHeight, 0);
        SelectObject(m_dc, scratch->DibSectionHandle());
        SelectObject(m_dc, m_pen);
        Polyline(m_dc, RowLine(kLineThrough), 2);
        scratch->DrawKeyed(image, 0, 0);
        scratch->DrawKeyed(selected, 0, 0);
    };

    while (ships.Current()) {
        uint32_t shipContents = 0;
        bool needNewRow = false;
        ObjectKeyIdNode* node = NewOrNull<ObjectKeyIdNode>();
        ObjectKey nodeKey;
        ships.Current()->GetKey(&nodeKey);
        node->m_key = nodeKey;
        node->AddId(m_childId);
        m_objects.Add(node, 0);
        if (!*position) {
            needNewRow = true;
        } else {
            ObjectKey currentKey;
            ships.Current()->GetKey(&currentKey);
            if (currentKey.m_value == (*position)->m_key.m_value) {
                if (shipRow) drawThroughLine(shipRow);
                shipRow = *position;
                GameObjNotifier* ship = ships.Current();
                shipContents = FleetContents(ship);
                if (shipContents != shipRow->m_userData) {
                    shipRow->m_userData = shipContents;
                    DibBitmap* image = shipRow->m_image;
                    DibBitmap* selected = shipRow->m_selectedImage;
                    ObjectKey shipKey;
                    ship->GetKey(&shipKey);
                    DibBitmap* picture = CreateObjectPicture(m_gokres, &shipKey, ship, 0, 1);
                    image->FillRect(0, 0, kRowWidth, kRowHeight, picture->m_colorKey);
                    picture->DrawKeyed(image, 5, 0xf);
                    DeleteBitmap(picture);
                    scratch->FillRect(0, 0, kRowWidth, kRowHeight, 0);
                    SelectObject(m_dc, scratch->DibSectionHandle());
                    SelectObject(m_dc, m_pen);
                    Polyline(m_dc, RowLine(kLineShipStub), 2);
                    Polyline(m_dc, RowLine(kLineBend), 2);
                    scratch->DrawKeyed(image, 0, 0);
                    DrawFleetContentIcons(image, static_cast<int>(ship->ControlSide()), shipContents, 1);
                    image->Blit(selected, 0, 0, 0, 0, 0, 0);
                    marker->DrawKeyed(selected, 0, 0);
                }
                shipRow->m_text = NameOf(ships.Current());
                SetFlag(shipRow, kFleetItemHidden, !expanded);
                *position = static_cast<FleetListItem*>((*position)->m_next);
                ships.Advance();
            } else if (((*position)->m_flags.m_value & kFleetItemFleet) == 0) {
                ObjectKey shipKey;
                ships.Current()->GetKey(&shipKey);
                const uint32_t rowKey = (*position)->m_key.m_value;
                if (shipKey.m_value < rowKey) {
                    needNewRow = true;
                } else {
                    ObjectKey laterKey;
                    ships.Current()->GetKey(&laterKey);
                    if (rowKey < laterKey.m_value) {
                        FleetListItem* gone = *position;
                        LinkNode* previous = gone->m_prev;
                        LinkNode* following = gone->m_next;
                        previous->m_next = following;
                        if (following) following->m_prev = previous;
                        delete gone;
                        *position = static_cast<FleetListItem*>(following);
                    }
                }
            } else {
                needNewRow = true;
            }
        }
        if (needNewRow) {
            if (shipRow) drawThroughLine(shipRow);
            ObjectKey rowKey;
            ships.Current()->GetKey(&rowKey);
            FleetListItem* row = NewOrNull<FleetListItem>(&rowKey);
            shipRow = row;
            DibBitmap* image = NewOrNull<DibBitmap>(kRowWidth, kRowHeight, m_paintBackground);
            DibBitmap* selected = NewOrNull<DibBitmap>(kRowWidth, kRowHeight, m_paintBackground);
            GameObjNotifier* ship = ships.Current();
            shipContents = FleetContents(ship);
            row->m_userData = shipContents;
            ObjectKey pictureKey;
            ship->GetKey(&pictureKey);
            DibBitmap* picture = CreateObjectPicture(m_gokres, &pictureKey, ship, 0, 1);
            image->FillRect(0, 0, kRowWidth, kRowHeight, picture->m_colorKey);
            picture->DrawKeyed(image, 5, 0xf);
            DeleteBitmap(picture);
            SetFlag(row, kFleetItemOwn, ship->ControlSide() == static_cast<uint32_t>(m_viewSide));
            scratch->FillRect(0, 0, kRowWidth, kRowHeight, 0);
            SelectObject(m_dc, scratch->DibSectionHandle());
            SelectObject(m_dc, m_pen);
            Polyline(m_dc, RowLine(kLineShipStub), 2);
            Polyline(m_dc, RowLine(kLineBend), 2);
            scratch->DrawKeyed(image, 0, 0);
            DrawFleetContentIcons(image, static_cast<int>(ship->ControlSide()), shipContents, 1);
            SetTextColor(m_dc, 0xffffff);
            row->m_text = NameOf(ships.Current());
            image->Blit(selected, 0, 0, 0, 0, 0, 0);
            marker->DrawKeyed(selected, 0, 0);
            SetFlag(row, kFleetItemHidden, !expanded);
            row->SetImages(image, selected);
            if (!*position) {
                m_fleetItems->Insert(row);
            } else {
                InsertShipRowBefore(row, *position);
            }
            ships.Advance();
        }
        contents |= shipContents;
    }

    if (contents != fleetRow->m_userData) {
        fleetRow->m_userData = contents;
        int skin = 0;
        DibBitmap* frame;
        DibBitmap* selectedFrame;
        if (fleet->ControlSide() == 1) {
            frame = LoadBitmap(art, 0x28a0);
            selectedFrame = LoadBitmap(art, 0x28a1);
        } else {
            skin = kSkinStep;
            selectedFrame = LoadBitmap(art, 0x28d3);
            frame = LoadBitmap(art, 0x28d2);
        }
        scratch->FillRect(0, 0, kRowWidth, kRowHeight, 0);
        fleetImage->FillRect(0, 0, fleetImage->Width(), fleetImage->Height(), frame->m_colorKey);
        SelectObject(m_dc, scratch->DibSectionHandle());
        SelectObject(m_dc, m_pen);
        if ((StateFlags(fleet) & kStateEnroute) != 0) {
            DibBitmap* badge = LoadBitmap(art, skin + 0x28b7);
            badge->DrawKeyed(fleetImage, 5, 5);
            DeleteBitmap(badge);
        }
        if ((StateFlags(fleet) & kStateHasOrders) != 0) {
            DibBitmap* badge = LoadBitmap(art, skin + 0x28b8);
            badge->DrawKeyed(fleetImage, 5, 5);
            DeleteBitmap(badge);
        }
        if (frame) {
            frame->DrawKeyed(fleetImage, 5, 5);
            DeleteBitmap(frame);
        }
        Polyline(m_dc, RowLine(kLineFleetStub), 2);
        scratch->DrawKeyed(fleetImage, 0, 0);
        DrawFleetContentIcons(fleetImage, static_cast<int>(fleet->ControlSide()), contents, 1);
        fleetImage->Blit(fleetSelected, 0, 0, 0, 0, 0, 0);
        selectedFrame->DrawKeyed(fleetSelected, 0, 0);
        DeleteBitmap(selectedFrame);
    }
    SelectObject(m_dc, previousBitmap);
    SelectObject(m_dc, previousPen);
    SelectObject(m_dc, previousPalette);
    DeleteBitmap(scratch);
    DeleteBitmap(marker);
}

// ---- The selection ----------------------------------------------------------------------------------------------

void FleetWindow::UpdateShipList() {
    HMODULE art = FindResourceModule(kBitmapSlot);
    FleetListItem* last = nullptr;
    bool hasOwn = false;
    bool changed = false;
    int selectedCount = 0;
    FleetListItem* row = static_cast<FleetListItem*>(m_fleetItems->First());
    m_shipItems->Clear();
    m_primaryTotal = 0;
    m_capacityTotal = 0;
    for (; row; row = static_cast<FleetListItem*>(row->Next())) {
        if ((row->m_state.m_value & kListItemSelected) == 0) continue;
        if ((row->m_flags.m_value & kFleetItemOwn) != 0) hasOwn = true;
        selectedCount++;
        last = row;
    }
    row = static_cast<FleetListItem*>(m_fleetItems->First());
    FleetListItem* lastOwn = last;
    while (row && hasOwn) {
        FleetListItem* keep = lastOwn;
        if ((row->m_state.m_value & kListItemSelected) != 0) {
            keep = row;
            if ((row->m_flags.m_value & kFleetItemOwn) == 0) {
                selectedCount--;
                row->m_state.m_value &= ~kListItemSelected;
                changed = true;
                keep = lastOwn;
            }
        }
        row = static_cast<FleetListItem*>(row->Next());
        lastOwn = keep;
    }
    last = lastOwn;
    if (changed) m_fleetList->Refresh();
    if (selectedCount < 2) {
        if (!last) {
            m_selectionLabel->SetText(nullptr);
        } else {
            if ((last->m_flags.m_value & kFleetItemFleet) == 0) {
                if ((m_uiFlags.m_value & kFleetShipSelected) == 0) m_uiFlags.m_value |= kFleetShipSelected;
            } else if ((m_uiFlags.m_value & kFleetShipSelected) != 0) {
                m_uiFlags.m_value &= ~kFleetShipSelected;
            }
            m_selectionLabel->SetText(last->m_text.m_text);
        }
        m_selectionLabel->Invalidate(m_hwnd, 0);
    } else {
        if ((m_uiFlags.m_value & kFleetShipSelected) != 0) m_uiFlags.m_value &= ~kFleetShipSelected;
        m_selectionLabel->SetText(nullptr);
        m_selectionLabel->Invalidate(m_hwnd, 0);
    }

    DibBitmap* source;
    int sourceX = 3;
    int sourceY = 0xd;
    if (!last) {
        source = m_paintBackground;
        sourceX = 100;
        sourceY = 0x2a;
    } else if ((last->m_flags.m_value & kFleetItemOwn) == 0) {
        source = m_infoBackdropOther;
    } else {
        source = m_infoBackdropOwn;
    }
    source->Blit(m_infoBitmap, 0, 0, sourceX, sourceY, 0, 0);
    InvalidateRect(m_hwnd, &g_fleetInfoRect, 0);
    if ((m_uiFlags.m_value & kFleetShipSelected) == 0) {
        if (selectedCount != 0) {
            int skin = SkinOffset(m_viewSide);
            if (!hasOwn) skin = kSkinStep - skin;
            DibBitmap* shape = LoadBitmap(art, skin + 0x28b9);
            const int left = (m_infoBitmap->Width() - shape->Width()) / 2;
            GameObjNotifier* fleet = GetObjectInFamilies_08_10(m_galaxy->m_side, &last->m_key);
            if (fleet) {
                if ((StateFlags(fleet) & kStateEnroute) != 0) {
                    DibBitmap* badge = LoadBitmap(art, skin + 0x28ba);
                    badge->DrawKeyed(m_infoBitmap, left, 0);
                    DeleteBitmap(badge);
                }
                if ((StateFlags(fleet) & kStateHasOrders) != 0) {
                    DibBitmap* badge = LoadBitmap(art, skin + 0x28bb);
                    badge->DrawKeyed(m_infoBitmap, left, 0);
                    DeleteBitmap(badge);
                }
            }
            if (shape) {
                if (shape->IsLoaded() != 0) shape->DrawKeyed(m_infoBitmap, left, 0);
                DeleteBitmap(shape);
            }
        }
    } else if (last) {
        if (GameObjNotifier* object = GetObjectInFamilies_01_ff(m_galaxySide, &last->m_key)) {
            ObjectKey key;
            object->GetKey(&key);
            DibBitmap* picture = CreateObjectPicture(m_gokres, &key, object, 1, 1);
            picture->DrawKeyed(m_infoBitmap, 0, 0);
            DeleteBitmap(picture);
        }
    }

    const COLORREF labelColor = (m_viewSide != 1 ? 0xfe01 : 0) + 0x20000ff;
    if (!hasOwn) {
        if (selectedCount < 1) {
            m_shipList->Hide();
            m_uiFlags.m_value &= ~(kFleetOwnSelected | kFleetOtherSelected);
        } else {
            m_shipList->Show(kShow);
            if ((m_uiFlags.m_value & kFleetOtherSelected) == 0) {
                const uint32_t flags = m_uiFlags.m_value;
                m_uiFlags.m_value = flags | kFleetOtherSelected;
                m_uiFlags.m_value = (flags & ~kFleetOwnSelected) | kFleetOtherSelected;
                m_shipList->SetBackground(m_infoBackdropOther, 4, 0x62);
            }
            m_selectionLabel->SetColor(labelColor, nullptr);
        }
    } else {
        m_shipList->Show(kShow);
        if ((m_uiFlags.m_value & kFleetOwnSelected) == 0) {
            const uint32_t flags = m_uiFlags.m_value;
            m_uiFlags.m_value = flags | kFleetOwnSelected;
            m_uiFlags.m_value = (flags & ~kFleetOtherSelected) | kFleetOwnSelected;
            m_shipList->SetBackground(m_infoBackdropOwn, 4, 0x62);
        }
        m_selectionLabel->SetColor(labelColor, nullptr);
    }

    UpdateTabs();
    for (row = static_cast<FleetListItem*>(m_fleetItems->First()); row; row = static_cast<FleetListItem*>(row->Next())) {
        if ((row->m_state.m_value & kListItemSelected) == 0) continue;
        if (GameObjNotifier* object = GetObjectInFamilies_01_ff(m_galaxySide, &row->m_key)) AddShipRows(object);
    }
    if (m_shipList) m_shipList->Refresh();
    if ((m_page == kPageFighters || m_page == kPageTroops) && selectedCount != 0) {
        RECT area = {1, 2, 0x7b, 0x2f};
        HDC windowDc = GetDC(m_hwnd);
        HDC dc = CreateCompatibleDC(windowDc);
        ReleaseDC(m_hwnd, windowDc);
        HGDIOBJ previousBitmap = SelectObject(dc, m_infoBitmap->DibSectionHandle());
        HPALETTE previousPalette = SelectPalette(dc, g_palette, 0);
        HGDIOBJ previousFont = SelectObject(dc, CoolGetFont(4));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, 0xffffff);
        StrStream stream;
        strstream_ctor(&stream, 1);
        ostream_operator_ls_long(stream.Out(), m_primaryTotal);
        ostream_operator_ls_char(stream.Out(), 0);
        DrawTextA(dc, strstreambuf_str(StreambufOf(&stream)), -1, &area, DT_SINGLELINE);
        strstreambuf_freeze(StreambufOf(&stream), 0);
        istream_seekg_pos(reinterpret_cast<istream*>(stream.m_bytes), 0);
        ostream_seekp_pos(stream.Out(), 0);
        ostream_operator_ls_long(stream.Out(), m_capacityTotal);
        ostream_operator_ls_char(stream.Out(), 0);
        DrawTextA(dc, strstreambuf_str(StreambufOf(&stream)), -1, &area, DT_SINGLELINE | DT_RIGHT);
        strstreambuf_freeze(StreambufOf(&stream), 0);
        SelectObject(dc, previousBitmap);
        SelectObject(dc, previousFont);
        SelectPalette(dc, previousPalette, 0);
        DeleteDC(dc);
        strstream_dtor(stream.Ios());
        ios_dtor(stream.Ios());
    }
}

void FleetWindow::UpdateTabs() {
    uint32_t selectedContents = 0;
    const uint32_t flags = m_uiFlags.m_value;
    int page = m_page;
    CoolTabControl* tabs;
    DibBitmap* backdrop;
    if ((flags & kFleetOwnSelected) == 0) {
        if ((flags & kFleetOtherSelected) == 0) {
            if ((flags & (kFleetOwnTabsShown | kFleetOtherTabsShown)) != 0) {
                m_uiFlags.m_value &= ~(kFleetOwnTabsShown | kFleetOtherTabsShown);
                m_ownTabs->Hide();
                m_otherTabs->Hide();
                InvalidateRect(m_hwnd, nullptr, 0);
            }
            return;
        }
        if ((flags & kFleetOtherTabsShown) == 0) {
            const uint32_t current = m_uiFlags.m_value;
            m_uiFlags.m_value = current | kFleetOtherTabsShown;
            m_uiFlags.m_value = (current & ~kFleetOwnTabsShown) | kFleetOtherTabsShown;
            InvalidateRect(m_hwnd, nullptr, 0);
            m_ownTabs->Hide();
        }
        tabs = m_otherTabs;
        backdrop = m_infoBackdropOther;
    } else {
        if ((flags & kFleetOwnTabsShown) == 0) {
            m_uiFlags.m_value = flags | kFleetOwnTabsShown;
            m_uiFlags.m_value = (flags & ~kFleetOtherTabsShown) | kFleetOwnTabsShown;
            m_otherTabs->Hide();
            InvalidateRect(m_hwnd, nullptr, 0);
        }
        tabs = m_ownTabs;
        backdrop = m_infoBackdropOwn;
    }
    if ((m_uiFlags.m_value & kFleetShipSelected) == 0) {
        CoolStrobeButton* shipsTab = static_cast<CoolStrobeButton*>(tabs->m_children.FindById(kPageShips));
        shipsTab->Enable();
        const uint32_t state = shipsTab->GetState();
        shipsTab->SetStateFlags(state & ~2u);
        shipsTab->Show(kShow);
    } else {
        if (page == kPageShips) page = kPageFighters;
        CoolStrobeButton* shipsTab = static_cast<CoolStrobeButton*>(tabs->m_children.FindById(kPageShips));
        shipsTab->SetStateFlags(2);
        shipsTab->Disable();
        shipsTab->Hide();
    }
    ChildWindowTree* tabButtons = &tabs->m_children;
    tabButtons->FindById(static_cast<uint32_t>(page));
    tabs->SelectTab(static_cast<uint32_t>(page), 0);
    m_page = page;
    for (FleetListItem* row = static_cast<FleetListItem*>(m_fleetItems->First()); row;
         row = static_cast<FleetListItem*>(row->Next())) {
        if ((row->m_state.m_value & kListItemSelected) != 0) selectedContents |= row->m_userData;
    }
    const struct {
        uint32_t bit;
        uint32_t tab;
    } kinds[] = {{kContentsFighters, kPageFighters}, {kContentsTroops, kPageTroops}, {kContentsPersonnel, kPagePersonnel}};
    for (const auto& kind : kinds) {
        CoolStrobeButton* tab = static_cast<CoolStrobeButton*>(tabButtons->FindById(kind.tab));
        if ((selectedContents & kind.bit) == 0) {
            tab->SetStateFlags(0x40);
        } else {
            tab->ClearStateFlags(0x40);
        }
    }
    if (backdrop) tabs->SetBackground(backdrop, tabs->m_x - 0x61, tabs->m_y - 0x1d);
    tabs->Show(kShow);
}

void FleetWindow::AddShipRows(GameObjNotifier* object) {
    ObjectKey key;
    object->GetKey(&key);
    if (FamilyIn(key, 8, 0x10)) {
        CapShipIterator ships(object, 3);
        for (ships.Begin(); ships.Current(); ships.Advance()) AddShipRows(ships.Current());
        return;
    }
    ObjectKey shipKey;
    object->GetKey(&shipKey);
    if (!FamilyIn(shipKey, 0x14, 0x1c)) return;
    ChildIterator* units = nullptr;
    CapShip* ship = static_cast<CapShip*>(object);
    if (m_page == kPageShips) {
        AddShipRow(object);
    } else if (m_page == kPageFighters) {
        units = NewOrNull<FightSquadIterator>(object, 3u);
        m_primaryTotal += ship->CountFighterSquadrons();
        m_capacityTotal += ship->GetFighterCapacity();
    } else if (m_page == kPageTroops) {
        units = NewOrNull<TroopRegIterator>(object, 3u);
        m_primaryTotal += ship->CountTroopRegiments();
        m_capacityTotal += ship->GetTroopCapacity();
    } else {
        units = NewOrNull<RoleIterator>(object, 3u);
    }
    if (units) {
        units->Begin();
        while (units->Current()) {
            AddShipRow(units->Current());
            units->Advance();
        }
        delete units;
    }
}

void FleetWindow::AddShipRow(GameObjNotifier* object) {
    ObjectKey key;
    object->GetKey(&key);
    bool found = false;
    FleetListItem* row = static_cast<FleetListItem*>(m_shipItems->First());
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    for (; row; row = static_cast<FleetListItem*>(row->m_next)) {
        if (found) return;
        found = row->m_key.m_value == key.m_value;
    }
    if (found) return;
    int skin = SkinOffset(m_viewSide);
    if ((m_uiFlags.m_value & kFleetOwnSelected) == 0) skin = kSkinStep - skin;
    DibBitmap* frame = LoadBitmap(art, skin + 0x28b4);
    FleetListItem* item = NewOrNull<FleetListItem>(&key);
    item->m_text = NameOf(object);
    DibBitmap* image = NewOrNull<DibBitmap>(0x7d, 0x32, frame);
    DibBitmap* selected = NewOrNull<DibBitmap>(0x7d, 0x32, frame);
    DibBitmap* picture = CreateObjectPicture(m_gokres, &key, nullptr, 0, 1);
    image->FillRect(0, 0, image->Width(), image->Height(), picture->m_colorKey);
    picture->Blit(image, 0x1c, 4, 0, 0, 0, 0);
    DeleteBitmap(picture);
    if (FamilyIn(key, 0x14, 0x1c)) {
        const uint32_t contents = FleetContents(object);
        DrawFleetContentIcons(image, static_cast<int>(object->ControlSide()), contents, 0);
    }
    image->Blit(selected, 0, 0, 0, 0, 0, 0);
    frame->DrawKeyed(selected, 1, 1);
    item->SetImages(image, selected);
    DeleteBitmap(frame);
    m_shipItems->Insert(item);
    ObjectKey objectKey;
    object->GetKey(&objectKey);
    if (!m_objects.Find(&objectKey)) {
        ObjectKeyIdNode* node = NewOrNull<ObjectKeyIdNode>();
        ObjectKey nodeKey;
        object->GetKey(&nodeKey);
        node->m_key = nodeKey;
        node->AddId(m_childId);
        m_objects.Add(node, 0);
    }
}

uint32_t REB_STDCALL FleetContents(GameObjNotifier* object) {
    FightSquadIterator squadrons(object, 3);
    TroopRegIterator regiments(object, 3);
    RoleIterator roles(object, 3);
    uint32_t contents = squadrons.Begin()->Current() != nullptr;
    if (regiments.Begin()->Current()) contents |= kContentsTroops;
    if (roles.Begin()->Current()) contents |= kContentsPersonnel;
    const uint32_t state = StateFlags(object);
    if ((state & kStateEnroute) != 0) contents |= kContentsEnroute;
    if ((state & kStateHasOrders) != 0) contents |= kContentsOrders;
    if ((state & kStateCompleted) == 0) contents |= kContentsIncomplete;
    if (static_cast<GameObj*>(object)->GetTransitSpeed(1) == 0) contents |= kContentsStationary;
    return contents;
}

void REB_STDCALL DrawFleetContentIcons(DibBitmap* target, int side, uint32_t contents, int compact) {
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    const int y = (compact != 0 ? 10 : 0) + 0x17;
    const int skin = SkinOffset(side);
    const struct {
        uint32_t bit;
        uint32_t picture;
        int x;
    } icons[] = {{kContentsFighters, 0x28a4, 0x19}, {kContentsTroops, 0x28a5, 0x29}, {kContentsPersonnel, 0x28a6, 0x39}};
    for (const auto& icon : icons) {
        if ((contents & icon.bit) == 0) continue;
        DibBitmap* bitmap = LoadBitmap(art, skin + icon.picture);
        bitmap->DrawKeyed(target, icon.x, y);
        DeleteBitmap(bitmap);
    }
    if ((contents & kContentsStationary) != 0) {
        DibBitmap* bitmap = LoadBitmap(art, skin + 0x28be);
        bitmap->DrawKeyed(target, 9, y);
        DeleteBitmap(bitmap);
    }
}
