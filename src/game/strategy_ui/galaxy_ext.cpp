#include <string>
#include "core/reb_x87.h"
#include <cstdint>

#include "ai/ai_common.h"
#include "app/app_cdcheck_version.h"
#include "app/game_session.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/stream_io.h"
#include "galaxy/sector.h"
#include "galaxy/system.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "strategy_ui/battle_in_progress_window.h"
#include "strategy_ui/fleet_window.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/sector_window.h"
#include "strategy_ui/system_forces_window.h"
#include "strategy_ui/system_missions_window.h"
#include "strategy_ui/system_window.h"
#include "ui/cool_game_window.h"
#include "ui/cool_label.h"
#include "ui/object_list_item.h"

extern const char *g_newlineString;  // 0x6a8170 "\n"
extern const int16_t g_mapGridWidth;   // 0x659b48
extern const int16_t g_mapGridHeight;  // 0x659b4a
extern const int16_t g_stringGroup65d424;  // 0x65d424 group of the localised sector name suffix

namespace {

constexpr uint32_t kStyleVisible = 0x10000000;
constexpr int kMouseModeRestoring = 4;
constexpr uint32_t kDisplayModeCommand = 0x140;
constexpr uint32_t kWindowTypeMask = 0x3f;
constexpr uint32_t kSectorWindowType = 1;
constexpr uint32_t kDisplayModeMask = 0xff;

enum SavedWindowType : uint32_t {
    kTypeSector = 1,
    kTypeFleetFinderAlt = 4,
    kTypeSide = 7,
    kTypeSystem = 9,
    kTypeTen = 10,
    kTypeEleven = 11,
};

int GridScaled(int value, int gridSize, double scale) {
    return reb_ftol(static_cast<double>(value) / static_cast<double>(gridSize + 1) * scale);
}

void AddSideOffset(int side, int* x, int* y) {
    if (side == 1) {
        *x += 0x15;
        *y += 0x19;
    } else {
        *x += 0x54;
        *y += 0x1b;
    }
}

}  // namespace

void GalaxyWindow::SaveState(ostream* out) {
    uint32_t packed = (static_cast<uint8_t>(m_alternateDisplayMode) << 8) | static_cast<uint8_t>(m_displayMode);
    WriteValue16(out, reinterpret_cast<const uint16_t*>(&packed));
    uint32_t count = m_children.Count();
    WriteValue16(out, reinterpret_cast<const uint16_t*>(&count));
    for (HWND hwnd = LastChildWindow(); hwnd; hwnd = GetWindow(hwnd, GW_HWNDPREV)) {
        if (CoolWindow* child = FindChildByHwnd(hwnd)) SaveChildWindow(out, child);
    }
}

HWND GalaxyWindow::LastChildWindow() {
    HWND window = GetTopWindow(m_hwnd);
    HWND last = nullptr;
    while (window) {
        HWND next = GetWindow(window, GW_HWNDNEXT);
        last = window;
        window = next;
    }
    return last;
}

CoolWindow* GalaxyWindow::FindChildByHwnd(HWND hwnd) {
    CoolWindow* found = nullptr;
    for (AvlNode* node = m_children.First(); node; node = node->m_successor) {
        found = static_cast<CoolWindow*>(node);
        if (found->m_hwnd == hwnd) return found;
    }
    return nullptr;
}

void GalaxyWindow::LoadState(istream* in) {
    m_mouseMode |= kMouseModeRestoring;
    // BUG(original): only the low half is read; the upper half keeps whatever the stack slot held, here the object address.
    uint32_t packed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    ReadValue16(in, reinterpret_cast<uint16_t*>(&packed));
    const uint32_t modeBits = packed & 0xffff;
    packed &= kDisplayModeMask;
    m_alternateDisplayMode = modeBits >> 8;
    uint32_t count = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(in));
    ReadValue16(in, reinterpret_cast<uint16_t*>(&count));
    for (uint16_t i = 0; i < static_cast<uint16_t>(count); ++i) LoadChildWindow(in);
    InitializeGameDisplay();
    if (static_cast<int16_t>(packed) != static_cast<int16_t>(m_displayMode)) {
        SendMessageA(m_hwnd, WM_COMMAND, (static_cast<int>(packed) << 16) | kDisplayModeCommand, 0);
    }
    m_previousActiveChild = nullptr;
    for (AvlNode* node = m_children.First(); node; node = node->m_successor) {
        CoolWindow* child = static_cast<CoolWindow*>(node);
        if ((child->m_childId & kWindowTypeMask) == kSectorWindowType)
            SetWindowPos(child->m_hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    }
    if (m_gidLegendIcon) SetWindowPos(m_gidLegendIcon->m_hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    m_mouseMode &= ~static_cast<uint32_t>(kMouseModeRestoring);
    UiSetHumanDispatchFlag(0);
}

void GalaxyWindow::SaveChildWindow(ostream* out, CoolWindow* window) {
    ChildWindowState state(window);
    if (state.m_valid) {
        state.Write(out);
        static_cast<CoolGameWindow*>(window)->SaveState(out);
    }
}

CoolWindow* GalaxyWindow::LoadChildWindow(istream* in) {
    ChildWindowState state;
    state.Read(in);
    return OpenChildWindow(in, &state);
}

CoolWindow* GalaxyWindow::OpenChildWindow(istream* in, ChildWindowState* state) {
    CoolWindow* window = nullptr;
    bool tracked = false;
    bool minimizable = true;
    if (state) {
        switch (state->m_type) {
        case kTypeSector: {
            void* memory = ::operator new(sizeof(SectorWindow));
            window = memory ? static_cast<CoolWindow*>(new (memory) SectorWindow(m_instance, state->m_x, state->m_y, state->m_width, state->m_height, this, GetBackground(), 0, nullptr))
                            : nullptr;
            tracked = true;
            minimizable = false;
            break;
        }
        case kTypeFleetFinderAlt: {
            void* memory = ::operator new(sizeof(FleetWindow));
            if (memory) window = static_cast<CoolWindow*>(new (memory) FleetWindow(m_instance, state->m_x, state->m_y, this, 0, nullptr));
            tracked = true;
            break;
        }
        case kTypeSide: {
            void* memory = ::operator new(sizeof(BattleInProgressWindow));
            window = memory ? static_cast<CoolWindow*>(new (memory) BattleInProgressWindow(m_instance, this, 0)) : nullptr;
            minimizable = false;
            break;
        }
        case kTypeSystem: {
            void* memory = ::operator new(sizeof(SystemWindow));
            tracked = true;
            window = memory ? static_cast<CoolWindow*>(new (memory) SystemWindow(m_instance, state->m_x, state->m_y, state->m_width, state->m_height, this, 0, nullptr))
                            : nullptr;
            break;
        }
        case kTypeTen: {
            void* memory = ::operator new(sizeof(SystemForcesWindow));
            if (memory) {
                window = static_cast<CoolWindow*>(new (memory) SystemForcesWindow(
                    m_instance, state->m_x, state->m_y, state->m_width, state->m_height, this, 0, nullptr));
            }
            tracked = true;
            break;
        }
        case kTypeEleven: {
            void* memory = ::operator new(sizeof(SystemMissionsWindow));
            if (memory) window = static_cast<CoolWindow*>(new (memory) SystemMissionsWindow(m_instance, state->m_x, state->m_y, this, 0, nullptr));
            tracked = true;
            break;
        }
        default:
            break;
        }
    }
    if (window) {
        static_cast<CoolGameWindow*>(window)->LoadState(in);
        window->m_style &= ~kStyleVisible;
        window->Create(0);
        m_children.Insert(window);
        if (tracked) TrackChildWindow(window);
        if (state->m_shown == 0 && minimizable) {
            window->Hide();
            MinimizeChildWindow(window);
        } else {
            static_cast<CoolGameWindow*>(window)->ShowAndTrack(5);
        }
    }
    return window;
}

void GalaxyWindow::Virtual08(int command, int unused1, int unused2) {
    ObjectKey none;
    ObjectKeyValueSet selection;
    UiHumanCommand(command, &selection, &none, nullptr);
}

void GalaxyWindow::BuildSectorItems(HMODULE module) {
    std::string text;
    const ResourceStringId suffixId = {0x173b, 2};
    std::string suffix = LoadStringResource(&suffixId);
    m_sectorLabel.SetFont(5);
    m_sectorLabel.m_drawFlags = 0;
    m_sectorLabel.SetColor(0x200f0f0, nullptr);
    for (GameObjNotifier* sector = GetFirstObjectInFamilies_80_90(m_side); sector;) {
        Sector* sectorObject = static_cast<Sector*>(sector);
        uint32_t position;
        sectorObject->GetPosition(&position);
        if (GetVersionCheckResult() == 0) {
            const ResourceStringId groupId = {0x4da0, g_stringGroup65d424};
            std::string check = LoadStringResource(&groupId);
            if (check.empty()) {
                std::string name;
                static_cast<GameObj*>(sector)->GetNameCopy(&name);
                text = name;
                std::string newline = StringFromText(g_newlineString);
                text += newline;
                text += suffix;
            } else {
                text = suffix;
                std::string newline = StringFromText(g_newlineString);
                text += newline;
                std::string name;
                static_cast<GameObj*>(sector)->GetNameCopy(&name);
                text += name;
            }
        } else {
            std::string name;
            static_cast<GameObj*>(sector)->GetNameCopy(&name);
            text = name;
            text += suffix;
        }
        int x = GridScaled(static_cast<int16_t>(position), g_mapGridWidth, 607.0);
        int y = GridScaled(static_cast<int16_t>(position >> 16), g_mapGridWidth, 437.0);
        AddSideOffset(m_side, &x, &y);
        const int top = y;
        ObjectKey key;
        GetObjectKey(sector, &key);
        ObjectListItem* item = NewOrNull<ObjectListItem>(&key);
        item->m_x = x;
        item->m_y = y;
        item->m_text = text.c_str();
        int maxX = x;
        int maxY = y;
        {
            SystemIterator systems(sector, 4);
            for (systems.Begin(); systems.Current(); systems.Advance()) {
                uint32_t systemPosition;
                static_cast<System*>(systems.Current())->GetRecordField4c(&systemPosition);
                int sx = GridScaled(static_cast<int16_t>(systemPosition), g_mapGridWidth, 607.0);
                int sy = GridScaled(static_cast<int16_t>(systemPosition >> 16), g_mapGridHeight, 437.0);
                AddSideOffset(m_side, &sx, &sy);
                if (sx > maxX) maxX = sx;
                if (sy > maxY) maxY = sy;
            }
            RECT area;
            SetRect(&area, x, top, maxX + 8, maxY + 8);
            item->m_rect = area;
            m_sectorItems.Insert(item);
        }
        const FamilyRange sectors(0x80, 0x90);
        sector = static_cast<GameObj*>(sector)->FindNextSibling(&sectors, 1);
    }
}

void SetDword30(void* object, int value) { *reinterpret_cast<int*>(static_cast<uint8_t*>(object) + 0x30) = value; }

const char *g_newlineString = nullptr;
const int16_t g_mapGridHeight = 1023;
const int16_t g_mapGridWidth = 1023;
