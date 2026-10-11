#include "strategy_ui/human_player.h"

#include <new>

#include "ai/ai_common.h"
#include "ai/ai_task_manager.h"
#include "app/rebapp_globals.h"
#include "audio/sound_manager.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/stream_io.h"
#include "mission/mission.h"
#include "events/net_messages.h"
#include "character/role.h"
#include "economy/side.h"
#include "events/game_clock_hooks.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/notif_payload.h"
#include "gameobj/object_lookup.h"
#include "gameobj/status_code.h"
#include "orders/order_base.h"
#include "strategy_ui/advisor_droid.h"
#include "strategy_ui/advisor_notices.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/animation_step.h"

// ---- Parts of the player controller (0x488460-0x48aa90) and of the advisor droid (0x4392b0-0x43a2d0) that this class
// calls; they belong to other modules.
void FUN_0048aa90(PlayerController* player, PlayerMessage* message);
void FUN_00488b50(PlayerDispatch* dispatch, void* message);
void FUN_00488b90(PlayerDispatch* dispatch);
void FUN_00488d20(PlayerDispatch* dispatch, const ObjectKey* key, int flag);
void FUN_0048a340(PlayerDispatch* dispatch, PlayerMessage* message);
void FUN_0048a3b0(PlayerDispatch* dispatch, PlayerMessage* message);
extern PersistentInt g_humanUiFlags;  // 0x6b28b0

namespace {

constexpr uint32_t kAdvisorMessageEnd = 0x11;
constexpr int kSideAlliance = 1;
constexpr int kSideEmpire = 2;
constexpr uint32_t kSetSpeedKind = 0x280;
constexpr uint32_t kSpeedTicks[4] = {600, 0x3c, 0xc, 4};
constexpr uint32_t kDefaultSpeedTicks = 0x3c;
constexpr int kMaterialsUnused = 1000;
constexpr uint32_t kSoundModule = 7;
constexpr uint32_t kTargetRequestSound = 0x26d;
constexpr uint32_t kBlockedRequestSound = 0x26c;
// An order with the given sound request flags (kind 1: a one-shot effect).
void PlayEffect(uint32_t soundId) {
    SoundRequest request(0, 0, 0, 0, 0, 0);
    request.m_kind = 1;
    request.m_soundId = soundId;
    request.m_moduleIndex = kSoundModule;
    SoundManager::Play(&request);
}

AdvisorBase* NewAdvisor(HumanPlayer* player) {
    if (player->m_side == kSideAlliance) return NewOrNull<AllianceAdvisor>(player->m_side, player);
    return NewOrNull<EmpireAdvisor>(player->m_side, player);
}

bool FamilyIn(const ObjectKey& key, uint32_t first, uint32_t last) { return key.Family() >= first && key.Family() < last; }

// The mask of command kinds the advisor allows, and the resource figure of the advisor's picture used by Virtual21.
struct AdvisorStats {
    int m_unknown[0x30];
};

}  // namespace

void HumanSetUiTarget(int a, int b) { AdvisorSetPendingCommand(a, b); }

// ---- Construction ----------------------------------------------------------------------------------------------

HumanPlayer::HumanPlayer() : PlayerController() {
    new (Dispatch()) PlayerDispatch();
    m_unk50 = 0;
    m_speed = 0;
    m_advisor = nullptr;
    m_pausedSpeed = 0;
}

HumanPlayer::HumanPlayer(int side) : PlayerController(side) {
    new (Dispatch()) PlayerDispatch();
    m_unk50 = 0;
    m_speed = 0;
    m_advisor = nullptr;
    m_pausedSpeed = 0;
    if (m_side == kSideAlliance || m_side == kSideEmpire) m_advisor = NewAdvisor(this);
    m_advisor->m_dispatch = Dispatch();
    Dispatch()->m_advisor = m_advisor;
    Dispatch()->m_side = m_side;
}

HumanPlayer::~HumanPlayer() {
    delete m_advisor;
    Dispatch()->PlayerDispatch::~PlayerDispatch();

}

void HumanPlayer::Read(istream* in) {
    PlayerController::Read(in);
    ReadValue32(in, &m_unk50);
    ReadValue32(in, &m_speed);
    m_alertList.Load(in);
    Dispatch()->Read(in);
    if (m_side == kSideAlliance || m_side == kSideEmpire) m_advisor = NewAdvisor(this);
    m_advisor->m_dispatch = Dispatch();
    Dispatch()->m_advisor = m_advisor;
    if (m_advisor) m_advisor->Read(in, this);
    m_pausedSpeed = 0;
}

void HumanPlayer::Write(ostream* out) {
    PlayerController::Write(out);
    WriteValue32(out, &m_unk50);
    WriteValue32(out, &m_speed);
    m_alertList.Save(out);
    Dispatch()->Write(out);
    if (m_advisor) m_advisor->Write(out);
}

// ---- Controller slots --------------------------------------------------------------------------------------------

int HumanPlayer::Virtual04() {
    int result = 1;
    if (m_advisor) result = m_advisor->StepStartup();
    if (result) {
        Dispatch()->PlayerDispatch::ClearRecords();
        SetSpeed(2);
    }
    return result;
}

void HumanPlayer::Virtual07(PlayerMessage* message) {
    if (reinterpret_cast<MessageBase*>(message)->GetKind() == kAdvisorMessageEnd) {
        Dispatch()->OnChatMessage(reinterpret_cast<MessageBase*>(message));
        if (message) delete reinterpret_cast<MessageBase*>(message);
    } else {
        PlayerController::Virtual07(message);
    }
}

int HumanPlayer::Virtual08(PlayerMessage* message) {
    if (reinterpret_cast<MessageBase*>(message)->GetKind() == kAdvisorMessageEnd) Dispatch()->OnChatMessage(reinterpret_cast<MessageBase*>(message));
    return 1;
}

int HumanPlayer::Virtual09() {
    int result = 1;
    if (m_advisor) result = m_advisor->StepTasks();
    return result;
}

void HumanPlayer::Virtual10() {
    if (m_advisor && !HasPendingMessages()) m_advisor->Tick();
    if (AiNoticeGroup* group = m_alertList.DetachFinished()) {
        Dispatch()->HandleNotifications(reinterpret_cast<LinkNode*>(group));
        delete group;
    }
    if (m_syncTarget == 0 && !UiIsPauseTick()) Dispatch()->Update();
}

void HumanPlayer::Virtual14() {
    UiRefreshChildWindows();
    const int day = GameHookDay();
    UiSetDateAndSpeed(day, m_speed);
    m_advisor->SpeakIfIdle();
}

void HumanPlayer::Virtual20(NotifPayload* notice) {
    if (!notice) return;
    notice->Dispatch(this);
    m_alertList.Add(notice);
}

void HumanPlayer::Virtual17(int id, ObjectKey* key) {
    AppPlayClickSound();
    ClearGameTime(&m_voteTime);
    m_voteState = 0;
    new (&m_voteKey) ObjectKey(*key);
    m_voteId = id;
    uint32_t a = 0;
    uint32_t b = 0;
    int c = 0;
    uint32_t d = 0;
    const int found = QueryBattleState(m_side, &a, &b, &c, &d);
    int attack = 0;
    if (found != 0 && a == 0 && b != 0 && c != 0) attack = 1;
    Dispatch()->AddArrivalNotice(key, attack);
    Dispatch()->m_unk40 = 1;
}

void HumanPlayer::Virtual18(int, ObjectKey*, int state, GameTime*) {
    if (state == 2) UiForwardToType6Window(0, 1);
}

void HumanPlayer::Virtual19(int id, ObjectKey* key) {
    Dispatch()->m_unk40 = 1;
    m_voteState = 0;
    ClearGameTime(&m_voteTime);
    new (&m_voteKey) ObjectKey(*key);
    m_voteId = id;
}

void HumanPlayer::Virtual21() {
    m_advisor->m_unk188 = 0;
    uint8_t* mood = &m_advisor->m_mood;
    *mood = 8;
    const int difference = m_advisor->m_aiHelper.Total(0x88) - m_advisor->m_aiHelper.Total(0x8c);
    if (difference < 2) {
        if (difference == 1)
            *mood = 2;
        else if (difference == -1)
            *mood = 0x20;
        else if (difference < -1)
            *mood = 0x40;
    } else {
        *mood = 1;
    }
    if (*mood == 8) {
        const int divisor = m_advisor->m_aiHelper.Total(0x9c);
        const int numerator = m_advisor->m_aiHelper.Total(0x98);
        const int ratio = divisor == 0 ? numerator * 10 : (numerator * 100) / divisor;
        if (299 < ratio) {
            *mood = 2;
            return;
        }
        if (199 < ratio) {
            *mood = 4;
            return;
        }
        if (ratio < 0x33) {
            *mood = 0x10;
            return;
        }
        if (ratio < 0x22) *mood = 0x20;
    }
}

void HumanPlayer::Virtual94(int side) {
    if (m_side == side) m_syncTarget = 0;
}

void HumanPlayer::Virtual95() {
    ObjectKeyValueSet selection;
    ObjectKey key;
    PlayerController::Virtual95();
    ExecuteCommand(5, &selection, &key, nullptr);
}

// ---- Notification handlers -----------------------------------------------------------------------------------

void HumanPlayer::Virtual22(int a, int b) {
    if (m_advisor) reinterpret_cast<AnimationStep*>(m_advisor)->AnimationStep::SetStart(a, b);
}

void HumanPlayer::Virtual23(int value) {
    UiSetRawMaterials(value, kMaterialsUnused);
}

void HumanPlayer::Virtual24(int value) {
    UiSetRefinedMaterials(value, kMaterialsUnused);
}

void HumanPlayer::Virtual25(intptr_t value) {
    GameObjNotifier* side = GetSideObject(AppGetSide(), AppGetSide());
    int available = 0;
    int total = 0;
    if (side) {
        IntPair scratch;
        total = GetSidePairAt58(static_cast<Side*>(side), &scratch)->m_first;
        available = total - static_cast<Side*>(side)->m_maintRequiredB;
    }
    UiSetMaintenance(available, total);
    m_advisor->SetAiHelperFlag(value);
}

void HumanPlayer::Virtual26(int value) {
    GameObjNotifier* side = GetSideObject(AppGetSide(), AppGetSide());
    int available = 0;
    int total = 0;
    if (side) {
        IntPair scratch;
        total = GetSidePairAt58(static_cast<Side*>(side), &scratch)->m_first;
        available = total - static_cast<Side*>(side)->m_maintRequiredB;
    }
    UiSetMaintenance(available, total);
    m_advisor->ClearAiHelperFlag(value);
}

void HumanPlayer::Virtual27(int) {}

void HumanPlayer::Virtual29(unsigned flags) {
    if ((flags & 0x10000000) && m_advisor) {
        m_advisor->HandleUiFlags(flags);
    }
}

void HumanPlayer::Virtual30(const ObjectKey* key, int arg) {
    UiOnSystemChangedKind2(key, arg, 0);
    UiBroadcastObjectUpdated(key, nullptr);
    m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChanged(key, 0);
}

void HumanPlayer::Virtual31(const ObjectKey* key, int arg) {
    UiOnSystemChangedKind1(key, -1, arg);
    m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChanged(key, 0);
}

void HumanPlayer::Virtual32(const ObjectKey* key, int arg) {
    UiOnSystemChangedKind1(key, arg, -1);
    m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChanged(key, 0);
}

void HumanPlayer::Virtual33(const ObjectKey* key, int) {
    UiBroadcastObjectUpdated(key, nullptr);
    m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChanged(key, 0);
}

void HumanPlayer::Virtual43(const ObjectKey* key, int sound) {
    UiBroadcastObjectUpdated(key, nullptr);
    m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChanged(key, 0);
    if (sound) PlayEffect(kTargetRequestSound);
}

void HumanPlayer::Virtual44(const ObjectKey* key, int sound) {
    UiBroadcastObjectUpdated(key, nullptr);
    m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChanged(key, 0);
    if (sound) PlayEffect(kBlockedRequestSound);
}

void HumanPlayer::Virtual45(const ObjectKey* key, int) { m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChanged(key, 0); }

void HumanPlayer::Virtual51(const ObjectKey* manager, int unused) {
    UiOnManagerProductionChanged(manager, unused);
    m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChanged(manager, 0);
}

void HumanPlayer::Virtual52(const ObjectKey* manager, int position) { UiOnManagerProgressChanged(manager, position); }

void HumanPlayer::Virtual53(const ObjectKey* manager, const ObjectKey* character) {
    UiOnManagerCharacterChanged(manager, character);
}

void HumanPlayer::Virtual54(const ObjectKey* manager, String* text) { UiOnManagerStatusTextChanged(manager, text); }

void HumanPlayer::Virtual62(const ObjectKey* key, int redraw) {
    if (redraw) m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChanged(key, 0);
}

void HumanPlayer::Virtual80(const ObjectKey* key, int) { UiBroadcastObjectUpdated(key, nullptr); }

void HumanPlayer::Virtual81(const ObjectKey* key, const ObjectKey* oldKey, const ObjectKey* newKey) {
    ObjectKey parentKey;
    ObjectKey oldMission;
    ObjectKey probeOld(*oldKey);
    if (probeOld.OrNull()) {
        if (GameObjNotifier* mission = GetObjectInFamilies_40_80(m_side, oldKey)) {
            ObjectKey missionKey;
            oldMission = *mission->GetKey(&missionKey);
        }
    }
    ObjectKey newMission;
    ObjectKey probeNew(*newKey);
    if (probeNew.OrNull()) {
        if (GameObjNotifier* mission = GetObjectInFamilies_40_80(m_side, newKey)) {
            ObjectKey missionKey;
            newMission = *mission->GetKey(&missionKey);
        }
    }
    if (oldMission.m_value != newMission.m_value) {
        if (oldMission.OrNull()) {
            parentKey = *oldKey;
            UiBroadcastObjectMoved(key, nullptr, &parentKey, nullptr);
        }
        GameObjNotifier* character = GetObjectInFamilies_30_40(m_side, key);
        GameObjNotifier* location = character ? character->m_parent : nullptr;
        if (location) {
            ObjectKey locationKey;
            parentKey = *location->GetKey(&locationKey);
        }
        UiBroadcastObjectMoved(key, nullptr, &parentKey, nullptr);
        UiBroadcastObjectChanged(key, nullptr);
    }
    m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChanged(key, 0);
}

void HumanPlayer::Virtual82(const ObjectKey* key) {
    UiBroadcastObjectChanged(key, nullptr);
    m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChanged(key, 0);
}

void HumanPlayer::Virtual83(const ObjectKey* key, const ObjectKey* newParent, const ObjectKey* arg) {
    if (FamilyIn(*key, 0x30, 0x40)) {
        if (GameObjNotifier* character = GetObjectInFamilies_30_40(m_side, key)) {
            ObjectKey* location = &static_cast<Role*>(character)->m_missionKey;
            if (location->IsSet() && !(static_cast<Role*>(character)->m_flags78 & 0x100)) {
                ObjectKey copy = *location;
                UiBroadcastObjectMoved(key, nullptr, &copy, nullptr);
            }
        }
    }
    UiBroadcastObjectMoved(key, nullptr, newParent, nullptr);
    UiBroadcastObjectChanged(key, nullptr);
    m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteMoved(key, newParent, arg);
}

void HumanPlayer::Virtual87(const ObjectKey* key, StringWords name) {
    UiBroadcastObjectUpdated(key, nullptr);
    // The String that was passed by value in the argument words is destroyed by the callee.
    static_assert(sizeof(String) <= sizeof(StringWords), "a String must fit in the argument words");
    reinterpret_cast<String*>(&name)->String::~String();
}

void HumanPlayer::Virtual90(const ObjectKey* key) {
    UiBroadcastObjectUpdated(key, nullptr);
    m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChanged(key, 0);
}

void HumanPlayer::Virtual91(const ObjectKey* key, const ObjectKey* newParent) {
    UiBroadcastObjectMoved(key, nullptr, newParent, nullptr);
    m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChangedAlt(key);
    m_advisor->m_aiHelper.AiGalaxyAnalysis::NoteChanged(newParent, 0);
}

// ---- Commands and orders (0x486fb0-0x488460) ----------------------------------------------------------------

void HumanPlayer::ExecuteCommand(int command, ObjectKeyValueSet* selection, ObjectKey* key, String* name) {
    OrderBase* order;
    auto toggleDispatchFlag = [&](uint32_t kind) {
        const unsigned state = Dispatch()->GetStateValue(kind);
        Dispatch()->SetStateValue(kind, state ^ 4);
    };
    auto showSelectedObject = [&] {
        ObjectKey selected;
        NullableKey type;
        type.m_value = 0;
        if (AvlNode* first = selection->First()) {
            static_cast<ObjectKeyNode*>(first)->GetKey(&selected);
            if (command == 0x103)
                UiOpenObjectStatus(&selected);
            else
                UiOpenEncyclopedia(&selected, &type);
        }
    };
    auto submitOrQueue = [&](int flag) {
        if (IsQueuedOrder(reinterpret_cast<PlayerMessage*>(order)))
            Dispatch()->QueueConfirmation(reinterpret_cast<MessageBase*>(order));
        else
            SubmitOrder(reinterpret_cast<PlayerMessage*>(order), flag);
    };
    auto runDefault = [&] {
        order = CreateOrder(command, selection, key);
        if (!order) return;
        submitOrQueue(1);
    };

    if (command < 0x101) {
        if (command == 0x100) {
            showSelectedObject();
            return;
        }
        switch (command) {
        case 0x20:
        case 0x25:
            UiBeginSync(0);
            return;
        case 0x21:
        case 0x26:
            SetSpeed(1);
            return;
        case 0x22:
        case 0x27:
            SetSpeed(2);
            return;
        case 0x23:
        case 0x28:
            SetSpeed(3);
            return;
        case 0x24:
        case 0x29:
            SetSpeed(4);
            return;
        }
        runDefault();
        return;
    }
    if (command < 0x113) {
        if (command > 0x10f) {
            m_advisor->SelectTaskMode(command);
            UiBeginTargeting(nullptr);
            return;
        }
        if (command == 0x103) {
            showSelectedObject();
            return;
        }
        runDefault();
        return;
    }
    if (command < 0x171) {
        if (command == 0x170) return;
        switch (command) {
        case 0x113: {
            GameObjNotifier* root = GetGameRootObject(m_side);
            if (!root) return;
            ObjectKey rootKey;
            UiOpenObjectStatus(root->GetKey(&rootKey));
            return;
        }
        case 0x114: {
            ObjectKey unset;
            UiOpenObjectStatus(&unset);
            return;
        }
        case 0x115:
        case 0x116:
        case 0x117:
        case 0x118:
            m_advisor->ToggleTask(command);
            return;
        case 0x119:
        case 0x11a:
        case 0x11b:
        case 0x11e:
            AdvisorToggleUiFlag(command);
            return;
        case 0x11c:
            UiPostOpenType0dWindow(1, 0x79);
            return;
        }
        runDefault();
        return;
    }
    if (command < 0x203) {
        if (command < 0x201) {
            switch (command) {
            case 0x171: toggleDispatchFlag(1); return;
            case 0x172: toggleDispatchFlag(0x80); return;
            case 0x173: toggleDispatchFlag(0x10); return;
            case 0x174: toggleDispatchFlag(4); return;
            case 0x175: toggleDispatchFlag(8); return;
            case 0x176: toggleDispatchFlag(0x40); return;
            case 0x177: toggleDispatchFlag(0x100); return;
            case 0x178: toggleDispatchFlag(0x200); return;
            case 0x179: toggleDispatchFlag(0x20); return;
            }
            runDefault();
            return;
        }
        order = CreateOrder(command, selection, key);
        if (!order) return;
        if ((key->m_value & 0xff000000) == 0) {
            UiBeginTargeting(order);
            return;
        }
        submitOrQueue(0);
        return;
    }
    switch (command) {
    case 0x203:
    case 0x215:
        order = CreateOrder(command, selection, key);
        if (!order) return;
        if (!name) {
            UiBeginRename(reinterpret_cast<TextMessage*>(order));
            return;
        }
        static_cast<TextMessage*>(static_cast<OrderBase*>(order))->m_text.Assign(*name);
        SubmitOrder(reinterpret_cast<PlayerMessage*>(order), 0);
        return;
    case 0x210:
        order = CreateOrder(command, selection, key);
        if (order) UiOpenManufactureWindow(reinterpret_cast<ManufactureOrderBase*>(order), 0x2a);
        return;
    case 0x211:
        order = CreateOrder(command, selection, key);
        if (order) UiOpenManufactureWindow(reinterpret_cast<ManufactureOrderBase*>(order), 0x28);
        return;
    case 0x212:
        order = CreateOrder(command, selection, key);
        if (order) UiOpenManufactureWindow(reinterpret_cast<ManufactureOrderBase*>(order), 0x29);
        return;
    case 0x214:
        order = CreateOrder(command, selection, key);
        if (!order) return;
        if ((key->m_value & 0xff000000) == 0) {
            UiBeginTargeting(order);
            return;
        }
        SubmitOrder(reinterpret_cast<PlayerMessage*>(order), 0);
        return;
    case 0x240:
        order = CreateOrder(command, selection, key);
        if (order) UiBeginTargeting(order);
        return;
    case 0x270: {
        ObjectKey holderKey;
        if (AvlNode* first = selection->First()) {
            static_cast<ObjectKeyNode*>(first)->GetKey(&holderKey);
            GameObjNotifier* holder = GetObjectInFamilies_01_ff(m_side, &holderKey);
            if (holder) {
                do {
                    if (FamilyIn(holderKey, 0x90, 0x98)) break;
                    holder = holder->m_parent;
                    if (!holder) return;
                    ObjectKey parentKey;
                    holderKey = *holder->GetKey(&parentKey);
                } while (holder);
                if (holder) {
                    order = CreateOrder(command, selection, &holderKey);
                    if (order) SubmitOrder(reinterpret_cast<PlayerMessage*>(order), 0);
                }
            }
        }
        return;
    }
    default:
        runDefault();
        return;
    }
}

void HumanPlayer::SubmitOrder(PlayerMessage* order, int flag) {
    if (!order) return;
    IntPair status;
    if (reinterpret_cast<SidedMessage*>(order)->CheckValid(&status) == 0) {
        RefuseOrder(order, &status);
    } else if (flag == 0 && IsQueuedOrder(order)) {
        Dispatch()->QueueConfirmation(reinterpret_cast<MessageBase*>(order));
    } else {
        this->HumanPlayer::SpeakOrderAdvice(order);
        Send(reinterpret_cast<MessageBase*>(order));
        m_advisor->ResetSpeech();
        m_advisor->OnOrderSubmitted(order);
    }
}

void HumanPlayer::NotifyAdvisor(const ObjectKey* key) {
    if (m_advisor) m_advisor->SetTaskState(key);
}

void HumanPlayer::SetVote(int choice, int timed, PlayerMessage* reply) {
    ClearGameTime(&m_voteTime);
    if (choice == 0) {
        m_voteState = 2;
    } else {
        m_voteState = 1;
        if (timed != 0) {
            m_voteTime.m_value = 10000.0f;
            Dispatch()->AddMessage(reinterpret_cast<UiNotice*>(reply));
            return;
        }
    }
    Dispatch()->AddMessage(reinterpret_cast<UiNotice*>(reply));
}

void HumanPlayer::ConfirmVote(PlayerMessage* reply) {
    Dispatch()->AddMessage(reinterpret_cast<UiNotice*>(reply));
    m_voteState = 3;
}

void HumanPlayer::SubmitMessage(PlayerMessage* message) { Send(reinterpret_cast<MessageBase*>(message)); }

void HumanPlayer::FillCommandMenu(EventKindSet* commands) {
    if (!m_advisor) return;
    g_humanUiFlags.Get();
    const uint32_t advisorMask = GetAdvisorCommandMask();
    const uint32_t orderMask = GetAdvisorOrderMask() & advisorMask;
    commands->Add(0x110, orderMask);
    commands->Add(0x111, orderMask);
    commands->Add(0x112, orderMask);
    commands->Add(0x115, advisorMask);
    commands->Add(0x116, advisorMask);
    commands->Add(0x113, 1);
    commands->Add(0x11b, advisorMask);
    commands->Add(0x114, 1);
    commands->Add(0x11e, advisorMask);
    if (g_humanUiFlags.m_value & 0x1000) commands->Find(0x11b)->m_unknown = 1;
    if (!(g_humanUiFlags.Get() & 0x8000)) commands->Find(0x11e)->m_unknown = 1;
    if (m_advisor->IsTaskReady(0x115)) commands->Find(0x115)->m_unknown = 1;
    if (m_advisor->IsTaskReady(0x116)) commands->Find(0x116)->m_unknown = 1;
    if (m_advisor->IsTaskReady(0x117)) commands->Find(0x117)->m_unknown = 1;
}

void HumanPlayer::FillOrderMenu(EventKindSet* commands) {
    if (!m_advisor) return;
    struct Toggle {
        int m_command;
        uint32_t m_kind;
    };
    const Toggle toggles[] = {{0x171, 1},    {0x172, 0x80}, {0x173, 0x10}, {0x174, 4},  {0x175, 8},
                              {0x176, 0x40}, {0x177, 0x100}, {0x179, 0x20}, {0x178, 0x200}};
    commands->Add(0x11c, 1);
    for (const Toggle& toggle : toggles) {
        commands->Add(toggle.m_command, 1);
        if ((Dispatch()->GetStateValue(toggle.m_kind) & 4) == 0) commands->Find(toggle.m_command)->m_unknown = 1;
    }
}

void HumanPlayer::AdvisorReset() {
    if (m_advisor) AdvisorNoteNag();
}

uint32_t HumanPlayer::GetAdvisorCommandMask() {
    uint32_t mask = 0;
    if (m_advisor) mask = static_cast<uint32_t>(m_advisor->m_enabled);
    return mask;
}

uint32_t HumanPlayer::GetAdvisorOrderMask() {
    uint32_t mask = 0;
    if (m_advisor) mask = m_advisor->IsBusy();
    return mask;
}

OrderBase* HumanPlayer::CreateOrder(int kind, ObjectKeyValueSet* team, ObjectKey* target) {
    OrderBase* order = static_cast<OrderBase*>(CreateMessageByKind(kind));
    if (order) {
        order->m_side = m_side;
        order->SetTeam(team);
        order->SetTarget(target);
    }
    return order;
}

void HumanPlayer::RefuseOrder(PlayerMessage* order, IntPair* status) {
    m_advisor->HandleRefusedOrder(reinterpret_cast<OrderBase*>(order), status);
    if (order) delete reinterpret_cast<MessageBase*>(order);
}

int HumanPlayer::IsQueuedOrder(PlayerMessage* order) {
    int result = 0;
    IntPair status;
    SidedMessage* message = reinterpret_cast<SidedMessage*>(order);
    if (message->CheckValid(&status) == 0) return 0;
    switch (message->GetKind()) {
    case 0x201: {
        AvlNode* first = static_cast<OrderBase*>(message)->GetTeam()->First();
        if (!first) break;
        ObjectKey shipKey;
        static_cast<ObjectKeyNode*>(first)->GetKey(&shipKey);
        GameObjNotifier* ship = GetObjectInFamilies_01_ff(m_side, &shipKey);
        GameObjNotifier* system = nullptr;
        uint32_t shipSide = 0;
        if (ship) {
            system = ship->m_parent;
            shipSide = (ship->m_flags >> 6) & 3;
        }
        if (!system) break;
        const uint32_t systemSide = (system->m_flags >> 6) & 3;
        ObjectKey systemKey;
        system->GetKey(&systemKey);
        if (!FamilyIn(systemKey, 0x90, 0x98)) break;
        if ((static_cast<System*>(static_cast<GameObj*>(system))->m_flags & system_flag::kBlockade) && shipSide == systemSide)
            result = 1;
        break;
    }
    case 0x200:
    case 0x202:
    case 0x213:
    case 0x242:
    case 0x250:
        result = 1;
        break;
    default:
        break;
    }
    return result;
}

void HumanPlayer::SetSpeed(int speed) {
    m_speed = speed;
    uint32_t ticks;
    switch (speed) {
    case 1: ticks = 600; break;
    case 2: ticks = 0x3c; break;
    case 3: ticks = 0xc; break;
    case 4: ticks = 4; break;
    default:
        ticks = kDefaultSpeedTicks;
        m_speed = 1;
    }
    if (SidedMessage* message = static_cast<SidedMessage*>(CreateMessageByKind(kSetSpeedKind))) {
        message->m_side = m_side;
        static_cast<NetMsg280*>(message)->m_value = static_cast<int>(ticks);
        SubmitOrder(reinterpret_cast<PlayerMessage*>(message), 0);
    }
}

int HumanPlayer::HasPendingMessages() {
    return m_alertList.Count() != 0;
}

void HumanPlayer::SetAdvisorFlag4000(int on) {
    if (!m_advisor) return;
    if (on != 0)
        g_humanUiFlags.m_value |= 0x4000;
    else
        g_humanUiFlags.m_value &= ~0x4000;
}

int HumanPlayer::NextAdvice() {
    int result = 0;
    if (m_advisor) result = m_advisor->PickAdvice();
    return result;
}

void HumanPlayer::ClearAlerts(unsigned mask) {
    if (m_advisor) m_advisor->SpeakTopic(mask);
}

void HumanPlayer::SetPaused(int paused) {
    const int saved = m_pausedSpeed;
    if (saved != 0 && paused == 0) {
        SetSpeed(saved);
        m_pausedSpeed = 0;
        return;
    }
    if (saved == 0 && paused != 0) {
        m_pausedSpeed = m_speed;
        SetSpeed(1);
    }
}

void HumanPlayer::SpeakOrderAdvice(PlayerMessage* order) {
    uint32_t advice = 0;
    MessageBase* message = reinterpret_cast<MessageBase*>(order);
    const int kind = message->GetKind();
    OrderBase* orderBase = static_cast<OrderBase*>(static_cast<SidedMessage*>(message));
    if (kind == 0x201 || kind == 0x240) {
        if (orderBase->GetTeam()->Count() == 1) {
            ObjectKeyNode* first = static_cast<ObjectKeyNode*>(orderBase->GetTeam()->First());
            const ObjectKey memberKey = first->m_key;
            if (FamilyIn(memberKey, 0x30, 0x38)) {
                bool known = true;
                switch (memberKey.m_value & 0xffffff) {
                case 0x240: advice = 0xf1; break;
                case 0x241: advice = 0xdd; break;
                case 0x242: {
                    GameObjNotifier* object = GetObject_32000242(m_side);
                    if (!(*reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(object) + 0xb0) & 8)) {
                        ObjectKey parentKey;
                        object->m_parent->GetKey(&parentKey);
                        if (FamilyIn(parentKey, 0x90, 0x98) && (parentKey.m_value & 0xffffff) == 0xf9)
                            advice = AdvisorPickAdviceLine(0x131);
                        if (static_cast<int16_t>(advice) == 0) advice = AdvisorPickAdviceLine(0xc9);
                        known = false;
                    } else {
                        advice = 0xc9;
                    }
                    break;
                }
                case 0x243: advice = 0xe7; break;
                case 0x280: advice = 0x105; break;
                case 0x281: advice = 0xfb; break;
                default: known = false;
                }
                if (known) advice = AdvisorPickAdviceLine(advice);
            }
        }
    } else if (kind == 0x250) {
        AvlNode* first = orderBase->GetTeam()->First();
        GameObjNotifier* mission =
            first ? GetObjectInFamilies_40_80(m_side, &static_cast<ObjectKeyNode*>(first)->m_key) : nullptr;
        if (mission) {
            const ObjectKey leaderKey = static_cast<Mission*>(mission)->m_leaderKey;
            if (FamilyIn(leaderKey, 0x30, 0x38)) {
                bool known = true;
                switch (leaderKey.m_value & 0xffffff) {
                case 0x240: advice = 0xf5; break;
                case 0x241: advice = 0xe1; break;
                case 0x242:
                    GetObject_32000242(m_side);
                    advice = 0xcd;
                    break;
                case 0x243: advice = 0xeb; break;
                case 0x280: advice = 0x109; break;
                case 0x281: advice = 0xff; break;
                default: known = false;
                }
                if (known) advice = AdvisorPickAdviceLine(advice);
            }
        }
    }
    if (static_cast<int16_t>(advice) != 0) {
        alignas(8) char result[0x18];
        AdvisorPlayAdvice(reinterpret_cast<SoundRequest*>(result), advice);
    }
}
