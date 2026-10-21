#include "app/battle_flow.h"

#include <cstdio>
#include <new>

#include "app/battle_corpus.h"
#include "app/battle_engine_space.h"
#include "app/battle_live.h"
#include "app/game_session.h"
#include "app/rebellion_application.h"
#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "events/game_clock_hooks.h"
#include "multiplayer/multiplayer_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/human_player.h"

extern int g_flowRangeSpan;               // 0x6b1240

namespace
{

constexpr int kRangeSlack = 5;

} // namespace

// ---- state names ----------------------------------------------------------------------------------------------------

int GetBattleStateName(int state, std::string* out)
{
  const char* name = nullptr;
  switch (state) {
    case 0:
      name = "NONE";
      break;
    case 1:
      name = "STRAT_INIT";
      break;
    case 2:
      name = "STRAT";
      break;
    case 3:
      name = "STRAT_ALL_BATTLE_START";
      break;
    case 4:
      name = "STRAT_BATTLE_START";
      break;
    case 5:
      name = "STRAT_BATTLE_CHOOSE_TYPE";
      break;
    case 6:
      name = "STRAT_BATTLE_WAIT_FOR_TYPE_CHOICE";
      break;
    case 7:
      name = "STRAT_BATTLE_CONVERT_TO_TACT";
      break;
    case 8:
      name = "STRAT_BATTLE_CONVERT_TO_TACT_ABSTRACT";
      break;
    case 9:
      name = "STRAT_BATTLE_FINISH";
      break;
    case 10:
      name = "STRAT_BATTLE_CHOOSE_DONE";
      break;
    case 11:
      name = "STRAT_BATTLE_WAIT_FOR_DONE_CHOICE";
      break;
    case 12:
      name = "STRAT_ALL_BATTLE_FINISH";
      break;
    case 0x14:
      name = "TACT";
      break;
    case 0x15:
      name = "TACT_ABSTRACT";
      break;
    case 0x16:
      name = "TACT_CONVERT_TO_STRAT";
      break;
    case 0x17:
      name = "TACT_ABSTRACT_CONVERT_TO_STRAT";
      break;
    default:
      break;
  }
  if (!name) {
    *out = "Invalid!";
    return 0;
  }
  *out = StringFromText(name);
  return 1;
}

// ---- game step driver lifetime
// ---------------------------------------------------------------------------------------------

GameStepDriver* BattleFlow::StepDriver() const noexcept
{
  return m_stepDriver.get();
}

int BattleFlow::CreateStepDriver()
{
  m_stepDriver.reset(NewOrNull<GameStepDriver>());
  int ok = 0;
  if (m_stepDriver && m_stepDriver->m_ok != 0) {
    ok = m_stepDriver->m_ok;
  }
  return ok;
}

int BattleFlow::CreateStepDriverFor(GameSettings* settings, int a, int b)
{
  m_stepDriver.reset(NewOrNull<GameStepDriver>());
  if (m_stepDriver) {
    m_stepDriver->InitializeSession(a, b);
  }
  int ok = 0;
  if (m_stepDriver && m_stepDriver->m_ok != 0) {
    ok = m_stepDriver->m_ok;
    m_stepDriver->BeginNewGame(settings);
  }
  return ok;
}

int BattleFlow::DestroyStepDriver()
{
  m_stepDriver.reset();
  return 1;
}

int CreateGameStepDriver()
{
  BattleFlow* flow = GetBattleFlow();
  return flow ? flow->CreateStepDriver() : 0;
}

int CreateGameStepDriverFor(GameSettings* settings, int a, int b)
{
  BattleFlow* flow = GetBattleFlow();
  return flow ? flow->CreateStepDriverFor(settings, a, b) : 0;
}

int DestroyGameStepDriver()
{
  BattleFlow* flow = GetBattleFlow();
  return flow ? flow->DestroyStepDriver() : 1;
}

void InitFlowRangeSpan()
{
  g_flowRangeSpan = g_rectSessionList.right - g_rectSessionList.left - kRangeSlack;
}

// ---- handing a battle to its engine and back ----------------------------------------------------------------

void BattleFlow::ConvertBattle()
{
  m_battleEngine = CreateSpaceBattleEngine();
  if (m_stepDriver.get()) {
    BattleSetup setup;
    setup.m_driver = m_stepDriver.get();
    setup.m_timeA = m_voteTimeA;
    setup.m_timeB = m_voteTimeB;
    setup.m_engine = m_battleEngine.get();
    setup.Convert();
    RecordBattleSnapshot(setup.m_snapshot);
  }
}

void BattleFlow::ApplyBattle()
{
  if (m_stepDriver.get()) {
    BattleResults results;
    results.m_driver = m_stepDriver.get();
    results.m_engine = m_battleEngine.get();
    results.Apply();
  }
  m_liveBattle.reset();
  m_battleEngine.reset();
  if (m_stepDriver && m_stepDriver->Manager()) {
    m_stepDriver->Manager()->RunUntilIdle();
  }
}

// ---- the state machine
// ----------------------------------------------------------------------------------------------------

void BattleFlow::Step(int* stepped)
{
  *stepped = 0;
  const bool strategicState = m_state >= 1 && m_state <= 0xc;
  const bool tacticalState = m_state >= 0x14 && m_state <= 0x17;
  int runnable = strategicState || tacticalState;
  if (!runnable) {
    return;
  }
  if (strategicState) {
    runnable = m_stepDriver.get() != nullptr;
  }
  if (!runnable) {
    return;
  }

  ObjectKey rootKey;
  const int previousState = m_state;
  int allVoted = 1;
  int allConfirmed = 1;
  int unanimous = 1;
  int status = runnable;

  switch (previousState) {
    case kFlowInit:
      if (m_stepDriver.get()->StepSetup(&status)) {
        AppForwardToStrategicWindow();
        m_state = kFlowStrategic;
      }
      break;
    case kFlowStrategic:
      if (m_stepDriver.get()->Step(reinterpret_cast<uint32_t*>(stepped))) {
        m_state = kFlowAllBattleStart;
      }
      break;
    case kFlowAllBattleStart:
      m_stepDriver.get()->m_hold = 1;
      m_state = kFlowBattleStart;
      break;
    case kFlowBattleStart:
      status = m_stepDriver->Manager() ? m_stepDriver->Manager()->StartPendingBattle() : 0;
      if (status != 0) {
        status = m_stepDriver.get()->GetRootKey(&rootKey);
        if (status != 0) {
          m_state = rootKey.IsSet() ? kFlowBattleChooseType : kFlowAllBattleFinish;
        }
      }
      break;
    case kFlowBattleChooseType:
      status = m_stepDriver.get()->GetRootKey(&rootKey);
      if (status != 0) {
        UiBeginVoteAll(GameHookTickCount(), &rootKey);
      }
      m_state = kFlowBattleWaitForType;
      break;
    case kFlowBattleWaitForType:
      m_stepDriver.get()->Step(reinterpret_cast<uint32_t*>(stepped));
      UiExchangeVotesTimed(&allVoted, &unanimous, &m_voteTimeA, &m_voteTimeB);
      if (allVoted != 0) {
        GameSession* session = m_stepDriver->Session();
        const bool takeCommand = session && session->m_human && session->m_human->m_voteState == 2;
        const bool live = takeCommand && !g_isNetworkGame && LiveBattlePresenterAvailable();
        if (takeCommand && !live) {
          std::fprintf(stderr, "battle: Take Command resolved headless: %s\n",
                       g_isNetworkGame ? "LAN battles are not live yet" : "no SDL renderer for the battle screen");
        }
        m_state = live ? kFlowConvertToTactical : kFlowConvertToAbstract;
      }
      break;
    case kFlowConvertToTactical:
      {
        ConvertBattle();
        m_battleEngine->Start();
        const int humanSide = m_stepDriver->Session()->GetHumanSide();
        m_liveBattle = std::make_unique<LiveBattleSession>(*m_battleEngine->LiveBattle(),
            humanSide == 2 ? reb::battle::Side::Empire : reb::battle::Side::Alliance);
        if (!m_liveBattle->Open()) {
          std::fprintf(stderr, "battle: the battle screen did not open; resolving headless\n");
          m_liveBattle->GetImmediateResult();
        }
        AppSetState(kStateBattleSwitch);
        m_state = kFlowTactical;
        break;
      }
    case kFlowConvertToAbstract:
      ConvertBattle();
      m_battleEngine->Start();
      m_state = kFlowTacticalAbstract;
      break;
    case kFlowBattleFinish:
      status = m_stepDriver->Manager() ? m_stepDriver->Manager()->RunFollowUps() : 0;
      m_state = kFlowBattleChooseDone;
      break;
    case kFlowBattleChooseDone:
      status = m_stepDriver.get()->GetRootKey(&rootKey);
      if (status != 0) {
        UiResetVoteAll(GameHookTickCount(), &rootKey);
      }
      m_state = kFlowBattleWaitForDone;
      break;
    case kFlowBattleWaitForDone:
      m_stepDriver.get()->Step(reinterpret_cast<uint32_t*>(stepped));
      UiExchangeVotes(&allConfirmed);
      if (allConfirmed != 0) {
        m_state = kFlowBattleStart;
      }
      break;
    case kFlowAllBattleFinish:
      m_stepDriver.get()->ClearHold();
      UiResumePendingRequest();
      m_state = kFlowStrategic;
      break;
    case kFlowTactical:
      *stepped = 1;
      if (m_liveBattle && m_liveBattle->Advance()) {
        RecordBattleResults(*m_battleEngine);
        m_state = kFlowTacticalToStrategic;
      }
      break;
    case kFlowTacticalAbstract:
      if (m_battleEngine->Advance()) {
        RecordBattleResults(*m_battleEngine);
        m_state = kFlowAbstractToStrategic;
      }
      break;
    case kFlowTacticalToStrategic:
      {
        m_liveBattle.reset();
        ApplyBattle();
        AppSetState(kStateBattleSwitch);
        m_state = kFlowBattleFinish;
        break;
      }
    case kFlowAbstractToStrategic:
      ApplyBattle();
      m_state = kFlowBattleFinish;
      break;
  }
  if (m_state != previousState) {
    std::string stateName;
    GetBattleStateName(m_state, &stateName);
  }
}

int g_flowRangeSpan{};
