#include "app/game_session.h"

#include <memory>
#include <new>

#include "ai/ai_controller_factory.h"
#include "app/ai_preferences.h"
#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "core/memory_stream.h"
#include "events/game_step_driver.h"
#include "gameobj/notif_payload.h"
#include "multiplayer/remote_player.h"
#include "multiplayer/lan_player.h"
#include "port/native/lan_game.h"
#include "orders/order_base.h"
#include "strategy_ui/advisor_droid.h"
#include "strategy_ui/game_ui_facade.h"

namespace
{

constexpr int kRemoteKind = 2;
// Additions to the original session data follow it as tagged blocks, written only when they hold something. Older
// saves have the position marker there, which no realistic save offset can confuse with a tag.
constexpr uint32_t kMissionRepeatsTag = 0x4d505452; // "RTPM"
constexpr uint32_t kMissionRepeatsVersion = 1;

void WriteMissionRepeats(ostream* out, HumanPlayer* human)
{
  if (!human || !human->m_missionRepeats.HasSavedState()) {
    return;
  }
  WriteValue32(out, &kMissionRepeatsTag);
  WriteValue32(out, &kMissionRepeatsVersion);
  human->m_missionRepeats.Write(out);
}

bool ReadMissionRepeats(istream* in, HumanPlayer* human)
{
  if (human) {
    human->m_missionRepeats.ForgetListedReports(*human);
  }
  const long position = istream_tellg(in);
  uint32_t tag = 0;
  ReadValue32(in, &tag);
  if (tag != kMissionRepeatsTag) {
    istream_seekg_pos(in, position);
    return true;
  }
  uint32_t version = 0;
  ReadValue32(in, &version);
  MissionRepeats unused;
  return version == kMissionRepeatsVersion && (human ? human->m_missionRepeats : unused).Read(in);
}

PlayerController* newOpponent(int opponentType, int side)
{
  if (opponentType == kPlayerLan) {
    return new LanPlayer(side);
  }
  if (opponentType == 1) {
    return CreateAiController(PreferredAiKind(side), side);
  }
  void* memory = ::operator new(sizeof(RemotePlayer));
  return memory ? new (memory) RemotePlayer(side) : nullptr;
}

HumanPlayer* newHuman(int side)
{
  void* memory = ::operator new(sizeof(HumanPlayer));
  return memory ? new (memory) HumanPlayer(side) : nullptr;
}

bool CopyControllerState(PlayerController& source, PlayerController& destination)
{
  reb::io::MemoryWriter state;
  source.PlayerController::Write(state.Out());
  if (!state.Good()) {
    return false;
  }
  reb::io::MemoryReader input(state.Bytes());
  destination.PlayerController::Read(input.In());
  return input.Good() && input.AtEnd();
}

} // namespace

// ---- the singleton ------------------------------------------------------------------------------------------

GameSession* GetGameSession()
{
  GameStepDriver* driver = GetGameStepDriver();
  return driver ? driver->Session() : nullptr;
}

void DestroyGameSession()
{
  if (GameStepDriver* driver = GetGameStepDriver()) {
    driver->DestroySession();
  }
}

void CreateGameSession(int opponentType, int humanSide)
{
  if (GameStepDriver* driver = GetGameStepDriver()) {
    driver->CreateSession(opponentType, humanSide);
  }
}

int CreateEmptyGameSession()
{
  GameStepDriver* driver = GetGameStepDriver();
  return driver ? driver->CreateEmptySession() : 0;
}

// ---- construction and persistence ---------------------------------------------------------------------------

GameSession::GameSession()
    : m_viewReady(0)
    , m_unk04(0)
    , m_nextId(1)
    , m_activeIndex(0)
    , m_opponentType(0)
    , m_valid(1)
{
  m_human = nullptr;
  m_controllers[0] = nullptr;
  m_controllers[1] = nullptr;
}

GameSession::GameSession(int opponentType, int humanSide)
    : m_viewReady(0)
    , m_unk04(0)
    , m_nextId(1)
    , m_activeIndex(0)
    , m_opponentType(opponentType)
    , m_valid(1)
{
  if (humanSide == 0 && opponentType == 1) {
    m_human = nullptr;
    m_controllers[0] = CreateAiController(PreferredAiKind(1), 1);
    m_controllers[1] = CreateAiController(PreferredAiKind(2), 2);
  } else if (humanSide == 1) {
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

GameSession::~GameSession()
{
  m_human = nullptr;
  delete m_controllers[0];
  m_controllers[0] = nullptr;
  delete m_controllers[1];
  m_controllers[1] = nullptr;
}

bool GameSession::SwitchHumanSide()
{
  if (!m_human || m_opponentType != 1) {
    return false;
  }
  const int oldIndex = m_human->m_side - 1;
  const int newIndex = 1 - oldIndex;
  if (!AiControllerName(m_controllers[newIndex]->GetKind())) {
    return false;
  }

  auto human = std::make_unique<HumanPlayer>(newIndex + 1);
  std::unique_ptr<PlayerController> ai(CreateAiController(PreferredAiKind(oldIndex + 1), oldIndex + 1));
  if (!ai) {
    return false;
  }
  constexpr int kMaxStartupSteps = 100000;
  int steps = 0;
  while (!ai->StepInitialization()) {
    if (++steps == kMaxStartupSteps) {
      return false;
    }
  }
  steps = 0;
  while (!human->m_advisor->StepStartup()) {
    if (++steps == kMaxStartupSteps) {
      return false;
    }
  }
  if (!CopyControllerState(*m_controllers[oldIndex], *ai) || !CopyControllerState(*m_controllers[newIndex], *human)) {
    return false;
  }
  human->m_speed = m_human->m_speed;
  human->m_pausedSpeed = m_human->m_pausedSpeed;

  delete m_controllers[oldIndex];
  delete m_controllers[newIndex];
  m_controllers[oldIndex] = ai.release();
  m_human = human.release();
  m_controllers[newIndex] = m_human;
  return true;
}

bool GameSession::SetAiControllerKind(int side, int kind)
{
  if (side < 1 || side > 2 || !AiControllerName(kind)) {
    return false;
  }
  const int index = side - 1;
  PlayerController* current = m_controllers[index];
  if (!current || !AiControllerName(current->GetKind())) {
    return false;
  }
  if (current->GetKind() == kind) {
    return true;
  }
  std::unique_ptr<PlayerController> replacement(CreateAiController(kind, side));
  if (!replacement) {
    return false;
  }
  constexpr int kMaxStartupSteps = 100000;
  int steps = 0;
  while (!replacement->StepInitialization()) {
    if (++steps == kMaxStartupSteps) {
      return false;
    }
  }
  if (!CopyControllerState(*current, *replacement)) {
    return false;
  }
  delete current;
  m_controllers[index] = replacement.release();
  return true;
}

bool GameSession::UseLanControllers(int humanSide)
{
  if (humanSide < 1 || humanSide > 2) {
    return false;
  }
  std::unique_ptr<PlayerController> replacements[2];
  for (int index = 0; index < 2; ++index) {
    PlayerController* current = m_controllers[index];
    const bool local = index + 1 == humanSide;
    if (current && current->GetKind() == (local ? kPlayerHuman : kPlayerLan)) {
      continue;
    }
    replacements[index].reset(local ? static_cast<PlayerController*>(newHuman(index + 1))
                                    : new LanPlayer(index + 1));
    if (local) {
      auto* human = static_cast<HumanPlayer*>(replacements[index].get());
      constexpr int kStartupLimit = 100000;
      int steps = 0;
      while (!human->m_advisor->StepStartup()) {
        if (++steps == kStartupLimit) {
          return false;
        }
      }
    }
    if (current && !CopyControllerState(*current, *replacements[index])) {
      return false;
    }
  }
  for (int index = 0; index < 2; ++index) {
    if (replacements[index]) {
      delete m_controllers[index];
      m_controllers[index] = replacements[index].release();
    }
  }
  m_human = static_cast<HumanPlayer*>(m_controllers[humanSide - 1]);
  m_opponentType = kPlayerLan;
  m_viewReady = 1;
  return true;
}

bool GameSession::UseAiControllers(int sideOneKind, int sideTwoKind)
{
  const int kinds[2] = {sideOneKind, sideTwoKind};
  std::unique_ptr<PlayerController> replacements[2];
  for (int index = 0; index < 2; ++index) {
    PlayerController* current = m_controllers[index];
    if (!current || !AiControllerName(kinds[index])) {
      return false;
    }
    if (current->GetKind() == kinds[index]) {
      continue;
    }
    if (current->GetKind() != kPlayerHuman && !AiControllerName(current->GetKind())) {
      return false;
    }
    replacements[index].reset(CreateAiController(kinds[index], index + 1));
    if (!replacements[index]) {
      return false;
    }
    constexpr int kMaxStartupSteps = 100000;
    int steps = 0;
    while (!replacements[index]->StepInitialization()) {
      if (++steps == kMaxStartupSteps) {
        return false;
      }
    }
    if (!CopyControllerState(*current, *replacements[index])) {
      return false;
    }
  }
  for (int index = 0; index < 2; ++index) {
    if (!replacements[index]) {
      continue;
    }
    delete m_controllers[index];
    m_controllers[index] = replacements[index].release();
  }
  m_human = nullptr;
  m_opponentType = 1;
  return true;
}

int GameSession::Read(istream* in)
{
  ReadValue32(in, &m_opponentType);
  ReadValue32(in, &m_valid);
  ReadValue32(in, &m_viewReady);
  ReadValue32(in, &m_unk04);
  ReadValue32(in, &m_activeIndex);
  ReadValue32(in, &m_nextId);
  m_messages.Clear();
  m_messageSides.clear();
  int kind;
  ReadValue32(in, &kind);
  int result = 0;
  if (CreateController(kind, 0)) {
    m_controllers[0]->Read(in);
    ReadValue32(in, &kind);
    result = CreateController(kind, 1);
    if (result) {
      m_controllers[1]->Read(in);
    }
  }
  g_sessionRandom.Read(in);
  if (!ReadMissionRepeats(in, m_human)) {
    iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
  }
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0) {
    result = 0;
  }
  return result;
}

int GameSession::Write(ostream* out)
{
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
  WriteMissionRepeats(out, m_human);
  return 1;
}

int GameSession::CreateController(int kind, int slot)
{
  if (kind == kPlayerLan) {
    m_controllers[slot] = new LanPlayer();
    return 1;
  }
  if (kind == kPlayerHuman) {
    HumanPlayer* human = nullptr;
    if (void* memory = ::operator new(sizeof(HumanPlayer))) {
      human = new (memory) HumanPlayer();
    }
    m_controllers[slot] = human;
    if (m_human) {
      return 0;
    }
    m_human = human;
    return 1;
  }
  if (kind == kRemoteKind) {
    RemotePlayer* remote = nullptr;
    if (void* memory = ::operator new(sizeof(RemotePlayer))) {
      remote = new (memory) RemotePlayer();
    }
    m_controllers[slot] = remote;
    return 1;
  }
  if (AiControllerName(kind)) {
    m_controllers[slot] = CreateAiController(kind, 0);
    return m_controllers[slot] != nullptr;
  }
  return 0;
}

// ---- turn and message flow ----------------------------------------------------------------------------------

int GameSession::AdvanceTurn(int* outRoundComplete)
{
  if (!m_controllers[m_activeIndex]->StepInitialization()) {
    return 0;
  }
  m_activeIndex = 1 - m_activeIndex;
  if (m_activeIndex != 0) {
    return 0;
  }
  m_unk04 = 1;
  *outRoundComplete = 1;
  return 1;
}

int GameSession::AdvancePhase()
{
  int done = m_controllers[m_activeIndex]->StepPlayer();
  if (done == 1) {
    m_activeIndex = 1 - m_activeIndex;
    if (m_activeIndex != 0) {
      done = 0;
    }
  }
  return done;
}

void GameSession::Update()
{
  if (!m_viewReady) {
    m_viewReady = UiIsGalaxyViewShown();
    return;
  }
  m_controllers[0]->ProcessPlayerFrame();
  m_controllers[1]->ProcessPlayerFrame();
  if (UiIsPauseTick() && m_controllers[0]->IsSynced() && m_controllers[1]->IsSynced()) {
    UiEndPause();
  }
}

int GameSession::AreAllSynced()
{
  return m_controllers[0]->IsSynced() && m_controllers[1]->IsSynced();
}

int GameSession::ProcessAllMessages()
{
  unsigned first = m_controllers[0]->DeliverNextMessage();
  unsigned second = m_controllers[1]->DeliverNextMessage();
  return first & 1 & second;
}

int GameSession::SyncAllPlayers(unsigned syncId)
{
  unsigned first = m_controllers[0]->ProcessSyncPoint(syncId);
  unsigned second = m_controllers[1]->ProcessSyncPoint(syncId);
  return first & 1 & second;
}

int GameSession::ResetAllPlayerFlags()
{
  unsigned first = m_controllers[0]->ResetQueueSyncState();
  unsigned second = m_controllers[1]->ResetQueueSyncState();
  return first & 1 & second;
}

void GameSession::RefreshAllPlayers()
{
  m_controllers[0]->RefreshPlayerInterface();
  m_controllers[1]->RefreshPlayerInterface();
}

void GameSession::ComputeAllAdvisorChoices()
{
  m_controllers[0]->AdvanceOrderBudget();
  m_controllers[1]->AdvanceOrderBudget();
}

int GameSession::HaveSameSyncValue()
{
  return m_controllers[0]->m_unk4c == m_controllers[1]->m_unk4c;
}

int GameSession::NextId()
{
  return m_nextId++;
}

void GameSession::RouteMessage(MessageBase* message, int senderSide)
{
  if (!message) {
    return;
  }
  if (auto* game = reb::native::CurrentLanGame(); game && game->Submit(message, senderSide)) {
    return;
  }
  auto* sidedMessage = dynamic_cast<SidedMessage*>(message);
  const int side = sidedMessage ? sidedMessage->m_side : senderSide;
  auto* playerMessage = static_cast<PlayerMessage*>(message);
  if (side == 1) {
    if (m_controllers[1]->RouteOutgoingMessage(playerMessage)) {
      m_controllers[0]->QueueReceivedMessage(playerMessage);
      return;
    }
  } else if (side == 2) {
    if (m_controllers[0]->RouteOutgoingMessage(playerMessage)) {
      m_controllers[1]->QueueReceivedMessage(playerMessage);
      return;
    }
  } else {
    delete message;
    return;
  }
  m_messages.Adopt(message);
  m_messageSides[message->m_id] = side;
}

void GameSession::RouteNotice(NotifPayload* notice)
{
  if (!notice) {
    return;
  }
  if (notice->m_value18 == 1) {
    m_controllers[0]->HandleNotification(notice);
  } else if (notice->m_value18 == 2) {
    m_controllers[1]->HandleNotification(notice);
  } else {
    delete notice;
  }
}

void GameSession::DeliverQueuedMessage(int id)
{
  auto* message = static_cast<MessageBase*>(m_messages.DetachById(id));
  if (!message) {
    return;
  }
  const auto sideEntry = m_messageSides.find(id);
  const int side = sideEntry != m_messageSides.end() ? sideEntry->second : 0;
  if (sideEntry != m_messageSides.end()) {
    m_messageSides.erase(sideEntry);
  }
  auto* playerMessage = static_cast<PlayerMessage*>(message);
  if (side == 1) {
    m_controllers[0]->QueueReceivedMessage(playerMessage);
  } else if (side == 2) {
    m_controllers[1]->QueueReceivedMessage(playerMessage);
  } else {
    delete message;
  }
}

void GameSession::SendNotificationToSide(int side, intptr_t argument)
{
  if (side == 1) {
    m_controllers[0]->SendKeyRequest(reinterpret_cast<const ObjectKey*>(argument));
  } else if (side == 2) {
    m_controllers[1]->SendKeyRequest(reinterpret_cast<const ObjectKey*>(argument));
  }
}

// ---- sync points --------------------------------------------------------------------------------------------

int GameSession::BeginSync(unsigned requested)
{
  if (auto* game = reb::native::CurrentLanGame(); game && game->IsGuest()) {
    return 1;
  }
  unsigned latest = m_controllers[0]->m_syncId;
  unsigned other = m_controllers[1]->m_syncId;
  if (latest <= other) {
    latest = other;
  }
  unsigned target = requested < latest ? latest + 1 : requested;
  unsigned granted = UiRaisePauseTo(target);
  if (granted == 0) {
    return 0;
  }
  m_controllers[0]->RequestSyncTarget(granted);
  m_controllers[1]->RequestSyncTarget(granted);
  return 1;
}

void GameSession::AckSync(int side)
{
  m_controllers[0]->AcknowledgeSync(side);
  m_controllers[1]->AcknowledgeSync(side);
}

void GameSession::FinishSync(UiNotice* message)
{
  if (m_human) {
    m_human->Dispatch()->AddMessage(message);
  }
  m_controllers[0]->OnSyncFinished();
  m_controllers[1]->OnSyncFinished();
}

void GameSession::SetSyncTargets(unsigned target)
{
  m_controllers[0]->SetSyncTarget(target);
  m_controllers[1]->SetSyncTarget(target);
}

// ---- battle-type vote ---------------------------------------------------------------------------------------

void GameSession::BeginVoteAll(int id, ObjectKey* key)
{
  m_controllers[0]->BeginVote(id, key);
  m_controllers[1]->BeginVote(id, key);
}

void GameSession::ResetVoteAll(int id, ObjectKey* key)
{
  m_controllers[0]->ResetVote(id, key);
  m_controllers[1]->ResetVote(id, key);
}

void GameSession::ExchangeVotesTimed(int* outAllVoted, int* outUnanimous, GameTime* timeA, GameTime* timeB)
{
  ObjectKey keyA;
  ObjectKey keyB;
  int idA;
  int idB;
  int stateA = m_controllers[0]->GetVoteWithTime(&idA, &keyA, timeA);
  int stateB = m_controllers[1]->GetVoteWithTime(&idB, &keyB, timeB);
  if (stateA) {
    m_controllers[1]->ForwardVoteResponse(idA, &keyA, stateA, timeA);
  }
  if (stateB) {
    m_controllers[0]->ForwardVoteResponse(idB, &keyB, stateB, timeB);
  }
  if (stateA == 0 || stateB == 0) {
    *outAllVoted = 0;
    *outUnanimous = 0;
  } else {
    *outAllVoted = 1;
    *outUnanimous = (stateA == 1 && stateB == 1) ? stateA : 0;
  }
}

void GameSession::ExchangeVotes(int* outAllConfirmed)
{
  ObjectKey keyA;
  ObjectKey keyB;
  GameTime unusedTime;
  unusedTime.m_value = 0;
  int idA;
  int idB;
  int stateA = m_controllers[0]->GetVote(&idA, &keyA);
  int stateB = m_controllers[1]->GetVote(&idB, &keyB);
  if (stateA) {
    m_controllers[1]->ForwardVoteResponse(idA, &keyA, stateA, &unusedTime);
  }
  if (stateB) {
    m_controllers[0]->ForwardVoteResponse(idB, &keyB, stateB, &unusedTime);
  }
  *outAllConfirmed = (stateA == kPlayerAi && stateB == kPlayerAi) ? 1 : 0;
}

// ---- who is who ---------------------------------------------------------------------------------------------

int GameSession::GetHumanSide()
{
  if (!m_human) {
    return 0;
  }
  int side = m_human->m_side;
  if (side == 1) {
    return 1;
  }
  if (side == 2) {
    return 2;
  }
  return 0;
}

int GameSession::IsOpponentAI()
{
  if (!m_human) {
    return m_controllers[0] && m_controllers[1] && AiControllerName(m_controllers[0]->GetKind())
           && AiControllerName(m_controllers[1]->GetKind());
  }
  int kind = 0;
  if (m_controllers[0] == m_human) {
    kind = m_controllers[1]->GetKind();
  } else if (m_controllers[1] == m_human) {
    kind = m_controllers[0]->GetKind();
  }
  return AiControllerName(kind) != nullptr;
}

// ---- forwarders to the human controller ---------------------------------------------------------------------

void GameSession::HumanCommand(int command, void* a, void* b, const void* c)
{
  if (m_human) {
    m_human->ExecuteCommand(command, static_cast<ObjectKeyValueSet*>(a), static_cast<ObjectKey*>(b),
                            static_cast<std::string*>(const_cast<void*>(c)));
  }
}

void GameSession::HumanReceiveMessage(PlayerMessage* message, int flag)
{
  if (m_human) {
    m_human->SubmitOrder(message, flag);
  }
}

void GameSession::HumanAdvisorNotify(const ObjectKey* key)
{
  if (m_human) {
    m_human->NotifyAdvisor(key);
  }
}

void GameSession::HumanSetVote(int choice, int timed, UiNotice* reply)
{
  if (m_human) {
    m_human->SetVote(choice, timed, reply);
  }
}

void GameSession::HumanConfirmVote(UiNotice* reply)
{
  if (m_human) {
    m_human->ConfirmVote(reply);
  }
}

void GameSession::SetHumanUiTarget(int a, int b)
{
  if (m_human) {
    HumanSetUiTarget(a, b);
  }
}

void GameSession::HumanRequestUpdate()
{
  if (m_human) {
    m_human->AdvisorReset();
  }
}

void GameSession::HumanRefreshMenus(intptr_t value)
{
  if (m_human) {
    m_human->FillCommandMenu(reinterpret_cast<EventKindSet*>(value));
  }
}

void GameSession::HumanRefreshOrderMenus(intptr_t value)
{
  if (m_human) {
    m_human->FillOrderMenu(reinterpret_cast<EventKindSet*>(value));
  }
}

int GameSession::HumanHasPending()
{
  return m_human ? m_human->HasPendingMessages() : 0;
}

int GameSession::HumanNextAdvice()
{
  return m_human ? m_human->NextAdvice() : 0;
}

void GameSession::HumanSetAdvisorMask(unsigned mask)
{
  if (m_human) {
    m_human->ClearAlerts(mask);
  }
}

void GameSession::HumanBeginHold()
{
  if (m_human) {
    m_human->SetPaused(1);
  }
}

void GameSession::HumanEndHold()
{
  if (m_human) {
    m_human->SetPaused(0);
  }
}

void GameSession::SetHumanUiFlag4000(int on)
{
  if (m_human) {
    m_human->SetAdvisorFlag4000(on);
  }
}

UiRecordTree* GameSession::GetHumanOrderTree()
{
  return m_human ? &m_human->Dispatch()->m_trees[0] : nullptr;
}

void GameSession::DeleteHumanMessage(int id)
{
  if (m_human) {
    m_human->Dispatch()->RemoveRecord(id);
  }
}

void GameSession::SetHumanDispatchFlag(int on)
{
  if (m_human) {
    m_human->Dispatch()->SetHold(on);
  }
}

void GameSession::SetHumanStateValue(int key, int value)
{
  if (m_human) {
    m_human->Dispatch()->SetStateValue(key, value);
  }
}

unsigned GameSession::GetHumanStateValue(int key)
{
  return m_human ? m_human->Dispatch()->GetStateValue(key) : 0;
}

int GameSession::GetHumanDefaultState()
{
  return m_human ? m_human->Dispatch()->m_defaultState.Get() : 0;
}

void GameSession::SetHumanDefaultState(int state)
{
  if (m_human) {
    m_human->Dispatch()->SetDefaultState(state);
  }
}

void GameSession::QueueHumanPopup(intptr_t a, int b)
{
  if (m_human) {
    m_human->Dispatch()->QueueChat(reinterpret_cast<std::string*>(a), b);
  }
}

int GameSession::HumanDispatchIdle()
{
  return m_human ? m_human->Dispatch()->IsIdle() : 1;
}

void GameSession::ClearHumanAlerts(unsigned mask)
{
  if (m_human) {
    m_human->Dispatch()->ClearAlerts(mask);
  }
}

void GameSession::RequestRedraw(int value)
{
  AdvisorAllowSpeech();
}

void GameSession::RequestRedrawAlt(int value)
{
  AdvisorSetFlag0();
}
