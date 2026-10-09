#include <cstdint>
#include <new>

#include "ai/ai_common.h"
#include "app/player_controller_decl.h"
#include "app/rebapp_globals.h"
#include "audio/music.h"
#include "audio/sound_manager.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/vptr_store.h"
#include "data/dat_record.h"
#include "economy/side.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_type_db.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/panel_drag_button.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_game_window.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"
#include "ui/cool_timer.h"
#include "ui/custom_dialog_box.h"
#include "ui/object_list_item.h"
#include "ui/resource_dll.h"
#include "ui/sprite_stage.h"

extern const char g_strategyDllName[];  // 0x6a86a8 "strategy.dll"
extern const char g_textcommDllName[];  // 0x6a7edc "textcomm.dll"


namespace {

constexpr uint32_t kArtSlot = 7;
constexpr uint32_t kTextCommSlot = 1;
constexpr uint32_t kBitmapResourceKind = 2;
constexpr uint32_t kMouseModeRestoring = 4;
constexpr int kMinimizedSlotCount = 12;
constexpr int kStockNullBrush = 5;
constexpr int kStockWhitePen = 6;
constexpr int kZoomSteps = 4;
constexpr DWORD kZoomStepMs = 0x32;
constexpr uint32_t kEncyclopediaIdBase = 0x1000;
constexpr uint32_t kEncyclopediaIdMask = 0xfff;
constexpr int kEncyclopediaTitleBias = 0x8000;

void DeleteBitmap(DibBitmap* bitmap) {
    if (!bitmap) return;
    bitmap->~DibBitmap();
    ::operator delete(bitmap);
}

void DeleteStage(SpriteStage*& stage) {
    if (!stage) return;
    stage->~SpriteStage();
    ::operator delete(stage);
    stage = nullptr;
}

void PlayMinimizeSound(int side, int firstSoundId) {
    SoundRequest request(0, 0, 0, 0, 0, 0);
    request.m_kind = 1;
    request.m_soundId = (firstSoundId - (side != 1)) & 0xffff;
    request.m_moduleIndex = 7;
    SoundManager::Play(&request);
}

bool IsInRange(uint32_t value, uint32_t first, uint32_t end) { return value >= first && value < end; }

// Which records the encyclopedia lists: the unit, facility and character families, and the 0x50-0x7f families
// whose record does not carry the "hidden" value at +0x5c.
bool IsListedRecord(const ObjectKey* key) {
    const uint32_t family = key->Family();
    bool listed = IsInRange(family, 0x14, 0x20) || IsInRange(family, 0x28, 0x30) || IsInRange(family, 0x22, 0x28) ||
                  IsInRange(family, 0x10, 0x14) || IsInRange(family, 0x30, 0x40) || IsInRange(family, 8, 0x10);
    if (IsInRange(family, 0x50, 0x80)) {
        DatRecord* record = FindDataRecord(AsLookupKey(key));
        if (record) listed = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(record) + 0x5c) == 0;
    }
    return listed;
}

ObjectListItem* NewEncyclopediaItem(uint32_t id, const ObjectKey* key, const String& title) {
    void* memory = ::operator new(sizeof(ObjectListItem));
    ObjectListItem* item = memory ? new (memory) ObjectListItem(id, key) : nullptr;
    item->m_text = title.m_text;
    return item;
}

void LoadEncyclopediaTitle(String* title, uint32_t recordBits) {
    title->Clear();
    const ResourceStringId resource{static_cast<int16_t>(static_cast<int16_t>(recordBits) - kEncyclopediaTitleBias),
                                    static_cast<int16_t>(recordBits >> 16)};
    title->SetFromResource(&resource);
}

}  // namespace

IntPair* __thiscall Side::GetMineStateCopy(IntPair* out) {
    new (out) IntPair(m_mineState);
    return out;
}

IntPair* __thiscall Side::GetRefineryStateCopy(IntPair* out) {
    new (out) IntPair(m_refineryState);
    return out;
}

// ---- Construction and destruction --------------------------------------------------------------------------

GalaxyWindow::GalaxyWindow(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, int side,
                           int restoring)
    : GameWindowObject(instance, x, y, width, height, parent, nullptr, 0),
      m_dateLabel(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0, 0),
      m_resourceLabels{CoolLabel(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0, 0),
                       CoolLabel(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0, 0),
                       CoolLabel(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0, 0)},
      m_latencyLabel(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0, 0),
      m_sectorItems(1),
      m_sectorLabel(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0, 0),
      m_systemItems(1) {
    m_titleStringId = ResourceStringId{0, 0};
    m_created = 0;
    m_style |= 0x6000000;
    m_cursor = nullptr;
    m_side = side;
    m_restoring = restoring;
    m_musicPending = 1;
    m_alternateDisplayMode = 0;
    m_speed = -1;
    m_optionsRequested = 0;
    for (CoolStrobeButton*& button : m_consoleButtons) button = nullptr;
    m_pendingMovie = 0;
    for (CoolFrameButton*& button : m_messageButtons) button = nullptr;
    m_inModalLoop = 0;
    for (int kind = 0; kind < 4; ++kind) {
        m_gidIconIds[0][kind] = 0;
        m_gidIconIds[1][kind] = 0;
        m_gidIconIds[2][kind] = 0;
    }
    m_gidIcons = nullptr;
    m_gidIcons2 = nullptr;
    *reinterpret_cast<int*>(&m_titleStringId) = 0;
    m_gidMenu = nullptr;
    m_gidLegendIcon = nullptr;
    m_gidLegend = nullptr;
    m_legendX = 0x96;
    m_legendY = 0x78;
    m_unknownButton150 = nullptr;
    m_hoveredSector = nullptr;
    if (m_side == 1) {
        SetRect(&m_panelArea, 0x37, 0x28, 0x21c, 0x186);
    } else {
        SetRect(&m_panelArea, 0x78, 0x28, 600, 0x18b);
    }
    m_minimizedCount = 0;
    for (MinimizedWindowSlot& slot : m_minimized) {
        slot.m_icon = nullptr;
        slot.m_iconId = 0;
        slot.m_area = RECT{0, 0, 0, 0};
        slot.m_childId = 0;
    }
    static constexpr int kAllianceSlotTops[11][2] = {{0x3d, 0x4f},   {0x53, 0x65},   {0x69, 0x7b},  {0x7f, 0x91},
                                                     {0x95, 0xa7},   {0xab, 0xbd},   {0xc1, 0xd3},  {0xd6, 0xe8},
                                                     {0xec, 0xfe},   {0x102, 0x114}, {0x117, 0x129}};
    static constexpr int kEmpireSlotTops[11][2] = {{0x30, 0x42},  {0x48, 0x5a},  {0x62, 0x74},   {0x7a, 0x8c},
                                                   {0x93, 0xa5},  {0xac, 0xbe},  {0xc4, 0xd6},   {0xdd, 0xef},
                                                   {0xf5, 0x107}, {0x10e, 0x120}, {0x127, 0x139}};
    const bool alliance = m_side == 1;
    const int slotLeft = alliance ? 0x220 : 0x15;
    const int slotRight = alliance ? 0x25e : 0x4b;
    const int (*slotTops)[2] = alliance ? kAllianceSlotTops : kEmpireSlotTops;
    for (int i = 0; i < 11; ++i) SetRect(&m_minimized[i].m_area, slotLeft, slotTops[i][0], slotRight, slotTops[i][1]);
    SetRect(&m_minimized[11].m_area, slotLeft, alliance ? 0x12d : 0x13f, slotRight, alliance ? 0x13f : 0x151);
    g_galaxyWindow = this;
    m_activeChild = nullptr;
    m_previousActiveChild = nullptr;
    m_mouseMode = 1;
    m_pendingCommand = nullptr;
    m_unknownAc = 0;
    m_currentCursor = nullptr;
    m_targetCursor = nullptr;
    m_offscreenDc = nullptr;
    m_droidStage = nullptr;
    m_droidStage2 = nullptr;
    m_tickTimer = nullptr;
    m_encyclopediaItems = nullptr;
    m_displayMode = 0x80;
    m_unknown470 = 0;
    char* dllLocation = GetDllLocation();
    LoadResourceModule(dllLocation, g_strategyDllName, kArtSlot);
    HMODULE textComm = LoadResourceModule(dllLocation, g_textcommDllName, kTextCommSlot);
    HACCEL accelerators = LoadAcceleratorsA(textComm, MAKEINTRESOURCEA(0xb));
    if (accelerators != nullptr && g_app != nullptr) {
        g_app->m_unknown98 = static_cast<int>(reinterpret_cast<uintptr_t>(accelerators));
    }
    if (dllLocation) ::operator delete(dllLocation);
}

GalaxyWindow::~GalaxyWindow() {
    REB_KEEP_VPTR_STORE();
    UiSetSessionViewReady(0);
    for (int i = 0; i < 7; ++i) delete m_consoleButtons[i];
    delete m_gidMenu;
    delete m_gidLegend;
    delete m_unknownButton150;
    delete m_gidLegendIcon;
    DeleteBitmap(m_offscreen);
    if (m_offscreenDc) DeleteDC(m_offscreenDc);
    for (CoolFrameButton* button : m_messageButtons) delete button;
    if (m_pendingMovie) UiFinishSync(reinterpret_cast<PlayerMessage*>(m_pendingMovie));
    if (m_encyclopediaItems) {
        m_encyclopediaItems->Clear();
        delete m_encyclopediaItems;
    }
    DeleteBitmap(m_gidIcons);
    DeleteBitmap(m_gidIcons2);
    if (g_galaxyWindow == this) {
        g_galaxyWindow = nullptr;
        SoundManager::Shutdown(0);
    }
    delete static_cast<PlayerMessage*>(m_pendingCommand);
    if (m_tickTimer) {
        m_tickTimer->Stop();
        delete m_tickTimer;
    }
    DeleteStage(m_droidStage);
    DeleteStage(m_droidStage2);
    UnloadSpriteDll();
    g_resourceDlls.UnloadSlot(kArtSlot);
}

// ---- Small accessors ---------------------------------------------------------------------------------------

GalaxyWindow* GetGalaxyWindow() { return g_galaxyWindow; }

void GalaxyWindow::StartMusic() {
    if (!m_musicPending) return;
    m_musicPending = 0;
    StartMusicTrack(UiHumanNextAdvice());
}

void GalaxyWindow::SetAlternateDisplayMode(uint32_t flags) {
    int mode = 0;
    if (flags & 0x100000) {
        m_alternateDisplayMode = (m_side == 2) + 0x11;
        return;
    }
    if (flags & 0x1000000) mode = (m_side != 1 ? 3 : 0) + 0x10;
    m_alternateDisplayMode = mode;
}

void GalaxyWindow::SetPendingMovie(LPARAM movie) {
    m_pendingMovie = movie;
    PostMessageA(m_hwnd, kGalaxyPlayMovie, 1, movie);
}

int GalaxyWindow::FindMinimizedSlot(uint32_t childId) {
    for (int i = 0; i < kMinimizedSlotCount; ++i) {
        if (m_minimized[i].m_childId == childId) return i + 1;
    }
    return 0;
}

void GalaxyWindow::SetResourceCounter(int counter, int value, int unused) {
    (void)unused;
    CoolLabel* label = nullptr;
    String text;
    text.AppendSigned(value);
    if (counter == kGalaxyRawMaterials) {
        label = &m_resourceLabels[0];
    } else if (counter == kGalaxyRefinedMaterials) {
        label = &m_resourceLabels[1];
    } else if (counter == kGalaxyMaintenance) {
        label = &m_resourceLabels[2];
        if (value > 99999) {
            const ResourceStringId overflow{0x1302, 2};
            text.SetFromResource(&overflow);
        }
    }
    if (label) {
        HDC dc = GetDC(m_hwnd);
        label->ReplaceText(m_hwnd, dc, text.m_text);
        ReleaseDC(m_hwnd, dc);
    }
}

// ---- Game display setup ------------------------------------------------------------------------------------

void GalaxyWindow::InitializeGameDisplay() {
    Side* side = static_cast<Side*>(GetSideObject(AppGetSide(), AppGetSide()));
    BuildSectorItems(CoolAppGetResourceModule(kArtSlot));

    IntPair state;
    side->GetMineStateCopy(&state);
    SetResourceCounter(kGalaxyRawMaterials, side->m_rawMaterials, 0);
    side->GetRefineryStateCopy(&state);
    SetResourceCounter(kGalaxyRefinedMaterials, side->m_refinedMaterials, 0);
    GetSidePairAt58(side, &state);
    IntPair maintenance;
    GetSidePairAt58(side, &maintenance);
    SetResourceCounter(kGalaxyMaintenance, static_cast<int>(maintenance.m_first) - side->m_maintRequiredB,
                       static_cast<int>(state.m_first));

    if (!m_encyclopediaItems) {
        ObjectRefList records;
        ListAllDataRecordKeys(&records);
        void* memory = ::operator new(0x10);
        m_encyclopediaItems = memory ? new (memory) CoolListItemList(2) : nullptr;
        for (AvlNode* node = records.First(); node; node = node->m_successor) {
            const ObjectKey* recordKey = &static_cast<ObjectRefNode*>(node)->m_key;
            if (!IsListedRecord(recordKey)) continue;
            DatRecord* record = FindDataRecord(AsLookupKey(recordKey));
            const uint32_t recordBits = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(record) + 0x30);
            const uint32_t itemId = (recordBits & kEncyclopediaIdMask) + kEncyclopediaIdBase;
            if (m_encyclopediaItems->FindById(itemId)) continue;
            String title;
            LoadEncyclopediaTitle(&title, recordBits);
            if (title.Length() == 0) title.Assign(record->m_name);
            ObjectKey itemKey;
            itemKey.m_value = (recordKey->Family() << 24) | (itemKey.m_value & 0xffffff);
            m_encyclopediaItems->Insert(NewEncyclopediaItem(itemId, &itemKey, title));
        }
        FamilyRange systemFamilies(0x90, 0x98);
        for (GameObjNotifier* system = GetFirstObjectInFamilies_90_98(m_side); system;
             system = static_cast<GameObj*>(system)->FindNextSibling(&systemFamilies, 1)) {
            const uint32_t recordBits = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(system->m_record) + 0x30);
            const uint32_t itemId = (recordBits & kEncyclopediaIdMask) + kEncyclopediaIdBase;
            if (m_encyclopediaItems->FindById(itemId)) continue;
            String title;
            LoadEncyclopediaTitle(&title, recordBits);
            if (title.Length() == 0) title.Assign(*static_cast<GameObj*>(system)->GetName());
            ObjectKey itemKey;
            GetObjectKey(system, &itemKey);
            m_encyclopediaItems->Insert(NewEncyclopediaItem(itemId, &itemKey, title));
        }
    }
    m_inModalLoop = 0;
    EnableControls();
}
