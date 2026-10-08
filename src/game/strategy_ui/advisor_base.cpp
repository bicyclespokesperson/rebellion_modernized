#include <new>

#include "app/player_controller_decl.h"
#include "app/rebapp_globals.h"
#include "core/stream_io.h"
#include "core/vptr_store.h"
#include "events/game_clock_hooks.h"
#include "orders/order_base.h"
#include "strategy_ui/advisor_decl.h"
#include "strategy_ui/advisor_droid.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/human_player.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));
extern "C" void __cdecl LAB_004392e0();

namespace {

constexpr int kNetworkModeSkipsSpeech1 = 1;
constexpr int kNetworkModeSkipsSpeech2 = 4;
constexpr uint32_t kFlagSpeechOff = 0x8000;
constexpr uint32_t kFlagNoticesCollected = 0x10000000;
constexpr int kTaskReady = 2;
constexpr int kNoticeKindMask = 0xf;
constexpr uint32_t kSpeechIdleTicks = 300;
constexpr int kInitialMood = 8;
constexpr int kInitialBatch = 5;
constexpr int kPrimaryTaskId = 0x17;
constexpr int kTaskIdA = 0x14;
constexpr int kTaskIdB = 0x15;

int TaskState(AdvisorTask* task) { return reinterpret_cast<PersistentInt*>(&task->m_value)->Get(); }
void SetTaskStateRaw(AdvisorTask* task, int value) { task->m_value = value; }
int NoticeKind(IdValueNode* notice) { return notice->m_value & kNoticeKindMask; }

}  // namespace

// ---- Static initialisation of g_humanUiFlags ------------------------------------------------------------------

void AdvisorInitFlags() { new (&g_humanUiFlags) PersistentInt(0); }  // 0x4392c0
void AdvisorRegisterFlagsAtExit() { atexit(LAB_004392e0); }          // 0x4392d0
void AdvisorInitStatics() {                                          // 0x4392b0
    AdvisorInitFlags();
    AdvisorRegisterFlagsAtExit();
}

// ---- Construction / persistence -------------------------------------------------------------------------------

AdvisorBase::AdvisorBase(int side, HumanPlayer* player) : m_flags(0) {
    FUN_0049dcf0(m_taskTree);
    FUN_0049c780(m_aiHelper);
    FUN_0049c6a0(m_cueTree);
    m_unk04 = 1;
    m_enabled = 1;
    m_state = 1;
    m_player = player;
    m_mood = kInitialMood;
    m_currentTask = nullptr;
    m_unk38 = 0;
    m_side = side;
    FUN_0049c880(m_aiHelper, side);
    *reinterpret_cast<int*>(m_taskTree + 0xc) = side;
    m_unk160 = 0;
    g_advisorNagCount = 0;
    m_cueCount = 0;
    m_cueDue = nullptr;
    m_cueCooldown = nullptr;
    g_advisorLastRun = 0;
    g_advisorNow = 0;
    m_lastSpeechTime = 0;
    m_unk14c = 8;
    g_advisorMaySpeak = 0;
    g_advisorFlag0 = 0;
    g_humanUiFlags.m_value = 0;
    g_advisorPendingMode = 0;
    g_advisorPendingArg = 0;
    m_statusRepeat = 0;
    m_lastStatus.m_first = 0;
    m_lastStatus.m_second = 0;
    m_primaryTask = nullptr;
    m_unk188 = 0;
    m_unk18c = 0;
    m_unk190 = 0;
    const int networkMode = AppGetNetworkMode();
    if (networkMode != kNetworkModeSkipsSpeech1 && networkMode != kNetworkModeSkipsSpeech2) {
        g_humanUiFlags.m_value |= kFlagSpeechOff;
    }
}

AdvisorBase::~AdvisorBase() {
    m_player = nullptr;
    m_primaryTask = nullptr;
    if (m_cueCooldown) {
        ::operator delete(m_cueCooldown);
        m_cueCooldown = nullptr;
    }
    FUN_0049c6e0(m_cueTree);
    FUN_0049c810(m_aiHelper);
    FUN_0049dd30(m_taskTree);
    REB_KEEP_VPTR_STORE();
}

void AdvisorBase::Read(istream* in, HumanPlayer* player) {
    int taskId = 0;
    m_player = player;
    ReadValue32(in, &m_unk04);
    ReadValue32(in, &m_enabled);
    ReadValue32(in, &m_side);
    ReadValue32(in, &m_state);
    ReadValue32(in, &g_advisorMaySpeak);
    ReadValue32(in, &g_advisorFlag0);
    ReadValuePersistentInt(in, &g_humanUiFlags);
    ReadValue32(in, &g_advisorPendingMode);
    ReadValue32(in, &g_advisorPendingArg);
    FUN_0049c8a0(m_aiHelper, in);
    FUN_0049c6f0(m_cueTree, in);
    ReadValue32(in, &m_unk38);
    FUN_0049dd50(m_taskTree, in, m_aiHelper);
    ReadValue32(in, &taskId);
    m_currentTask = static_cast<AdvisorTask*>(Tasks()->FindById(taskId));
    ReadValue8(in, &m_mood);
    ReadValue32(in, &m_unk160);
    ReadValue32(in, &g_advisorNagCount);
    ReadValue32(in, &m_cueCount);
    ReadValue32(in, &m_unk188);
    ReadValue32(in, &m_unk18c);
    ReadValue32(in, &m_unk190);
    if (m_cueDue) ::operator delete(m_cueDue);
    if (m_cueCooldown) ::operator delete(m_cueCooldown);
    if (m_cueCount != 0) {
        m_cueDue = static_cast<uint32_t*>(::operator new(m_cueCount * 4));
        m_cueCooldown = static_cast<uint32_t*>(::operator new(m_cueCount * 4));
        for (uint32_t i = 0; i < m_cueCount; ++i) {
            ReadValue32(in, &m_cueDue[i]);
            m_cueCooldown[i] = 0;
        }
    }
    ReadValue32(in, &m_lastSpeechTime);
    ReadValue32(in, &m_unk14c);
    m_flags.Read(in);
    m_notices.Clear();
    m_notices.Read(in);
    m_primaryTask = static_cast<AdvisorTask*>(Tasks()->FindById(kPrimaryTaskId));
    m_primaryTask->m_advisor = this;
}

void AdvisorBase::Write(ostream* out) {
    uint32_t taskId = 0;
    WriteValue32(out, &m_unk04);
    WriteValue32(out, &m_enabled);
    WriteValue32(out, &m_side);
    WriteValue32(out, &m_state);
    WriteValue32(out, &g_advisorMaySpeak);
    WriteValue32(out, &g_advisorFlag0);
    WriteValuePersistentInt(out, &g_humanUiFlags);
    WriteValue32(out, &g_advisorPendingMode);
    WriteValue32(out, &g_advisorPendingArg);
    FUN_0049c9f0(m_aiHelper, out);
    FUN_004e5210(m_cueTree, out);
    WriteValue32(out, &m_unk38);
    FUN_0049de40(m_taskTree, out);
    if (m_currentTask) taskId = m_currentTask->m_id;
    WriteValue32(out, &taskId);
    WriteValue8(out, &m_mood);
    WriteValue32(out, &m_unk160);
    WriteValue32(out, &g_advisorNagCount);
    WriteValue32(out, &m_cueCount);
    WriteValue32(out, &m_unk188);
    WriteValue32(out, &m_unk18c);
    WriteValue32(out, &m_unk190);
    if (m_cueCount != 0) {
        for (uint32_t i = 0; i < m_cueCount; ++i) WriteValue32(out, &m_cueDue[i]);
    }
    WriteValue32(out, &m_lastSpeechTime);
    WriteValue32(out, &m_unk14c);
    m_flags.Write(out);
    m_notices.Write(out);
}

// ---- The task machine -----------------------------------------------------------------------------------------

int AdvisorBase::StepStartup() {
    switch (m_state) {
    case 1:
        LoadCueScripts();
        m_state = 2;
        break;
    case 2:
        if (FUN_0049cb30(m_aiHelper) != 0) m_state = 3;
        break;
    case 3: {
        FUN_0049df20(m_taskTree, kTaskIdA, m_aiHelper);
        FUN_0049df20(m_taskTree, kTaskIdB, m_aiHelper);
        FUN_0049df20(m_taskTree, kPrimaryTaskId, m_aiHelper);
        m_unk18c = kInitialBatch;
        ResetTasks();
        m_primaryTask = static_cast<AdvisorTask*>(Tasks()->FindById(kPrimaryTaskId));
        m_primaryTask->m_advisor = this;
        m_state = 5;
        return 1;
    }
    }
    return 0;
}

int AdvisorBase::StepTasks() {
    int result = 0;
    if (m_state == 4) {
        AdvisorTask* task = m_currentTask;
        if (!task) {
            m_currentTask = FUN_0049df70(m_taskTree);
            if (m_currentTask) return 0;
        } else if (m_unk188 < m_unk18c || task->m_id == static_cast<uint32_t>(kPrimaryTaskId)) {
            task->m_unk30 = 0;
            SidedMessage* order = task->NextOrder();
            if (order) {
                IntPair status;
                if (order->CheckValid(&status) == 0) {
                    task->OnOrderRefused(&status, order);
                } else {
                    m_player->SubmitMessage(reinterpret_cast<PlayerMessage*>(order));
                    if (task->m_id != static_cast<uint32_t>(kPrimaryTaskId)) ++m_unk188;
                }
            }
            if (task->m_done == 0) return 0;
            if (TaskState(task) != kTaskReady) SetTaskStateRaw(task, 0);
            m_currentTask = nullptr;
        }
        m_state = 5;
        result = 1;
    } else if (m_state == 5) {
        if (FUN_0049cc80(m_aiHelper) != 0) {
            m_state = 6;
            return 0;
        }
    } else if (m_state == 6) {
        if (!m_currentTask) m_currentTask = static_cast<AdvisorTask*>(Tasks()->First());
        m_currentTask->Advance();
        m_currentTask = static_cast<AdvisorTask*>(m_currentTask->m_successor);
        if (!m_currentTask) {
            m_state = 4;
            return 0;
        }
    } else {
        m_state = 5;
        return 0;
    }
    return result;
}

int AdvisorBase::Tick() { return Update(); }

// ---- Speech ---------------------------------------------------------------------------------------------------

void AdvisorBase::SpeakIfIdle() {
    if ((g_humanUiFlags.m_value & kFlagSpeechOff) == 0 && m_enabled != 0 &&
        m_lastSpeechTime + kSpeechIdleTicks <= g_advisorNow) {
        SpeakNow();
    }
}

void AdvisorBase::ResetSpeech() {
    m_lastStatus.m_first = 0;
    m_statusRepeat = 0;
    g_advisorNagCount = 0;
}

int AdvisorBase::IsBusy() { return m_primaryTask && TaskState(m_primaryTask) == kTaskReady; }

void AdvisorBase::SetTaskState(int value) {
    if (m_primaryTask) FUN_0049e380(m_primaryTask, value);
}

void AdvisorBase::SelectTaskMode(int command) {
    if (m_enabled == 0) return;
    if (command == 0x110) {
        FUN_0049e360(m_primaryTask, 2);
    } else if (command == 0x111) {
        FUN_0049e360(m_primaryTask, 3);
    } else if (command == 0x112) {
        FUN_0049e360(m_primaryTask, 1);
    }
}

void AdvisorBase::ToggleTask(unsigned command) {
    AdvisorTask* task = nullptr;
    unsigned toggledOn = command;
    unsigned toggledOff = command;
    if (m_enabled != 0) {
        if (command == 0x115) {
            task = static_cast<AdvisorTask*>(Tasks()->FindById(kTaskIdB));
            toggledOn = 0x100;
            toggledOff = 0x10;
        } else if (command == 0x116) {
            task = static_cast<AdvisorTask*>(Tasks()->FindById(kTaskIdA));
            toggledOn = 0x200;
            toggledOff = 0x20;
        }
    }
    if (!task) return;
    if (TaskState(task) == kTaskReady) {
        SetTaskStateRaw(task, 0);
        ++m_unk160;
        if (m_unk160 > 3) {
            g_humanUiFlags.m_value |= 0x1000000;
            return;
        }
        g_humanUiFlags.m_value |= toggledOff;
        return;
    }
    g_humanUiFlags.m_value |= toggledOn;
    SetTaskStateRaw(task, kTaskReady);
    --m_unk160;
}

int AdvisorBase::IsTaskReady(int command) {
    AdvisorTask* task = nullptr;
    if (m_enabled != 0) {
        if (command == 0x115) {
            task = static_cast<AdvisorTask*>(Tasks()->FindById(kTaskIdB));
        } else if (command == 0x116) {
            task = static_cast<AdvisorTask*>(Tasks()->FindById(kTaskIdA));
        }
    }
    if (task && TaskState(task) != kTaskReady) return 1;
    return 0;
}

void AdvisorToggleUiFlag(int command) {  // 0x439e80
    if (command == 0x119) return;
    if (command == 0x11b) {
        g_humanUiFlags.m_value ^= 0x1000;
    } else if (command == 0x11e) {
        g_humanUiFlags.m_value ^= kFlagSpeechOff;
    }
}

void AdvisorSetPendingCommand(int mode, int argument) {  // 0x439eb0
    g_advisorPendingMode = mode;
    g_advisorPendingArg = argument;
}

void AdvisorNoteNag() {  // 0x439ed0
    g_humanUiFlags.m_value |= 0x2000000;
    ++g_advisorNagCount;
}

void AdvisorAllowSpeech() {  // 0x439ef0
    g_advisorMaySpeak = 1;
    if (g_advisorLastRun + 0x32 < g_advisorNow) g_humanUiFlags.m_value |= 0x80000000;
}

void AdvisorSetFlag0() { g_advisorFlag0 = 1; }  // 0x43a1f0

void AdvisorRequestIdle() { g_humanUiFlags.m_value |= 0x2000; }  // 0x43a200

void AdvisorBase::FlushPending() {
    if ((m_flags.m_value & kFlagNoticesCollected) != 0) return;
    if (FUN_0048b460(m_side, &m_notices) != 0) m_flags.m_value |= kFlagNoticesCollected;
    IdValueNode* notice = static_cast<IdValueNode*>(m_notices.Last());
    while (notice) {
        IdValueNode* previous = static_cast<IdValueNode*>(notice->Predecessor());
        if (NoticeKind(notice) == 7 || g_advisorFlushAll != 0) {
            FUN_0048a590(m_player->Dispatch(), notice);
            m_notices.DetachById(notice->m_id);
            delete notice;
        }
        notice = previous;
    }
}

void AdvisorBase::SpeakNow() {
    IdValueNode* notice = static_cast<IdValueNode*>(m_notices.First());
    uint32_t found = 0;
    if ((m_flags.m_value & kFlagNoticesCollected) == 0) FlushPending();
    while (notice && found == 0) {
        switch (notice->m_value & kNoticeKindMask) {
        default:
            found = 1;
            break;
        case 2:
            found = (m_flags.m_value >> 1) & 1;
            break;
        case 3:
            found = (m_flags.m_value >> 2) & 1;
            break;
        case 4:
            found = (m_flags.m_value >> 4) & 1;
            break;
        case 5:
            found = (m_flags.m_value >> 3) & 1;
            break;
        case 6:
            found = m_flags.m_value & 1;
        }
        if (found == 0) {
            notice = static_cast<IdValueNode*>(notice->m_successor);
        } else {
            FUN_0048a590(m_player->Dispatch(), notice);
            IdNode* removed = m_notices.DetachById(notice->m_id);
            if (!removed) break;
            delete removed;
            break;
        }
    }
    m_lastSpeechTime = GameHookTickCount();
}

void AdvisorBase::SpeakTopic(unsigned topic) {
    if ((g_humanUiFlags.m_value & kFlagSpeechOff) != 0 || m_enabled == 0) return;
    if ((m_flags.m_value & kFlagNoticesCollected) == 0) FlushPending();
    if (topic == 0 || (m_flags.m_value & topic) != 0) return;
    m_flags.m_value |= topic;
    uint32_t kind = 0;
    switch (topic) {
    case 1:
        kind = 6;
        break;
    case 2:
        kind = 2;
        break;
    case 4:
        kind = 3;
        break;
    case 8:
        kind = 5;
        break;
    case 0x10:
        kind = 4;
    }
    if (kind == 0) return;
    bool told = false;
    IdValueNode* notice = static_cast<IdValueNode*>(m_notices.First());
    while (notice && !told) {
        if (static_cast<uint32_t>(NoticeKind(notice)) == kind) {
            FUN_0048a590(m_player->Dispatch(), notice);
            told = true;
            m_lastSpeechTime = GameHookTickCount();
            IdNode* removed = m_notices.DetachById(notice->m_id);
            // BUG(original): the loop reads the successor of the node it has just deleted (harmless, the loop ends).
            if (removed) delete removed;
            break;
        }
        notice = static_cast<IdValueNode*>(notice->m_successor);
    }
}

void AdvisorBase::SetAiHelperFlag(int value) { FUN_0049cd70(m_aiHelper, value); }
void AdvisorBase::ClearAiHelperFlag(int value) { FUN_0049cd90(m_aiHelper, value); }

void AdvisorBase::ResetTasks() {
    for (AvlNode* node = Tasks()->First(); node; node = node->m_successor) static_cast<IdValueNode*>(node)->m_value = kTaskReady;
}

int AdvisorBase::PickAdvice() {
    if (m_unk190 > 0) {
        --m_unk190;
        return UiSessionRandom(6) / 2 + 1;
    }
    m_unk190 = 3;
    return PickMoodCue();
}

void AdvisorBase::SetRefusedOrder(const void* order) { m_refusedOrder = order; }

void AdvisorBase::OnOrderSubmitted(const void* order) {
    if ((g_humanUiFlags.m_value & kFlagNoticesCollected) != 0 && order == m_refusedOrder) g_humanUiFlags.m_value |= 1;
    g_humanUiFlags.m_value &= ~kFlagNoticesCollected;
}
