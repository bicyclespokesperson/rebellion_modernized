#include "economy/side.h"
#include <new>

#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/file_util.h"
#include "core/game_string.h"
#include "core/iostream_lib.h"
#include "events/game_clock_hooks.h"
#include "gameobj/object_lookup.h"
#include "orders/order_base.h"
#include "platform/win32.h"
#include "strategy_ui/advisor_decl.h"
#include "strategy_ui/advisor_droid.h"
#include "strategy_ui/advisor_sides.h"
#include "strategy_ui/game_ui_facade.h"


extern IdAvlTree g_empireChoices;      // 0x6b2918 the list the display-mode switches show
extern const char *g_invalidCommandText;  // 0x6a8838 "Processing Invalid Command"

using namespace advisor_sides;

namespace {

constexpr int kCueCount = 0x56;
constexpr unsigned kGoodbyeCueOffset = 0x128;
constexpr int kRcdataScriptDirectory = 0x618;
constexpr int kRcdataScriptName = 0x61c;
constexpr uint32_t kUiFlagChatty = 0x1000;
constexpr uint32_t kUiFlagIdleRequested = 0x2000;
constexpr uint32_t kUiFlagSleep = 0x4000000;
constexpr uint32_t kUiFlagWake = 0x8000000;


IdAvlTree* ChoicesTree() { return &g_empireChoices; }

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

void ShowFirstSystemChoice(EmpireAdvisor* advisor, IdAvlTree* tree, ObjectKey key, int16_t titleId, int mode) {
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

void PlayEntry(const DroidScriptEntry* entry) {
    UiPlayDroidScript(entry->m_id, entry->m_x, entry->m_y, (entry->m_flags >> 1) & 1);
}

void EmpireScheduleFallback(AdvisorBase* advisor) { static_cast<EmpireAdvisor*>(advisor)->ScheduleRandomPair(); }

}  // namespace

EmpireAdvisor::EmpireAdvisor(int side, HumanPlayer* player) : AdvisorBase(side, player) {
    m_cueCount = kCueCount;
    g_humanUiFlags.m_value |= kUiFlagChatty;
    m_cueDue = static_cast<uint32_t*>(::operator new(m_cueCount * sizeof(uint32_t)));
    m_cueCooldown = static_cast<uint32_t*>(::operator new(m_cueCount * sizeof(uint32_t)));
    for (uint32_t i = 0; i < m_cueCount; ++i) {
        m_cueDue[i] = 0;
        m_cueCooldown[i] = 0;
    }
    CueDue(kGoodbyeCueOffset) = kNeverTicks;
}

EmpireAdvisor::~EmpireAdvisor() {
    ::operator delete(m_cueDue);

}

void EmpireAdvisor::Read(istream* in, HumanPlayer* player) { AdvisorBase::Read(in, player); }

void EmpireAdvisor::Write(ostream* out) { AdvisorBase::Write(out); }

void EmpireAdvisor::HandleRefusedOrder(OrderBase* order, IntPair* status) {
    OutputDebugStringA(g_invalidCommandText);
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

void EmpireAdvisor::HandleUiFlags(unsigned flags) {
    if ((flags & 0x100000) != 0) {
        CueDue(0x150) = g_advisorNow + kNeverTicks;
        UiMarkGalaxyViewUnknownAc();
        return;
    }
    if ((flags & 0x1000000) != 0) {
        CueDue(0x154) = g_advisorNow + kNeverTicks;
        UiMarkGalaxyViewUnknownAc();
    }
}

int EmpireAdvisor::Update() {
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
                    if ((entry->m_flags & 1) == 0 ||
                        ((g_humanUiFlags.m_value & kUiFlagChatty) != 0 && (g_humanUiFlags.m_value & 0x4000) == 0)) {
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

int EmpireAdvisor::LoadCueScripts() {
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
        m_cueTree.Load(file.get()->In());
        CloseFileStream(file.get());
    }
    return error == 0;
}

void EmpireAdvisor::ProcessIdle() {
    uint32_t& flags = reinterpret_cast<uint32_t&>(g_humanUiFlags.m_value);
    if (m_enabled == 0) {
        if ((flags & kUiFlagWake) != 0) {
            CueDue(0x6c) = g_advisorNow + kLongCue;
            flags &= ~kUiFlagWake;
            m_enabled = 1;
        }
        flags &= 0xc000;
        return;
    }
    g_advisorMaySpeak = 0;
    if ((flags & kUiFlagSleep) != 0) {
        CueDue(0x68) = g_advisorNow + kShortCue;
        flags &= ~kUiFlagSleep;
        m_enabled = 0;
        ResetTasks();
        return;
    }
    if ((flags & kUiFlagIdleRequested) != 0) {
        CueDue(0x124) = g_advisorNow + kNeverTicks;
        UiBeginGameInput();
        flags &= ~kUiFlagIdleRequested;
        return;
    }
    if ((flags & 1) != 0) {
        CueDue(0x70) = g_advisorNow + kShortCue;
        flags &= ~1u;
        return;
    }
    if ((flags & 0x10) != 0) {
        CueDue(0x74) = g_advisorNow + kShortCue;
        flags &= ~0x10u;
        return;
    }
    if ((flags & 0x20) != 0) {
        CueDue(0x7c) = g_advisorNow + kShortCue;
        flags &= ~0x20u;
        return;
    }
    if ((flags & 0x100) != 0) {
        CueDue(0x78) = g_advisorNow + kLongCue;
        flags &= ~0x100u;
        return;
    }
    if ((flags & 0x200) != 0) {
        CueDue(0x80) = g_advisorNow + kLongCue;
        flags &= ~0x200u;
        return;
    }
    if ((flags & 0x2000000) != 0) {
        uint32_t cue = g_advisorNagCount + 0x49;
        if (cue < 0x4b) {
            cue = 0x4b;
        } else if (cue > 0x4d) {
            cue = 0x4d;
            g_advisorNagCount = 0;
        }
        m_cueDue[cue] = g_advisorNow + kQuickCue;
        flags &= ~0x2000000u;
        return;
    }
    if ((flags & 0x80000000) != 0) {
        const uint32_t pick = UiSessionRandom(0xc);
        if (pick < 6) CueDue(0x138 + pick * 4) = g_advisorNow + kQuickCue;
        g_advisorMaySpeak = 1;
        flags &= 0x7fffffff;
    }
}

void EmpireAdvisor::RunPendingCommand() {
    if (g_advisorPendingMode != 1) {
        if (g_advisorPendingMode == 2) {
            RunCommand(g_advisorPendingArg);
        } else if (g_advisorPendingMode == 3) {
            GameObjNotifier* side = GetSideObject(2, 2);
            if (!side) {
                UiSetAlternateDisplayMode(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)));
            } else {
                UiSetAlternateDisplayMode(static_cast<uint32_t>(static_cast<Side*>(side)->m_victoryConditions));
            }
        }
    }
    g_advisorPendingMode = 0;
    g_advisorPendingArg = 0;
}

void EmpireAdvisor::RunCommand(int command) {
    IdAvlTree* choices = ChoicesTree();
    switch (command) {
    case 1:
        UiSetDisplayMode(0x11, 0);
        break;
    case 2:
    case 0x12:
        choices->Clear();
        AddChoice(choices, 0x109);
        ShowChoices(0x92, choices, 0x1624);
        break;
    case 3:
        choices->Clear();
        AddChoice(choices, 0x121);
        ShowChoices(0x93, choices, 0x1622);
        break;
    case 4:
        choices->Clear();
        UiSetDisplayMode(0x17, 0);
        break;
    case 0x11:
        UiSetDisplayMode(0x17, 0);
        break;
    case 5:
        UiSetDisplayMode(0x20, 0);
        break;
    case 6:
        UiSetDisplayMode(0x91, 0);
        break;
    case 7:
        choices->Clear();
        ShowFirstSystemChoice(this, choices, ObjectKey::Make(0x34, 0x280), 0x1627, 0x92);
        break;
    case 8:
    case 0x14:
        choices->Clear();
        AddChoice(choices, 0x121);
        ShowChoices(0x93, choices, 0x1622);
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
    case 0xe:
        UiSetDisplayMode(0x13, 0);
        break;
    case 0xf:
        UiSetDisplayMode(0x15, 0);
        break;
    case 0x10:
        UiSetDisplayMode(0x14, 0);
        break;
    case 0x13:
        choices->Clear();
        ShowFirstSystemChoice(this, choices, ObjectKey::Make(0x35, 0x281), 0x1628, 0x92);
        break;
    }
}

void EmpireAdvisor::RefusedSystem(OrderBase* order, IntPair* status) {
    if (Is(status, 0x90, 1)) {
        Raise(this, 0x30, kShortCue);
    } else if (Is(status, 0x90, 2)) {
        Raise(this, 0x34, kShortCue);
    } else if (Is(status, 0x90, 4)) {
        Raise(this, 0x58, kShortCue);
    } else if (Is(status, 0x90, 3)) {
        Raise(this, 0x2c, kShortCue);
    } else {
        ScheduleRandomPair();
    }
}

void EmpireAdvisor::RefusedManager(OrderBase* order, IntPair* status) {
    if (Is(status, 0xa0, 2)) {
        Raise(this, 8, kShortCue);
    } else if (Is(status, 0xa0, 3)) {
        Raise(this, 100, kShortCue);
    } else {
        ScheduleRandomPair();
    }
}

void EmpireAdvisor::RefusedFleet(OrderBase* order, IntPair* status) { ScheduleRandomPair(); }

void EmpireAdvisor::RefusedShip(OrderBase* order, IntPair* status) {
    RefusedUnits(this, order, status, 2, EmpireScheduleFallback);
}

void EmpireAdvisor::RefusedSide(OrderBase* order, IntPair* status) {
    if (Is(status, 0xf3, 1)) {
        Raise(this, 0x28, kShortCue);
    } else {
        ScheduleRandomPair();
    }
}

void EmpireAdvisor::RefusedOther(OrderBase* order, IntPair* status) {
    static const RefusalOffsets offsets = {0x60, 0x5c, 0x54};
    RefusedGeneric(this, order, status, offsets, EmpireScheduleFallback);
}

void EmpireAdvisor::ToggleRepeatedStatus(IntPair* status) {
    if (m_statusRepeat == 0) {
        m_statusRepeat = 1;
        CueDue(0x40) = g_advisorNow + kShortCue;
        return;
    }
    m_statusRepeat = 0;
    m_lastStatus.m_first = 0;
    m_lastStatus.m_second = 0;
    CueDue(0x44) = g_advisorNow + kShortCue;
}

void EmpireAdvisor::ScheduleRandomPair() {
    const int pick = UiSessionRandom(2);
    if (pick < 2) CueDue(0x48 + pick * 4) = g_advisorNow + kShortCue;
}

void EmpireAdvisor::ScheduleRandomQuad() {
    for (int i = 0; i < 4; ++i) {
        if (CueDue(0x108 + i * 4) != 0) return;
    }
    const int pick = UiSessionRandom(4);
    if (pick < 4) CueDue(0x108 + pick * 4) = g_advisorNow + kShortCue;
}

void EmpireAdvisor::ScheduleRandomPairAt118() {
    const int pick = UiSessionRandom(2);
    if (pick < 2) CueDue(0x118 + pick * 4) = g_advisorNow + kShortCue;
}

void EmpireAdvisor::ScheduleCue(int cue) {
    switch (cue) {
    case 1:
        Raise(this, 0xc0, kShortCue);
        break;
    case 2:
        Raise(this, 0xc4, kShortCue);
        break;
    case 3:
        Raise(this, 0xd0, kShortCue);
        break;
    case 4:
        Raise(this, 0xd8, kShortCue);
        break;
    case 5:
        Raise(this, 0xdc, kShortCue);
        break;
    case 6:
        Raise(this, 0xe4, kShortCue);
        break;
    case 8:
        Raise(this, 0xe0, kShortCue);
        break;
    case 9:
        Raise(this, 0xe8, kShortCue);
        break;
    case 0xc:
        Raise(this, 0xd4, kShortCue);
        break;
    case 0xd:
        Raise(this, 0xf4, kShortCue);
        break;
    case 0xe:
        Raise(this, 0xf8, kQuickCue);
        break;
    case 0x14:
        Raise(this, 0x100, kShortCue);
        break;
    case 0x15:
        Raise(this, 0xfc, kShortCue);
        break;
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
        Raise(this, 0x100, kNeverTicks);
        break;
    case 0x1a:
        Raise(this, 0x88, kNeverTicks);
        break;
    case 0x1b:
        Raise(this, 0x84, kNeverTicks);
        break;
    case 0x1c:
        Raise(this, 0xcc, kNeverTicks);
        break;
    case 0x1e:
        Raise(this, 0xa8, kNeverTicks);
        break;
    case 0x1f:
        Raise(this, 0xa4, kNeverTicks);
        break;
    case 0x20:
        Raise(this, 0xac, kNeverTicks);
        break;
    case 0x21:
        Raise(this, 0xb0, kNeverTicks);
        break;
    case 0x22:
        Raise(this, 0xb8, kNeverTicks);
        break;
    case 0x23:
        Raise(this, 0xb4, kNeverTicks);
        break;
    case 0x24:
        Raise(this, 0xbc, kNeverTicks);
        break;
    case 0x25:
        Raise(this, 0x98, kNeverTicks);
        break;
    case 0x26:
        Raise(this, 0x94, kNeverTicks);
        break;
    case 0x27:
        Raise(this, 0xa0, kNeverTicks);
        break;
    case 0x28:
        Raise(this, 0x9c, kNeverTicks);
        break;
    case 0x29:
        Raise(this, 0x104, kLongCue);
        break;
    case 0x2a:
        ScheduleRandomQuad();
        break;
    case 0x2b:
        ScheduleRandomPairAt118();
        break;
    case 0x2d:
        Raise(this, 0x120, kShortCue);
        break;
    case 0x2e:
        Raise(this, 0x8c, kShortCue);
        break;
    case 0x2f:
        Raise(this, 0x90, kShortCue);
        break;
    }
}

int EmpireAdvisor::PickMoodCue() {
    switch (m_mood) {
    case 1:
        return 9;
    case 2:
        return 8;
    case 4:
        return 7;
    case 0x10:
        return 6;
    case 0x20:
        return 5;
    case 0x40:
        return 4;
    default:
        return UiSessionRandom(3) + 1;
    }
}
IdAvlTree g_empireChoices{};
const char *g_invalidCommandText = nullptr;
