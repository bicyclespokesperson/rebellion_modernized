#include "events/game_step_driver.h"
#include "core/checked_new.h"
#include "core/icf_stubs.h"

#include "app/app_control_cmds.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "events/game_clock_hooks.h"
#include "events/game_manager.h"
#include "events/net_messages.h"
#include "events/ui_notifs.h"
#include "events/ui_notifs_ext.h"
#include "gameobj/factory_registries.h"
#include "orders/order_base.h"
#include "strategy_ui/game_ui_facade.h"

// Forwarders of the game session (0x41cc00-0x41d3a0 call into 0x435a40's object) and UI glue, declared by address.

extern int g_strategicFactoriesRegistered; // 0x6b14c0

namespace
{

enum Phase
{
  kPhaseAdvanceTick = 0,
  kPhaseSyncA = 1,
  kPhasePauseCheck = 2,
  kPhaseSyncB = 3,
  kPhaseIdle = 4,
  kPhaseScheduler = 5,
  kPhaseEndOfStep = 6,
  kPhaseSyncC = 7,
  kPhaseSyncD = 8,
  kPhaseSyncE = 9,
  kPhaseSyncF = 10,
  kPhaseBattleCheck = 11,
  kPhaseSnapshot = 12,
  kPhaseFinish = 13,
  kSessionTypeSinglePlayer = 1,
};

enum SetupStage
{
  kSetupGame = 1,
  kSetupSession = 2,
  kSetupWaitForUser = 3
};

} // namespace

GameStepDriver::GameStepDriver()
{
  InitFields();
}

void GameStepDriver::InitializeSession(int opponentType, int humanSide)
{
  CreateSession(opponentType, humanSide);
}

GameManager* GameStepDriver::Manager() const noexcept
{
  return m_manager.get();
}

GameSession* GameStepDriver::Session() const noexcept
{
  return m_session.get();
}

void GameStepDriver::CreateManager()
{
  DestroyManager();
  m_manager.reset(NewOrNull<GameManager>());
  if (!m_manager || !m_manager->m_ok)
    DestroyManager();
}

void GameStepDriver::DestroyManager()
{
  m_manager.reset();
}

void GameStepDriver::CreateSession(int opponentType, int humanSide)
{
  DestroySession();
  m_session.reset(NewOrNull<GameSession>(opponentType, humanSide));
  if (m_session && m_session->m_valid == 0)
    DestroySession();
}

int GameStepDriver::CreateEmptySession()
{
  DestroySession();
  m_session.reset(NewOrNull<GameSession>());
  if (!m_session)
    return 0;
  if (m_session->m_valid == 0)
  {
    DestroySession();
    return 0;
  }
  return 1;
}

void GameStepDriver::DestroySession()
{
  m_session.reset();
}

GameStepDriver::~GameStepDriver()
{
  ReleaseResources();
}

int GameStepDriver::InitFields()
{
  m_ok = 1;
  m_hold = 0;
  m_phase = 0;
  m_setupStage = 1;
  m_pauseTick = 0;
  m_unk18 = 0;
  m_unk1c = 0;
  m_setupDone = 0;
  m_dataLocation = GetDataLocation();
  return m_ok;
}

int GameStepDriver::ReleaseResources()
{
  if (m_dataLocation)
  {
    operator delete(m_dataLocation);
    m_dataLocation = nullptr;
  }
  m_session.reset();
  m_manager.reset();
  return 1;
}

int GameStepDriver::Load(istream* in)
{
  RegisterStrategicFactoriesOnce();
  InstallGameManagerHooks();
  int ok = ReleaseResources();
  if (ok)
    ok = InitFields();
  if (ok)
    ok = CheckPositionMarker(in);
  if (ok)
  {
    for (int* field : {&m_ok, &m_phase, &m_setupStage, &m_pauseTick, &m_hold, &m_unk18, &m_unk1c})
    {
      ReadValue32(in, field);
    }
    ok = CheckPositionMarker(in);
    if (ok)
    {
      CreateManager();
      ok = m_manager != nullptr;
      if (ok)
        ok = m_manager->Read(in);
      if (ok)
        ok = CheckPositionMarker(in);
      if (ok)
        ok = UiReadSession(in);
      if (ok)
        ok = CheckPositionMarker(in);
    }
  }
  return ok;
}

int GameStepDriver::Save(ostream* out)
{
  WritePositionMarker(out);
  for (const int* field : {&m_ok, &m_phase, &m_setupStage, &m_pauseTick, &m_hold, &m_unk18, &m_unk1c})
  {
    WriteValue32(out, field);
  }
  WritePositionMarker(out);
  int ok = m_manager != nullptr;
  if (ok)
  {
    ok = m_manager->Write(out);
    if (ok)
    {
      WritePositionMarker(out);
      ok = UiWriteSession(out);
      if (ok)
        WritePositionMarker(out);
    }
  }
  return ok;
}

void GameStepDriver::BeginNewGame(GameSettings* settings)
{
  RegisterStrategicFactoriesOnce();
  m_hold = 1;
  CreateManager();
  if (m_manager)
    m_manager->BeginNewGame(settings);
}

int GameStepDriver::StepSetup(int* status)
{
  *status = 1;
  switch (m_setupStage)
  {
    case kSetupGame:
      if (m_manager->StepSetup(status))
        m_setupStage = kSetupSession;
      break;
    case kSetupSession:
      if (UiAdvanceTurn(status))
      {
        m_setupStage = kSetupWaitForUser;
        return 0;
      }
      break;
    case kSetupWaitForUser:
      if (m_setupDone)
      {
        m_setupStage = 0;
        return 1;
      }
      break;
  }
  return 0;
}

int GameStepDriver::Step(uint32_t* done)
{
  int battlePending = 0;
  switch (m_phase)
  {
    case kPhaseAdvanceTick:
      if (AdvanceClock())
        ++m_phase;
      break;
    case kPhaseSyncA:
      if (PausePhase())
        ++m_phase;
      break;
    case kPhasePauseCheck:
      if (!IsPauseTick())
        ++m_phase;
      break;
    case kPhaseSyncB:
      if (UiProcessAllMessages())
        ++m_phase;
      break;
    case kPhaseIdle:
      if (RunIdlePhase())
        ++m_phase;
      break;
    case kPhaseScheduler:
      if (RunSchedulerPhase())
        ++m_phase;
      break;
    case kPhaseEndOfStep:
      if (RunEndOfStepPhase())
        ++m_phase;
      break;
    case kPhaseSyncC:
      if (UiResetAllPlayerFlags())
        ++m_phase;
      break;
    case kPhaseSyncD:
      if (UiAdvancePhase())
        ++m_phase;
      break;
    case kPhaseSyncE:
      if (!UiHumanHasPending())
        ++m_phase;
      break;
    case kPhaseSyncF:
      if (UiHumanDispatchIdle())
        ++m_phase;
      break;
    case kPhaseBattleCheck:
      if (!UiHaveSameSyncValue())
      {
        if (AppGetGameType() != kSessionTypeSinglePlayer)
          AppShowResumeNotice();
      }
      else if (AppGetGameType() != kSessionTypeSinglePlayer)
      {
        AppPlayClickSound();
        ++m_phase;
      }
      else
      {
        ++m_phase;
      }
      break;
    case kPhaseSnapshot:
      if (CaptureSnapshotPhase())
        ++m_phase;
      break;
    case kPhaseFinish:
      if (FinishPhase(&battlePending))
        m_phase = 0;
      break;
  }
  UiUpdateSession();
  *done = m_phase == 2;
  return battlePending;
}

int GameManagerExists()
{
  return GetGameManager() != nullptr;
}

int GetGameTickCount()
{
  int tick = 0;
  if (GameManagerExists())
    tick = GetGameManager()->m_tickCount;
  return tick;
}

int GameStepDriver::GetRootKey(ObjectKey* out)
{
  if (!m_manager)
    return 0;
  return m_manager->GetRootKey(out);
}

int GameStepDriver::QueryBattle(int side, uint32_t* a, uint32_t* b, int* c, uint32_t* d)
{
  if (!m_manager)
    return 0;
  return m_manager->QueryRootBattle(side, a, b, c, d);
}

int StartPendingGameBattle()
{
  if (!GameManagerExists())
    return 0;
  return GetGameManager()->StartPendingBattle();
}

int RunSchedulerUntilIdle()
{
  if (!GameManagerExists())
    return 0;
  return GetGameManager()->RunUntilIdle();
}

int RunGameFollowUps()
{
  if (!GameManagerExists())
    return 0;
  return GetGameManager()->RunFollowUps();
}

uint32_t GameStepDriver::RaisePauseTo(uint32_t tick)
{
  if (static_cast<uint32_t>(m_pauseTick) < tick)
  {
    m_pauseTick = tick;
    AppPromptPause();
  }
  return m_pauseTick;
}

int GameStepDriver::BeginPause()
{
  if (m_pauseTick == 0)
  {
    m_pauseTick = m_manager ? m_manager->m_tickCount : 0;
    UiSetSyncTargets(m_pauseTick);
  }
  return m_pauseTick;
}

int GameStepDriver::EndPause()
{
  const int atPauseTick = m_pauseTick == (m_manager ? m_manager->m_tickCount : 0);
  if (atPauseTick)
  {
    m_pauseTick = 0;
    AppPlayClickSound();
  }
  return atPauseTick;
}

int GameStepDriver::IsPauseTick()
{
  return (m_manager ? m_manager->m_tickCount : 0) == m_pauseTick;
}

void GameStepDriver::ClearHold()
{
  m_hold = 0;
  AppPlayClickSound();
}

int GameStepDriver::AdvanceClock()
{
  const int waited = AppHasWaitedLongEnough();
  int battlePending = 0;
  if (m_manager)
    battlePending = m_manager->HasPendingBattle();
  if (waited)
  {
    if (battlePending)
    {
      UiResumePendingRequest();
      return 0;
    }
    if (m_hold == 0)
    {
      if (AppHasWaitedLongEnough())
        AppResetWaitTimer();
      if (m_manager)
        m_manager->AdvanceTick();
      return 1;
    }
  }
  if (battlePending == 0)
    return 0;
  UiResumePendingRequest();
  return 0;
}

int GameStepDriver::RunIdlePhase()
{
  return m_manager ? ReturnOne() : 1;
}

int GameStepDriver::RunSchedulerPhase()
{
  return m_manager ? m_manager->RunScheduler() : 1;
}

int GameStepDriver::RunEndOfStepPhase()
{
  return m_manager ? m_manager->EndOfStep() : 1;
}

int GameStepDriver::CaptureSnapshotPhase()
{
  return m_manager ? m_manager->CaptureSnapshot() : 1;
}

int GameStepDriver::PausePhase()
{
  return UiSyncAllPlayers(m_manager ? m_manager->m_tickCount : 0);
}

int IdlePhase()
{
  return GetGameManager() ? ReturnOne() : 1;
}

int SchedulerPhase()
{
  GameManager* manager = GetGameManager();
  return manager ? manager->RunScheduler() : 1;
}

int EndOfStepPhase()
{
  GameManager* manager = GetGameManager();
  return manager ? manager->EndOfStep() : 1;
}

int SnapshotPhase()
{
  GameManager* manager = GetGameManager();
  return manager ? manager->CaptureSnapshot() : 1;
}

int GameStepDriver::FinishPhase(int* battlePending)
{
  UiRefreshAllPlayers();
  if (m_manager)
  {
    if (m_manager->m_dayEnded != 0)
      UiComputeAllAdvisorChoices();
    *battlePending = m_manager->GetRootField70();
    if (*battlePending == 0)
      UiResumePendingRequest();
  }
  return 1;
}

void RegisterStrategicFactoriesOnce()
{
  if (g_strategicFactoriesRegistered == 0)
  {
    RegisterAllFactoriesOnce();
    RegisterKeyedFactories();
    RegisterMultiplayerOrderFactories();
    g_strategicFactoriesRegistered = 1;
  }
}

void RegisterKeyedFactories()
{
  RegisterKeyedFactory(1, CreateUiNotif1);
  RegisterKeyedFactory(0x1c0, CreateUiNotif1c0);
  RegisterKeyedFactory(0x1c1, CreateUiNotif1c1);
  RegisterKeyedFactory(0x1c2, CreateUiNotif1c2);
  RegisterKeyedFactory(0x1c3, CreateUiNotif1c3);
  RegisterKeyedFactory(0x1c4, CreateUiNotif1c4);
  RegisterKeyedFactory(0x1c5, CreateUiNotif1c5);
  RegisterKeyedFactory(0x1c6, CreateUiNotif1c6);
  RegisterKeyedFactory(0x1e2, CreateUiNotif1e2);
  RegisterKeyedFactory(0x360, CreateUiNotif360);
  RegisterKeyedFactory(0x1e1, CreateUiNotif1e1);
  RegisterKeyedFactory(0x1e5, CreateUiNotif1e5);
  RegisterKeyedFactory(0x1e4, CreateUiNotif1e4);
  RegisterKeyedFactory(0x362, CreateUiNotif362);
  RegisterKeyedFactory(0x1e3, CreateUiNotif1e3);
  RegisterKeyedFactory(0x1e0, CreateUiNotif1e0);
  RegisterKeyedFactory(0x361, CreateUiNotif361);
  RegisterKeyedFactory(0x230, CreateUiNotif230);
  RegisterKeyedFactory(0x231, CreateUiNotif231);
  RegisterKeyedFactory(2, CreateUiNotif2);
  RegisterKeyedFactory(0x1a0, CreateUiNotif1a0);
  RegisterKeyedFactory(0x180, CreateUiNotif180);
  RegisterKeyedFactory(0x181, CreateUiNotif181);
  RegisterKeyedFactory(0x182, CreateUiNotif182);
  RegisterKeyedFactory(0x183, CreateUiNotif183);
  RegisterKeyedFactory(0x103, CreateUiNotif103);
  RegisterKeyedFactory(0x100, CreateUiNotif100);
  RegisterKeyedFactory(0x102, CreateUiNotif102);
  RegisterKeyedFactory(0x106, CreateUiNotif106);
  RegisterKeyedFactory(0x107, CreateUiNotif107);
  RegisterKeyedFactory(0x302, CreateUiNotif302);
  RegisterKeyedFactory(0x303, CreateUiNotif303);
  RegisterKeyedFactory(0x304, CreateUiNotif304);
  RegisterKeyedFactory(0x305, CreateUiNotif305);
  RegisterKeyedFactory(0x306, CreateUiNotif306);
  RegisterKeyedFactory(0x104, CreateUiNotif104);
  RegisterKeyedFactory(0x105, CreateUiNotif105);
  RegisterKeyedFactory(0x500, CreateUiNotif500);
  RegisterKeyedFactory(0x300, CreateUiNotif300);
  RegisterKeyedFactory(0x301, CreateUiNotif301);
  RegisterKeyedFactory(0x101, CreateUiNotif101);
  RegisterKeyedFactory(0x212, CreateUiNotif212);
  RegisterKeyedFactory(0x210, CreateUiNotif210);
  RegisterKeyedFactory(0x211, CreateUiNotif211);
  RegisterKeyedFactory(0x161, CreateUiNotif161);
  RegisterKeyedFactory(0x321, CreateUiNotif321);
  RegisterKeyedFactory(0x520, CreateUiNotif520);
  RegisterKeyedFactory(0x320, CreateUiNotif320);
  RegisterKeyedFactory(0x160, CreateUiNotif160);
  RegisterKeyedFactory(0x163, CreateUiNotif163);
  RegisterKeyedFactory(0x162, CreateUiNotif162);
  RegisterKeyedFactory(0x400, CreateUiNotif400);
  RegisterKeyedFactory(0x1f1, CreateUiNotif1f1);
  RegisterKeyedFactory(0x1f0, CreateUiNotif1f0);
  RegisterKeyedFactory(0x370, CreateUiNotif370);
  RegisterKeyedFactory(0x221, CreateUiNotif221);
  RegisterKeyedFactory(0x220, CreateUiNotif220);
  RegisterKeyedFactory(0x200, CreateUiNotif200);
  RegisterKeyedFactory(0x380, CreateUiNotif380);
  RegisterKeyedFactory(0x12b, CreateUiNotif12b);
  RegisterKeyedFactory(0x127, CreateUiNotif127);
  RegisterKeyedFactory(0x120, CreateUiNotif120);
  RegisterKeyedFactory(0x600, CreateUiNotif600);
  RegisterKeyedFactory(0x123, CreateUiNotif123);
  RegisterKeyedFactory(0x121, CreateUiNotif121);
  RegisterKeyedFactory(0x12c, CreateUiNotif12c);
  RegisterKeyedFactory(0x122, CreateUiNotif122);
  RegisterKeyedFactory(0x124, CreateUiNotif124);
  RegisterKeyedFactory(0x129, CreateUiNotif129);
  RegisterKeyedFactory(0x125, CreateUiNotif125);
  RegisterKeyedFactory(0x12a, CreateUiNotif12a);
  RegisterKeyedFactory(0x126, CreateUiNotif126);
  RegisterKeyedFactory(0x128, CreateUiNotif128);
  RegisterKeyedFactory(0x151, CreateUiNotif151);
  RegisterKeyedFactory(0x140, CreateUiNotif140);
  RegisterKeyedFactory(0x141, CreateUiNotif141);
  RegisterKeyedFactory(0x142, CreateUiNotif142);
  RegisterKeyedFactory(0x143, CreateUiNotif143);
  RegisterKeyedFactory(0x144, CreateUiNotif144);
  RegisterKeyedFactory(0x145, CreateUiNotif145);
  RegisterKeyedFactory(0x146, CreateUiNotif146);
  RegisterKeyedFactory(0x147, CreateUiNotif147);
  RegisterKeyedFactory(0x156, CreateUiNotif156);
  RegisterKeyedFactory(0x148, CreateUiNotif148);
  RegisterKeyedFactory(0x149, CreateUiNotif149);
  RegisterKeyedFactory(0x14a, CreateUiNotif14a);
  RegisterKeyedFactory(0x14b, CreateUiNotif14b);
  RegisterKeyedFactory(0x152, CreateUiNotif152);
  RegisterKeyedFactory(0x153, CreateUiNotif153);
  RegisterKeyedFactory(0x154, CreateUiNotif154);
  RegisterKeyedFactory(0x155, CreateUiNotif155);
  RegisterKeyedFactory(0x14c, CreateUiNotif14c);
  RegisterKeyedFactory(0x14d, CreateUiNotif14d);
  RegisterKeyedFactory(0x14e, CreateUiNotif14e);
  RegisterKeyedFactory(0x14f, CreateUiNotif14f);
  RegisterKeyedFactory(0x150, CreateUiNotif150);
  RegisterKeyedFactory(0x340, CreateUiNotif340);
}

void RegisterMultiplayerOrderFactories()
{
  RegisterOrderFactory(0x2, CreateNetMsg2);
  RegisterOrderFactory(0x1, CreateNetMsg1);
  RegisterOrderFactory(0x10, CreateMpLobbySettingsMsg);
  RegisterOrderFactory(0x12, CreateMpPlayerInfoMsg);
  RegisterOrderFactory(0x11, CreateMpChatMsg);
  RegisterOrderFactory(0x13, CreateNetMsg13);
  RegisterOrderFactory(0x4, CreateNetMsg4);
  RegisterOrderFactory(0x15, CreateNetMsg15);
  RegisterOrderFactory(0x14, CreateMpSlotTableMsg);
  RegisterOrderFactory(0x5, CreateNetMsg5);
}

int g_strategicFactoriesRegistered{};
