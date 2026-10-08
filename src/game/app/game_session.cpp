#include "app/game_session.h"

#include <new>

#include "ai/ai_player.h"
#include "core/checked_new.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));

// Game-clock hooks of the strategic game (game_ui_facade / game_clock_hooks modules).
int FUN_0041d6d0();
int FUN_0041dc10();
int FUN_0041dbf0();
unsigned FUN_0041dba0(unsigned target);

namespace {

constexpr int kRemoteKind = 2;
constexpr int kAiKind = 3;

PlayerController* newOpponent(int opponentType, int side) {
    if (opponentType == 1) {
        void* memory = ::operator new(sizeof(AiPlayer));
        return memory ? new (memory) AiPlayer(side) : nullptr;
    }
    void* memory = ::operator new(sizeof(RemotePlayer));
    return memory ? FUN_00484e70(memory, side) : nullptr;
}

HumanPlayer* newHuman(int side) {
    void* memory = ::operator new(sizeof(HumanPlayer));
    return memory ? new (memory) HumanPlayer(side) : nullptr;
}

}  // namespace

// ---- module static initializer ------------------------------------------------------------------------------

void ConstructSessionRandom() { new (&g_sessionRandom) RandomGenerator(); }

void RegisterSessionRandomDestructor() { atexit(DestroySessionRandom); }

void GameSessionStaticInit() {
    ConstructSessionRandom();
    RegisterSessionRandomDestructor();
}

void DestroySessionRandom() {
    if (!(g_sessionRandomDestroyedGuard & 1)) {
        g_sessionRandomDestroyedGuard |= 1;
        g_sessionRandom.~RandomGenerator();
    }
}

// ---- the singleton ------------------------------------------------------------------------------------------

GameSession* GetGameSession() { return g_gameSession; }

void DestroyGameSession() {
    GameSession* session = g_gameSession;
    g_gameSession = nullptr;
    delete session;
}

void CreateGameSession(int opponentType, int humanSide) {
    DestroyGameSession();
    g_gameSession = NewOrNull<GameSession>(opponentType, humanSide);
    if (g_gameSession && g_gameSession->m_valid == 0) DestroyGameSession();
}

int CreateEmptyGameSession() {
    DestroyGameSession();
    g_gameSession = NewOrNull<GameSession>();
    if (!g_gameSession) return 0;
    if (g_gameSession->m_valid == 0) {
        DestroyGameSession();
        return 0;
    }
    return 1;
}

// ---- construction and persistence ---------------------------------------------------------------------------

GameSession::GameSession() : m_viewReady(0), m_nextId(1), m_activeIndex(0), m_opponentType(0), m_valid(1) {
    m_human = nullptr;
    m_controllers[0] = nullptr;
    m_controllers[1] = nullptr;
}

GameSession::GameSession(int opponentType, int humanSide)
    : m_viewReady(0), m_nextId(1), m_activeIndex(0), m_opponentType(opponentType), m_valid(1) {
    if (humanSide == 1) {
        HumanPlayer* human = newHuman(1);
        m_human = human;
        m_controllers[0] = human;
        m_controllers[1] = newOpponent(m_opponentType, 2);
    } else {
        HumanPlayer* human = newHuman(2);
        m_human = human;
        m_controllers[1] = human;
        m_controllers[0] = newOpponent(m_opponentType, 1);
    }
}

GameSession::~GameSession() {
    m_human = nullptr;
    delete m_controllers[0];
    m_controllers[0] = nullptr;
    delete m_controllers[1];
    m_controllers[1] = nullptr;
}

int GameSession::Read(istream* in) {
    ReadValue32(in, &m_opponentType);
    ReadValue32(in, &m_valid);
    ReadValue32(in, &m_viewReady);
    ReadValue32(in, &m_unk04);
    ReadValue32(in, &m_activeIndex);
    ReadValue32(in, &m_nextId);
    m_messages.Clear();
    int kind;
    ReadValue32(in, &kind);
    int result = 0;
    if (CreateController(kind, 0)) {
        m_controllers[0]->Read(in);
        ReadValue32(in, &kind);
        result = CreateController(kind, 1);
        if (result) m_controllers[1]->Read(in);
    }
    RandomGenerator unusedRandom;
    unusedRandom.Read(in);
    return result;
}

int GameSession::Write(ostream* out) {
    WriteValue32(out, &m_opponentType);
    WriteValue32(out, &m_valid);
    WriteValue32(out, &m_viewReady);
    WriteValue32(out, &m_unk04);
    WriteValue32(out, &m_activeIndex);
    WriteValue32(out, &m_nextId);
    int kind = m_controllers[0]->GetKind();
    WriteValue32(out, &kind);
    m_controllers[0]->Write(out);
    kind = m_controllers[1]->GetKind();
    WriteValue32(out, &kind);
    m_controllers[1]->Write(out);
    g_sessionRandom.Write(out);
    return 1;
}

int GameSession::CreateController(int kind, int slot) {
    if (kind == kPlayerHuman) {
        HumanPlayer* human = nullptr;
        if (void* memory = ::operator new(sizeof(HumanPlayer))) human = new (memory) HumanPlayer();
        m_controllers[slot] = human;
        if (m_human) return 0;
        m_human = human;
        return 1;
    }
    if (kind == kRemoteKind) {
        RemotePlayer* remote = nullptr;
        if (void* memory = ::operator new(sizeof(RemotePlayer))) remote = FUN_00484df0(memory);
        m_controllers[slot] = remote;
        return 1;
    }
    if (kind == kAiKind) {
        AiPlayer* ai = nullptr;
        if (void* memory = ::operator new(sizeof(AiPlayer))) ai = new (memory) AiPlayer();
        m_controllers[slot] = ai;
        return 1;
    }
    return 0;
}

// ---- turn and message flow ----------------------------------------------------------------------------------

int GameSession::AdvanceTurn(int* outRoundComplete) {
    if (!m_controllers[m_activeIndex]->Virtual04()) return 0;
    m_activeIndex = 1 - m_activeIndex;
    if (m_activeIndex != 0) return 0;
    m_unk04 = 1;
    *outRoundComplete = 1;
    return 1;
}

int GameSession::AdvancePhase() {
    int done = m_controllers[m_activeIndex]->Virtual09();
    if (done == 1) {
        m_activeIndex = 1 - m_activeIndex;
        if (m_activeIndex != 0) done = 0;
    }
    return done;
}

void GameSession::Update() {
    if (!m_viewReady) {
        m_viewReady = FUN_0041d6d0();
        return;
    }
    m_controllers[0]->Virtual10();
    m_controllers[1]->Virtual10();
    if (FUN_0041dc10() && m_controllers[0]->IsSynced() && m_controllers[1]->IsSynced()) FUN_0041dbf0();
}

int GameSession::AreAllSynced() { return m_controllers[0]->IsSynced() && m_controllers[1]->IsSynced(); }

int GameSession::ProcessAllMessages() {
    unsigned first = m_controllers[0]->Virtual11();
    unsigned second = m_controllers[1]->Virtual11();
    return first & 1 & second;
}

int GameSession::SyncAllPlayers(unsigned syncId) {
    unsigned first = m_controllers[0]->Virtual12(syncId);
    unsigned second = m_controllers[1]->Virtual12(syncId);
    return first & 1 & second;
}

int GameSession::ResetAllPlayerFlags() {
    unsigned first = m_controllers[0]->Virtual13();
    unsigned second = m_controllers[1]->Virtual13();
    return first & 1 & second;
}

void GameSession::RefreshAllPlayers() {
    m_controllers[0]->Virtual14();
    m_controllers[1]->Virtual14();
}

void GameSession::ComputeAllAdvisorChoices() {
    m_controllers[0]->Virtual21();
    m_controllers[1]->Virtual21();
}

int GameSession::HaveSameSyncValue() { return m_controllers[0]->m_unk4c == m_controllers[1]->m_unk4c; }

int GameSession::NextId() { return m_nextId++; }

void GameSession::RouteMessage(PlayerMessage* message) {
    if (!message) return;
    if (message->m_side == 1) {
        if (m_controllers[1]->Virtual08(message)) {
            m_controllers[0]->Virtual07(message);
            return;
        }
    } else if (message->m_side == 2) {
        if (m_controllers[0]->Virtual08(message)) {
            m_controllers[1]->Virtual07(message);
            return;
        }
    } else {
        delete message;
        return;
    }
    m_messages.Adopt(message);
}

void GameSession::RouteNotice(PlayerNotice* notice) {
    if (!notice) return;
    if (notice->m_side == 1)
        m_controllers[0]->Virtual20(notice);
    else if (notice->m_side == 2)
        m_controllers[1]->Virtual20(notice);
    else
        delete notice;
}

void GameSession::DeliverQueuedMessage(int id) {
    auto* message = static_cast<PlayerMessage*>(m_messages.DetachById(id));
    if (!message) return;
    if (message->m_side == 1)
        m_controllers[0]->Virtual07(message);
    else
        m_controllers[1]->Virtual07(message);
}

AvlNode* GameSession::GetFirstMessage() { return m_messages.First(); }

void GameSession::SendNotificationToSide(int side, int argument) {
    if (side == 1)
        m_controllers[0]->SendKeyRequest(reinterpret_cast<const ObjectKey*>(argument));
    else if (side == 2)
        m_controllers[1]->SendKeyRequest(reinterpret_cast<const ObjectKey*>(argument));
}

// ---- sync points --------------------------------------------------------------------------------------------

int GameSession::BeginSync(unsigned requested) {
    unsigned latest = m_controllers[0]->m_syncId;
    unsigned other = m_controllers[1]->m_syncId;
    if (latest <= other) latest = other;
    unsigned target = requested < latest ? latest + 1 : requested;
    unsigned granted = FUN_0041dba0(target);
    if (granted == 0) return 0;
    m_controllers[0]->Virtual93(granted);
    m_controllers[1]->Virtual93(granted);
    return 1;
}

void GameSession::AckSync(int side) {
    m_controllers[0]->Virtual94(side);
    m_controllers[1]->Virtual94(side);
}

void GameSession::FinishSync(PlayerMessage* message) {
    if (m_human) m_human->Dispatch()->AddMessage(reinterpret_cast<UiNotice*>(message));
    m_controllers[0]->Virtual95();
    m_controllers[1]->Virtual95();
}

void GameSession::SetSyncTargets(unsigned target) {
    m_controllers[0]->SetSyncTarget(target);
    m_controllers[1]->SetSyncTarget(target);
}

// ---- battle-type vote ---------------------------------------------------------------------------------------

void GameSession::BeginVoteAll(int id, ObjectKey* key) {
    m_controllers[0]->Virtual17(id, key);
    m_controllers[1]->Virtual17(id, key);
}

void GameSession::ResetVoteAll(int id, ObjectKey* key) {
    m_controllers[0]->Virtual19(id, key);
    m_controllers[1]->Virtual19(id, key);
}

void GameSession::ExchangeVotesTimed(int* outAllVoted, int* outUnanimous, GameTime* timeA, GameTime* timeB) {
    ObjectKey keyA;
    ObjectKey keyB;
    int idA;
    int idB;
    int stateA = m_controllers[0]->Virtual15(&idA, &keyA, timeA);
    int stateB = m_controllers[1]->Virtual15(&idB, &keyB, timeB);
    if (stateA) m_controllers[1]->Virtual18(idA, &keyA, stateA, timeA);
    if (stateB) m_controllers[0]->Virtual18(idB, &keyB, stateB, timeB);
    if (stateA == 0 || stateB == 0) {
        *outAllVoted = 0;
        *outUnanimous = 0;
    } else {
        *outAllVoted = 1;
        *outUnanimous = (stateA == 1 && stateB == 1) ? stateA : 0;
    }
}

void GameSession::ExchangeVotes(int* outAllConfirmed) {
    ObjectKey keyA;
    ObjectKey keyB;
    GameTime unusedTime;
    unusedTime.m_value = 0;
    int idA;
    int idB;
    int stateA = m_controllers[0]->Virtual16(&idA, &keyA);
    int stateB = m_controllers[1]->Virtual16(&idB, &keyB);
    if (stateA) m_controllers[1]->Virtual18(idA, &keyA, stateA, &unusedTime);
    if (stateB) m_controllers[0]->Virtual18(idB, &keyB, stateB, &unusedTime);
    *outAllConfirmed = (stateA == kPlayerAi && stateB == kPlayerAi) ? 1 : 0;
}

// ---- who is who ---------------------------------------------------------------------------------------------

int GameSession::GetHumanSide() {
    if (!m_human) return 0;
    int side = m_human->m_side;
    if (side == 1) return 1;
    if (side == 2) return 2;
    return 0;
}

int GameSession::IsOpponentAI() {
    int kind = 0;
    if (m_controllers[0] == m_human)
        kind = m_controllers[1]->GetKind();
    else if (m_controllers[1] == m_human)
        kind = m_controllers[0]->GetKind();
    return kind == kPlayerAi;
}

// ---- forwarders to the human controller ---------------------------------------------------------------------

void GameSession::HumanCommand(int command, void* a, void* b, const void* c) {
    if (m_human) m_human->ExecuteCommand(command, static_cast<ObjectKeyValueSet*>(a), static_cast<ObjectKey*>(b), static_cast<String*>(const_cast<void*>(c)));
}

void GameSession::HumanReceiveMessage(PlayerMessage* message, int flag) {
    if (m_human) m_human->SubmitOrder(message, flag);
}

void GameSession::HumanAdvisorNotify(const ObjectKey* key) {
    if (m_human) m_human->NotifyAdvisor(key);
}

void GameSession::HumanSetVote(int choice, int timed, PlayerMessage* reply) {
    if (m_human) m_human->SetVote(choice, timed, reply);
}

void GameSession::HumanConfirmVote(PlayerMessage* reply) {
    if (m_human) m_human->ConfirmVote(reply);
}

void GameSession::SetHumanUiTarget(int a, int b) {
    if (m_human) HumanSetUiTarget(a, b);
}

void GameSession::HumanRequestUpdate() {
    if (m_human) m_human->AdvisorReset();
}

void GameSession::HumanRefreshMenus(int value) {
    if (m_human) m_human->FillCommandMenu(reinterpret_cast<EventKindSet*>(static_cast<uintptr_t>(static_cast<uint32_t>(value))));
}

void GameSession::HumanRefreshOrderMenus(int value) {
    if (m_human) m_human->FillOrderMenu(reinterpret_cast<EventKindSet*>(static_cast<uintptr_t>(static_cast<uint32_t>(value))));
}

int GameSession::HumanHasPending() { return m_human ? m_human->HasPendingMessages() : 0; }

int GameSession::HumanNextAdvice() { return m_human ? m_human->NextAdvice() : 0; }

void GameSession::HumanSetAdvisorMask(unsigned mask) {
    if (m_human) m_human->ClearAlerts(mask);
}

void GameSession::HumanBeginHold() {
    if (m_human) m_human->SetPaused(1);
}

void GameSession::HumanEndHold() {
    if (m_human) m_human->SetPaused(0);
}

void GameSession::SetHumanUiFlag4000(int on) {
    if (m_human) m_human->SetAdvisorFlag4000(on);
}

IdAvlTree* GameSession::GetHumanOrderTree() { return m_human ? &m_human->Dispatch()->m_trees[0] : nullptr; }

void GameSession::DeleteHumanMessage(int id) {
    if (m_human) m_human->Dispatch()->RemoveRecord(id);
}

void GameSession::SetHumanDispatchFlag(int on) {
    if (m_human) m_human->Dispatch()->SetHold(on);
}

void GameSession::SetHumanStateValue(int key, int value) {
    if (m_human) m_human->Dispatch()->SetStateValue(key, value);
}

unsigned GameSession::GetHumanStateValue(int key) { return m_human ? m_human->Dispatch()->GetStateValue(key) : 0; }

int GameSession::GetHumanDefaultState() { return m_human ? m_human->Dispatch()->m_defaultState.Get() : 0; }

void GameSession::SetHumanDefaultState(int state) {
    if (m_human) m_human->Dispatch()->SetDefaultState(state);
}

void GameSession::QueueHumanPopup(int a, int b) {
    if (m_human) m_human->Dispatch()->QueueChat(reinterpret_cast<String*>(a), b);
}

int GameSession::HumanDispatchIdle() { return m_human ? m_human->Dispatch()->IsIdle() : 1; }

void GameSession::ClearHumanAlerts(unsigned mask) {
    if (m_human) m_human->Dispatch()->ClearAlerts(mask);
}

void GameSession::RequestRedraw(int value) { FUN_004878b0(value); }

void GameSession::RequestRedrawAlt(int value) { FUN_004878c0(value); }
