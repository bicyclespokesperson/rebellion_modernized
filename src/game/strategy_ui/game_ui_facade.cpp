#include "strategy_ui/game_ui_facade.h"

#include "app/game_session.h"
#include "core/utility_classes.h"
#include "events/game_clock_hooks.h"
#include "events/game_step_driver.h"
#include "orders/order_list.h"

// 0x4fd220: queues a notice for the player's manager.
void AddManagerNotice(void* notice);
// 0x4fcf20: the menu commands that apply to `selection` for `player`.
void BuildAvailableOrdersViaManager(int side, ObjectKeyValueSet* team, EventKindSet* orders);

// ---- Game session ------------------------------------------------------------------------------------------

void UiCreateGameSession(int opponentType, int humanSide) { CreateGameSession(opponentType, humanSide); }

int UiAdvanceTurn(int* roundComplete) {
    int result = 1;
    GameSession* session = GetGameSession();
    if (session) result = session->AdvanceTurn(roundComplete);
    return result;
}

int UiReadSession(istream* in) {
    int result = 0;
    GameSession* session = GetGameSession();
    if (session) return session->Read(in);
    CreateEmptyGameSession();
    if (GetGameSession()) result = UiReadSession(in);
    return result;
}

int UiWriteSession(ostream* out) {
    int result = 0;
    GameSession* session = GetGameSession();
    if (session) result = session->Write(out);
    return result;
}

void UiSetSessionViewReady(int ready) {
    GameSession* session = GetGameSession();
    if (session) session->m_viewReady = ready;
}

int UiSessionRandom(int limit) {
    int result = 0;
    if (GetGameSession()) result = g_sessionRandom.Next(limit);
    return result;
}

int UiGetHumanSide() {
    int result = 0;
    GameSession* session = GetGameSession();
    if (session) result = session->GetHumanSide();
    return result;
}

int UiIsOpponentAI() {
    int result = 0;
    GameSession* session = GetGameSession();
    if (session) result = session->IsOpponentAI();
    return result;
}

void UiHumanCommand(int command, void* selection, void* key, int unknown) {
    GameSession* session = GetGameSession();
    if (session) session->HumanCommand(command, selection, key, unknown);
}

void UiHumanReceiveMessage(PlayerMessage* message, int flag) {
    GameSession* session = GetGameSession();
    if (session) session->HumanReceiveMessage(message, flag);
}

void UiHumanAdvisorNotify(int value) {
    GameSession* session = GetGameSession();
    if (session) session->HumanAdvisorNotify(value);
}

void UiHumanSetVote(int choice, int timed, PlayerMessage* reply) {
    GameSession* session = GetGameSession();
    if (session) session->HumanSetVote(choice, timed, reply);
}

void UiHumanConfirmVote(PlayerMessage* reply) {
    GameSession* session = GetGameSession();
    if (session) session->HumanConfirmVote(reply);
}

void UiRequestRedraw(int value) {
    GameSession* session = GetGameSession();
    if (session) session->RequestRedraw(value);
}

void UiRouteMessage(PlayerMessage* message) {
    GameSession* session = GetGameSession();
    if (session) session->RouteMessage(message);
}

void UiDeliverQueuedMessage(int id) {
    GameSession* session = GetGameSession();
    if (session) session->DeliverQueuedMessage(id);
}

int UiAdvancePhase() {
    int result = 0;
    GameSession* session = GetGameSession();
    if (session) result = session->AdvancePhase();
    return result;
}

int UiProcessAllMessages() {
    int result = 1;
    GameSession* session = GetGameSession();
    if (session) result = session->ProcessAllMessages();
    return result;
}

void UiUpdateSession() {
    GameSession* session = GetGameSession();
    if (session) session->Update();
}

int UiAreAllSynced() {
    int result = 0;
    GameSession* session = GetGameSession();
    if (session) result = session->AreAllSynced();
    return result;
}

void UiBeginVoteAll(int id, ObjectKey* key) {
    GameSession* session = GetGameSession();
    if (session) session->BeginVoteAll(id, key);
}

void UiExchangeVotesTimed(int* allVoted, int* unanimous, GameTime* timeA, GameTime* timeB) {
    GameSession* session = GetGameSession();
    if (session) session->ExchangeVotesTimed(allVoted, unanimous, timeA, timeB);
}

void UiResetVoteAll(int id, ObjectKey* key) {
    GameSession* session = GetGameSession();
    if (session) session->ResetVoteAll(id, key);
}

void UiExchangeVotes(int* allConfirmed) {
    GameSession* session = GetGameSession();
    if (session) session->ExchangeVotes(allConfirmed);
}

void UiRefreshAllPlayers() {
    GameSession* session = GetGameSession();
    if (session) session->RefreshAllPlayers();
}

void UiComputeAllAdvisorChoices() {
    GameSession* session = GetGameSession();
    if (session) session->ComputeAllAdvisorChoices();
}

int UiSyncAllPlayers(unsigned syncId) {
    int result = 1;
    GameSession* session = GetGameSession();
    if (session) result = session->SyncAllPlayers(syncId);
    return result;
}

int UiResetAllPlayerFlags() {
    int result = 1;
    GameSession* session = GetGameSession();
    if (session) result = session->ResetAllPlayerFlags();
    return result;
}

AvlNode* UiGetFirstMessage() {
    GameSession* session = GetGameSession();
    if (session) return session->GetFirstMessage();
    return nullptr;
}

int UiHumanHasPending() {
    int result = 0;
    GameSession* session = GetGameSession();
    if (session) result = session->HumanHasPending();
    return result;
}

int UiNextId() {
    int result = 0;
    GameSession* session = GetGameSession();
    if (session) result = session->NextId();
    return result;
}

void UiSetHumanUiTarget(int a, int b) {
    GameSession* session = GetGameSession();
    if (session) session->SetHumanUiTarget(a, b);
}

void UiHumanRefreshMenus(int value, int requestUpdate) {
    GameSession* session = GetGameSession();
    if (session) {
        session->HumanRefreshMenus(value);
        if (requestUpdate) session->HumanRequestUpdate();
    }
}

void UiHumanRefreshOrderMenus(int value) {
    GameSession* session = GetGameSession();
    if (session) session->HumanRefreshOrderMenus(value);
}

IdAvlTree* UiGetHumanOrderTree() {
    GameSession* session = GetGameSession();
    IdAvlTree* result = nullptr;
    if (session) result = session->GetHumanOrderTree();
    return result;
}

void UiDeleteHumanMessage(int id) {
    GameSession* session = GetGameSession();
    if (session) session->DeleteHumanMessage(id);
}

void UiSetHumanDispatchFlag(int on) {
    GameSession* session = GetGameSession();
    if (session) session->SetHumanDispatchFlag(on);
}

void UiSetHumanUiFlag4000(int on) {
    GameSession* session = GetGameSession();
    if (session) session->SetHumanUiFlag4000(on);
}

void UiSetHumanStateValue(int key, int value) {
    GameSession* session = GetGameSession();
    if (session) session->SetHumanStateValue(key, value);
}

unsigned UiGetHumanStateValue(int key) {
    unsigned result = 0;
    GameSession* session = GetGameSession();
    if (session) result = session->GetHumanStateValue(key);
    return result;
}

int UiGetHumanDefaultState() {
    int result = 0;
    GameSession* session = GetGameSession();
    if (session) result = session->GetHumanDefaultState();
    return result;
}

void UiSetHumanDefaultState(int state) {
    GameSession* session = GetGameSession();
    if (session) session->SetHumanDefaultState(state);
}

void UiQueueHumanPopup(int a, int b) {
    GameSession* session = GetGameSession();
    if (session) session->QueueHumanPopup(a, b);
}

void UiHumanSetAdvisorMask(unsigned mask) {
    GameSession* session = GetGameSession();
    if (session) session->HumanSetAdvisorMask(mask);
}

int UiBeginSync(unsigned requested) {
    int result = 0;
    GameSession* session = GetGameSession();
    if (session) result = session->BeginSync(requested);
    return result;
}

void UiAckSync(int side) {
    GameSession* session = GetGameSession();
    if (session) session->AckSync(side);
}

void UiFinishSync(PlayerMessage* message) {
    GameSession* session = GetGameSession();
    if (session) session->FinishSync(message);
}

void UiSetSyncTargets(unsigned target) {
    GameSession* session = GetGameSession();
    if (session) session->SetSyncTargets(target);
}

int UiHumanDispatchIdle() {
    int result = 1;
    GameSession* session = GetGameSession();
    if (session) result = session->HumanDispatchIdle();
    return result;
}

int UiHaveSameSyncValue() {
    int result = 1;
    GameSession* session = GetGameSession();
    if (session) result = session->HaveSameSyncValue();
    return result;
}

int UiHumanNextAdvice() {
    GameSession* session = GetGameSession();
    if (session) return session->HumanNextAdvice();
    return 0;
}

void UiClearHumanAlerts(unsigned mask) {
    GameSession* session = GetGameSession();
    if (session) session->ClearHumanAlerts(mask);
}

void UiHumanBeginHold() {
    GameSession* session = GetGameSession();
    if (session) session->HumanBeginHold();
}

void UiHumanEndHold() {
    GameSession* session = GetGameSession();
    if (session) session->HumanEndHold();
}

void UiSendNotificationToSide(int side, int argument) {
    GameSession* session = GetGameSession();
    if (session) session->SendNotificationToSide(side, argument);
}

// ---- Game step driver ----------------------------------------------------------------------------------------

void UiAddManagerNotice(void* notice) {
    if (notice) AddManagerNotice(notice);
}

unsigned UiRaisePauseTo(unsigned tick) {
    if (g_gameStepDriver) return g_gameStepDriver->RaisePauseTo(tick);
    return tick;
}

int UiBeginPause() {
    if (g_gameStepDriver) return g_gameStepDriver->BeginPause();
    return 0;
}

void UiClearHold() {
    if (g_gameStepDriver) g_gameStepDriver->ClearHold();
}

int UiEndPause() {
    int result = 0;
    if (g_gameStepDriver) result = g_gameStepDriver->EndPause();
    return result;
}

int UiIsPauseTick() {
    int result = 1;
    if (g_gameStepDriver) result = g_gameStepDriver->IsPauseTick();
    return result;
}

int UiGetHold() {
    int result = 1;
    if (g_gameStepDriver) result = g_gameStepDriver->m_hold;
    return result;
}

void UiSetSetupDone() {
    if (g_gameStepDriver) g_gameStepDriver->m_setupDone = 1;
}

void UiBuildAvailableOrders(int player, ObjectKeyValueSet* selection, EventKindSet* commands) {
    BuildAvailableOrdersViaManager(player, selection, commands);
}
