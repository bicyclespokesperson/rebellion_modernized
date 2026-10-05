#include <new>

#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/file_util.h"
#include "core/game_string.h"
#include "core/iostream_lib.h"
#include "core/vptr_store.h"
#include "events/game_clock_hooks.h"
#include "gameobj/object_lookup.h"
#include "orders/order_base.h"
#include "strategy_ui/advisor_decl.h"
#include "strategy_ui/advisor_droid.h"
#include "strategy_ui/advisor_sides.h"
#include "strategy_ui/game_ui_facade.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));
extern "C" void __cdecl LAB_004c3740();

extern IdAvlTree g_allianceChoices;      // 0x6b2928 the list the display-mode switches show
extern uint32_t g_allianceChoicesGuard;  // 0x6b2934

using namespace advisor_sides;

namespace {

constexpr int kCueCount = 0x58;
constexpr unsigned kGoodbyeCueOffset = 0xa4;
constexpr int kRcdataScriptDirectory = 0x618;
constexpr int kRcdataScriptName = 0x61b;
constexpr uint32_t kUiFlagChatty = 0x1000;
constexpr uint32_t kUiFlagIdleRequested = 0x2000;
constexpr uint32_t kUiFlagSleep = 0x4000000;
constexpr uint32_t kUiFlagWake = 0x8000000;

IdAvlTree* ChoicesTree() {
    if ((g_allianceChoicesGuard & 1) == 0) {
        g_allianceChoicesGuard |= 1;
        new (&g_allianceChoices) IdAvlTree();
        atexit(LAB_004c3740);
    }
    return &g_allianceChoices;
}

void AddChoice(IdAvlTree* tree, uint32_t id) {
    IdNode* node = NewOrNull<IdNode>(id);
    tree->AddWithId(node);
}

void ShowChoices(int mode, IdAvlTree* tree, int16_t titleId) {
    const ResourceStringId title = {titleId, 2};
    UiSetDisplayModeTitled(mode, reinterpret_cast<LPARAM>(tree), &title);
}

GameObjNotifier* FindSystemAbove(GameObjNotifier* object) {
    ObjectKey key;
    for (GameObjNotifier* parent = object->m_parent; parent; parent = parent->m_parent) {
        if (KeyIn(*parent->GetKey(&key), 0x90, 0x98)) return parent;
    }
    return nullptr;
}

void ShowFirstSystemChoice(AllianceAdvisor* advisor, IdAvlTree* tree, ObjectKey key, int16_t titleId, int mode) {
    GameObjNotifier* object = GetObjectInFamilies_01_ff(advisor->m_side, &key);
    if (object) {
        GameObjNotifier* system = FindSystemAbove(object);
        if (system) {
            ObjectKey systemKey;
            AddChoice(tree, system->GetKey(&systemKey)->Id());
        }
    }
    ShowChoices(mode, tree, titleId);
}

}  // namespace

AllianceAdvisor::AllianceAdvisor(int side, HumanPlayer* player) : AdvisorBase(side, player) {
    g_humanUiFlags.m_value |= kUiFlagChatty;
    m_cueCount = kCueCount;
    m_cueDue = static_cast<uint32_t*>(::operator new(0x160));
    m_cueCooldown = static_cast<uint32_t*>(::operator new(m_cueCount << 2));
    for (uint32_t i = 0; i < m_cueCount; ++i) {
        m_cueDue[i] = 0;
        m_cueCooldown[i] = 0;
    }
    CueDue(kGoodbyeCueOffset) = kNeverTicks;
}

AllianceAdvisor::~AllianceAdvisor() {
    ::operator delete(m_cueDue);
    REB_KEEP_VPTR_STORE();
}

void AllianceAdvisor::HandleRefusedOrder(OrderBase* order, IntPair* status) {
    if (m_enabled == 0) return;
    if (status->m_first == m_lastStatus.m_first && status->m_second == m_lastStatus.m_second) {
        ToggleRepeatedStatus(status);
        return;
    }
    m_lastStatus = *status;
    m_statusRepeat = 0;
    const uint32_t category = status->m_first;
    if (category >= 0x90 && category < 0x98) {
        RefusedSystem(order, status);
    } else if (category >= 0xa0 && category < 0xb0) {
        RefusedManager(order, status);
    } else if (category >= 8 && category < 0x10) {
        RefusedFleet(order, status);
    } else if (category >= 0x14 && category < 0x1c) {
        RefusedShip(order, status);
    } else if (category == 0xf3) {
        RefusedSide(order, status);
    } else {
        RefusedOther(order, status);
    }
}

void AllianceAdvisor::HandleUiFlags(unsigned flags) {
    if ((flags & 0x100000) != 0) {
        CueDue(0x15c) = g_advisorNow + kNeverTicks;
        UiMarkGalaxyViewUnknownAc();
        return;
    }
    if ((flags & 0x1000000) != 0) {
        CueDue(0x158) = g_advisorNow + kNeverTicks;
        UiMarkGalaxyViewUnknownAc();
    }
}

namespace {

void PlayEntry(const DroidScriptEntry* entry) {
    UiPlayDroidScript(entry->m_id, entry->m_x, entry->m_y, (entry->m_flags >> 1) & 1);
}

}  // namespace

int AllianceAdvisor::Update() {
    bool played = false;
    uint32_t cue = 1;
    g_advisorNow = GameHookTickCount();
    if (g_advisorPendingMode != 0) {
        RunPendingCommand();
        return 1;
    }
    g_advisorPendingMode = 0;
    while (g_advisorMaySpeak != 0) {
        if (played) {
            g_advisorLastRun = g_advisorNow;
            return 1;
        }
        played = false;
        if (m_cueCount <= cue) break;
        uint32_t* due = &m_cueDue[cue];
        if (*due == 0 || *due < g_advisorNow) {
            *due = 0;
            ++cue;
            continue;
        }
        *due = 0;
        if (m_cueCooldown[cue] < g_advisorNow) {
            m_cueCooldown[cue] = g_advisorNow + 0x3c;
            played = true;
            DroidScript* script = static_cast<DroidScript*>(Cues()->FindById(cue));
            if (script) {
                while (DroidScriptEntry* entry = script->NextEntry()) {
                    if ((entry->m_flags & 1) == 0) {
                        PlayEntry(entry);
                    } else if ((g_humanUiFlags.m_value & kUiFlagChatty) != 0 && (g_humanUiFlags.m_value & 0x4000) == 0) {
                        PlayEntry(entry);
                    }
                }
            }
        }
    }
    if (played) {
        g_advisorLastRun = g_advisorNow;
        return 1;
    }
    ProcessIdle();
    return 1;
}

int AllianceAdvisor::LoadCueScripts() {
    ScopedIfstream file;
    char* dataLocation = GetDataLocation();
    ScopedStrstream name;
    ostream* out = iostream_access::AsOstream(name.get());
    if (dataLocation) {
        ostream_operator_ls_cstr(ostream_operator_ls_cstr(out, dataLocation), "\\");
        ::operator delete(dataLocation);
    }
    ostream* afterDirectory = ostream_operator_ls_cstr(out, LoadRcdataString(kRcdataScriptDirectory));
    ostream* afterName = ostream_operator_ls_cstr(afterDirectory, LoadRcdataString(kRcdataScriptName));
    ostream_operator_ls_char(afterName, 0);
    OpenInputFile(file.get(), strstreambuf_str(iostream_access::BufferOf(name.get())));
    strstreambuf_freeze(iostream_access::BufferOf(name.get()), 0);
    const int error = GetFileError();
    if (error == 0) {
        FUN_0049c6f0(m_cueTree, reinterpret_cast<istream*>(file.get()));
        CloseFileStream(file.get());
    }
    return error == 0;
}

void AllianceAdvisor::ProcessIdle() {
    g_advisorNow = GameHookTickCount();
    if (m_enabled == 0) {
        if ((g_humanUiFlags.m_value & kUiFlagWake) != 0) {
            g_advisorNagCount = 0;
            CueDue(0x88) = g_advisorNow + kQuickCue;
            g_humanUiFlags.m_value &= ~kUiFlagWake;
            m_enabled = 1;
        }
        g_humanUiFlags.m_value &= 0xc000;
        return;
    }
    g_advisorMaySpeak = 0;
    uint32_t& flags = reinterpret_cast<uint32_t&>(g_humanUiFlags.m_value);
    if ((flags & kUiFlagSleep) != 0) {
        CueDue(0x84) = g_advisorNow + kNeverTicks;
        flags &= ~kUiFlagSleep;
        m_enabled = 0;
        ResetTasks();
        return;
    }
    if ((flags & kUiFlagIdleRequested) != 0) {
        CueDue(0xa0) = g_advisorNow + kNeverTicks;
        UiBeginGameInput();
        flags &= ~kUiFlagIdleRequested;
        return;
    }
    if ((flags & 1) != 0) {
        CueDue(0x8c) = g_advisorNow + kShortCue;
        flags &= ~1u;
        return;
    }
    if ((flags & 0x10) != 0) {
        CueDue(0x90) = g_advisorNow + kShortCue;
        flags &= ~0x10u;
        return;
    }
    if ((flags & 0x20) != 0) {
        CueDue(0x98) = g_advisorNow + kShortCue;
        flags &= ~0x20u;
        return;
    }
    if ((flags & 0x100) != 0) {
        CueDue(0x94) = g_advisorNow + kLongCue;
        flags &= ~0x100u;
        return;
    }
    if ((flags & 0x200) != 0) {
        CueDue(0x9c) = g_advisorNow + kLongCue;
        flags &= ~0x200u;
        return;
    }
    if ((flags & 0x1000000) != 0) {
        CueDue(0x13c) = g_advisorNow + kQuickCue;
        flags &= ~0x1000000u;
        return;
    }
    if ((flags & 0x2000000) != 0) {
        uint32_t cue = g_advisorNagCount + 0x4a;
        if (cue < 0x4c) {
            cue = 0x4c;
        } else if (cue > 0x4e) {
            cue = 0x4e;
            g_advisorNagCount = 0;
        }
        m_cueDue[cue] = g_advisorNow + kQuickCue;
        flags &= ~0x2000000u;
        return;
    }
    if ((flags & 0x80000000) != 0) {
        const uint32_t pick = UiSessionRandom(0xc);
        if (pick < 6) CueDue(0x140 + pick * 4) = g_advisorNow + kNeverTicks;
        g_advisorMaySpeak = 1;
        flags &= 0x7fffffff;
    }
}

void AllianceAdvisor::RunPendingCommand() {
    if (g_advisorPendingMode != 1) {
        if (g_advisorPendingMode == 2) {
            RunCommand(g_advisorPendingArg);
        } else if (g_advisorPendingMode == 3) {
            GameObjNotifier* side = GetSideObject(1, 1);
            if (!side) {
                UiSetAlternateDisplayMode(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)));
            } else {
                UiSetAlternateDisplayMode(*reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(side) + 0xbc));
            }
        }
    }
    g_advisorPendingMode = 0;
    g_advisorPendingArg = 0;
}

void AllianceAdvisor::RunCommand(int command) {
    IdAvlTree* choices = ChoicesTree();
    switch (command) {
    case 1:
        UiSetDisplayMode(0x11, 0);
        break;
    case 2:
    case 0xe:
        choices->Clear();
        UiSetDisplayMode(0x13, 0);
        break;
    case 3: {
        choices->Clear();
        ObjectKey key = ObjectKey::Make(0x20, 5);
        GameObjNotifier* object = GetObjectInFamilies_01_ff(m_side, &key);
        if (!object) {
            UiSetDisplayMode(0x80, 0);
            break;
        }
        ObjectKey parentKey;
        void* memory = ::operator new(0x1c);
        IdNode* node = nullptr;
        if (memory) node = new (memory) IdNode(object->m_parent->GetKey(&parentKey)->Id());
        choices->AddWithId(node);
        ShowChoices(0x93, choices, 0x1623);
        break;
    }
    case 4:
        choices->Clear();
        AddChoice(choices, 0x109);
        ShowChoices(0x92, choices, 0x1624);
        break;
    case 5:
        choices->Clear();
        UiSetDisplayMode(0x20, 0);
        break;
    case 6:
        UiSetDisplayMode(0x91, 0);
        break;
    case 7:
        choices->Clear();
        ShowFirstSystemChoice(this, choices, ObjectKey::Make(0x30, 0x240), 0x1625, 0x93);
        break;
    case 8:
    case 0x14:
        choices->Clear();
        AddChoice(choices, 0x109);
        ShowChoices(0x92, choices, 0x1624);
        break;
    case 9:
        UiSetDisplayMode(0x80, 0);
        break;
    case 0xb:
        UiSetDisplayMode(0x11, 0);
        UiClearHold();
        break;
    case 0xc:
        UiInstallInputHooks();
        break;
    case 0xd:
        UnloadBriefingDll();
        if ((g_humanUiFlags.m_value & 0x8000) == 0) {
            FlushPending();
            UiPostOpenType0dWindow(1, 0x82);
        }
        UiRemoveInputHooks();
        break;
    case 0xf:
        choices->Clear();
        UiSetDisplayMode(0x15, 0);
        break;
    case 0x10:
        choices->Clear();
        UiSetDisplayMode(0x16, 0);
        break;
    case 0x11:
        choices->Clear();
        UiSetDisplayMode(0x17, 0);
        break;
    case 0x12:
        choices->Clear();
        AddChoice(choices, 0x121);
        ShowChoices(0x93, choices, 0x1622);
        break;
    case 0x13:
        choices->Clear();
        ShowFirstSystemChoice(this, choices, ObjectKey::Make(0x32, 0x242), 0x1626, 0x93);
        break;
    }
}

void AllianceAdvisor::RefusedSystem(OrderBase* order, IntPair* status) {
    if (Is(status, 0x90, 1)) {
        Raise(this, 0x2c, kShortCue);
    } else if (Is(status, 0x90, 2)) {
        Raise(this, 0x30, kShortCue);
    } else if (Is(status, 0x90, 4)) {
        Raise(this, 0x54, kShortCue);
    } else {
        ScheduleRandomPair();
    }
}

void AllianceAdvisor::RefusedManager(OrderBase* order, IntPair* status) {
    if (Is(status, 0xa0, 2)) {
        Raise(this, 8, kShortCue);
    } else if (Is(status, 0xa0, 3)) {
        Raise(this, 0x60, kShortCue);
    } else {
        ScheduleRandomPair();
    }
}

void AllianceAdvisor::RefusedFleet(OrderBase* order, IntPair* status) { ScheduleRandomPair(); }

namespace {
void AllianceScheduleFallback(AdvisorBase* advisor) { static_cast<AllianceAdvisor*>(advisor)->ScheduleRandomPair(); }
}  // namespace

void AllianceAdvisor::RefusedShip(OrderBase* order, IntPair* status) {
    RefusedUnits(this, order, status, 1, AllianceScheduleFallback);
}

void AllianceAdvisor::RefusedSide(OrderBase* order, IntPair* status) {
    if (Is(status, 0xf3, 1)) {
        Raise(this, 0x28, kShortCue);
    } else {
        ScheduleRandomPair();
    }
}

void AllianceAdvisor::RefusedOther(OrderBase* order, IntPair* status) {
    static const RefusalOffsets offsets = {0x5c, 0x58, 0x50};
    RefusedGeneric(this, order, status, offsets, AllianceScheduleFallback);
}

void AllianceAdvisor::ScheduleRandomPair() {
    const int pick = UiSessionRandom(2);
    if (pick < 2) CueDue(0x44 + pick * 4) = g_advisorNow + kShortCue;
}

void AllianceAdvisor::ToggleRepeatedStatus(IntPair* status) {
    if (m_statusRepeat == 0) {
        m_statusRepeat = 1;
        CueDue(0x3c) = g_advisorNow + kShortCue;
        return;
    }
    m_statusRepeat = 0;
    m_lastStatus.m_first = 0;
    m_lastStatus.m_second = 0;
    CueDue(0x40) = g_advisorNow + kShortCue;
}

void AllianceAdvisor::ScheduleRandomQuad() {
    for (int i = 0; i < 4; ++i) {
        if (CueDue(0x114 + i * 4) != 0) return;
    }
    const int pick = UiSessionRandom(4);
    if (pick < 4) CueDue(0x114 + pick * 4) = g_advisorNow + kShortCue;
}

void AllianceAdvisor::ScheduleRandomPairAt124() {
    const int pick = UiSessionRandom(2);
    if (pick < 2) CueDue(0x124 + pick * 4) = g_advisorNow + kShortCue;
}

void AllianceAdvisor::ScheduleCue(int cue) {
    switch (cue) {
    case 1:
        Raise(this, 0xd0, kShortCue);
        break;
    case 2:
        Raise(this, 0xd4, kShortCue);
        break;
    case 3:
        Raise(this, 0xe0, kQuickCue);
        break;
    case 4:
        Raise(this, 0xe8, kShortCue);
        break;
    case 5:
        Raise(this, 0xec, kShortCue);
        break;
    case 6:
        Raise(this, 0xf4, kShortCue);
        break;
    case 8:
        Raise(this, 0xf0, kShortCue);
        break;
    case 9:
        Raise(this, 0xf8, kShortCue);
        break;
    case 0xc:
        Raise(this, 0xe4, kShortCue);
        break;
    case 0xd:
        Raise(this, 0x104, kShortCue);
        break;
    case 0xe:
        Raise(this, 0x10c, kShortCue);
        break;
    case 0x14:
        Raise(this, 0xa8, kShortCue);
        break;
    case 0x15:
        Raise(this, 0x108, kShortCue);
        break;
    case 0x16:
        Raise(this, 0xb4, kNeverTicks);
        break;
    case 0x17:
        Raise(this, 0xac, kNeverTicks);
        break;
    case 0x18:
        Raise(this, 0xb0, kNeverTicks);
        break;
    case 0x19:
        Raise(this, 0xb8, kNeverTicks);
        break;
    case 0x1a:
    case 0x1b:
        Raise(this, 0xa8, kNeverTicks);
        break;
    case 0x1c:
        Raise(this, 0xdc, kShortCue);
        break;
    case 0x1e:
        Raise(this, 0x70, kNeverTicks);
        break;
    case 0x1f:
        Raise(this, 0x6c, kNeverTicks);
        break;
    case 0x20:
        Raise(this, 0x74, kNeverTicks);
        break;
    case 0x21:
        Raise(this, 0x78, kNeverTicks);
        break;
    case 0x22:
        Raise(this, 0x80, kNeverTicks);
        break;
    case 0x23:
        Raise(this, 0x7c, kNeverTicks);
        break;
    case 0x24:
        Raise(this, 0xcc, kNeverTicks);
        break;
    case 0x25:
        Raise(this, 0xc0, kNeverTicks);
        break;
    case 0x26:
        Raise(this, 0xbc, kNeverTicks);
        break;
    case 0x27:
        Raise(this, 200, kNeverTicks);
        break;
    case 0x28:
        Raise(this, 0xc4, kNeverTicks);
        break;
    case 0x29:
        Raise(this, 0x110, kLongCue);
        break;
    case 0x2a:
        ScheduleRandomQuad();
        break;
    case 0x2b:
        ScheduleRandomPairAt124();
        break;
    case 0x2c:
        Raise(this, 300, kShortCue);
        break;
    case 0x2e:
        Raise(this, 100, kShortCue);
        break;
    case 0x2f:
        Raise(this, 0x68, kShortCue);
        break;
    }
}

int AllianceAdvisor::PickMoodCue() {
    switch (m_mood) {
    case 1:
        return 4;
    case 2:
        return 5;
    case 4:
        return 6;
    case 0x10:
        return 7;
    case 0x20:
        return 8;
    case 0x40:
        return 9;
    default:
        return UiSessionRandom(3) + 1;
    }
}
