#include "live_battle_smoke.h"

#include <algorithm>
#include <cstdio>
#include <variant>

#include "app/app_dialogs.h"
#include "app/battle_flow.h"
#include "app/battle_live.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "events/game_clock_hooks.h"
#include "events/game_manager.h"
#include "fleet/capship.h"
#include "fleet/fight_squad.h"
#include "gameobj/object_queries.h"
#include "lan_battle_smoke.h"
#include "space_battle/battle.h"
#include "space_battle/presenter.h"
#include "strategy_ui/human_player.h"

namespace reb::native {
namespace {

void Step(RebellionApplication& app)
{
  app.m_waitElapsed = 1;
  int stepped = 1;
  AppStepGame(&stepped);
}

bool CheckResults(const battle::BattleSnapshot& snapshot, const battle::BattleResultSet& results)
{
  for (const auto& result : results.ships) {
    const auto& identity = std::get<battle::CapitalShipEntry>(snapshot.units.at(result.unit)).identity;
    const ObjectKey key = ObjectKey::Make(identity.family, identity.id);
    auto* ship = static_cast<CapShip*>(GetMasterInFamilies_14_1c(&key));
    if (!ship || ship->GetHull() - ship->GetHullValueDamage() != result.hull) return false;
  }
  for (const auto& result : results.squadrons) {
    const auto& identity = std::get<battle::SquadronEntry>(snapshot.units.at(result.unit)).identity;
    const ObjectKey key = ObjectKey::Make(identity.family, identity.id);
    auto* squad = static_cast<FightSquad*>(GetMasterInFamilies_01_ff(&key));
    if (!squad || static_cast<int>(squad->GetSquadSize()) - squad->GetSquadSizeDamage() != result.size) return false;
  }
  return true;
}

bool DrawCapture(battle_ui::BattleScreen& screen, const std::string& path)
{
  battle_ui::DrawList frame;
  screen.Build(frame);
  return space_battle::Capture(frame, screen.Screen().width, screen.Screen().height, path);
}

bool SameResults(const battle::BattleResultSet& a, const battle::BattleResultSet& b)
{
  return a.outcome.alliancePresent == b.outcome.alliancePresent
      && a.outcome.empirePresent == b.outcome.empirePresent
      && a.outcome.allianceWithdrew == b.outcome.allianceWithdrew
      && a.outcome.empireWithdrew == b.outcome.empireWithdrew
      && std::equal(a.ships.begin(), a.ships.end(), b.ships.begin(), b.ships.end(),
                    [](const auto& x, const auto& y) {
                      return x.unit == y.unit && x.fate == y.fate && x.hull == y.hull && x.criticals == y.criticals;
                    })
      && std::equal(a.squadrons.begin(), a.squadrons.end(), b.squadrons.begin(), b.squadrons.end(),
                    [](const auto& x, const auto& y) {
                      return x.unit == y.unit && x.fate == y.fate && x.size == y.size && x.carrier == y.carrier;
                    })
      && std::equal(a.deathStars.begin(), a.deathStars.end(), b.deathStars.begin(), b.deathStars.end(),
                    [](const auto& x, const auto& y) {
                      return x.unit == y.unit && x.alive == y.alive && x.withdrew == y.withdrew && x.hull == y.hull;
                    });
}

bool CheckLiveOutro(LiveBattleSession& live, const std::string& savePath, std::string& error)
{
  auto fail = [&](const char* message) { error = message; return false; };
  auto& simulator = live.Simulator();
  auto& screen = live.Screen();
  if (!live.Open()) return fail("Could not open the live presenter for the outro.");
  live.SetObserving(true);
  screen.SetPaused(false);
  for (int frame = 0; frame < simulator.Options().tickLimit + battle::kTicksPerSecond; ++frame) {
    screen.Update(battle::kTickSeconds);
    if (screen.View().decided) break;
  }
  if (!screen.View().decided || screen.View().finished || simulator.Finished())
    return fail("Observed live battle did not enter a decided, unfinished outro.");
  const auto decisiveTick = simulator.Tick();
  const auto results = simulator.Results();
  const auto checkpoint = simulator.SaveState();
  if (!DrawCapture(screen, savePath + ".outro.png")) return fail("Could not capture the live result banner.");
  screen.SetPaused(true);
  screen.Update(10.0);
  if (simulator.Tick() != decisiveTick || simulator.Finished() || !SameResults(results, simulator.Results()))
    return fail("Pausing the live outro did not freeze its clock and results.");
  screen.SetPaused(false);
  for (int frame = 0; frame < battle::kTicksPerSecond * 10 && !simulator.Finished(); ++frame) {
    screen.Update(battle::kTickSeconds);
    if (!SameResults(results, simulator.Results())) return fail("Results changed during the live outro.");
  }
  const auto outroTicks = simulator.Tick() - decisiveTick;
  if (!simulator.Finished() || outroTicks < battle::kTicksPerSecond * 4
      || outroTicks > battle::kTicksPerSecond * 6)
    return fail("The live outro did not finish after approximately five seconds of battle time.");
  auto skipped = battle::Battle::RestoreState(simulator.Snapshot(), simulator.Options(), checkpoint);
  if (!skipped) return fail("Could not restore the decisive checkpoint for Continue.");
  skipped->SetOutroSeconds(5.0f);
  skipped->SkipOutro();
  if (!skipped->Finished() || skipped->Tick() != decisiveTick || !SameResults(results, skipped->Results()))
    return fail("Continue did not finish the outro immediately with unchanged results.");
  auto immediate = battle::Battle::RestoreState(simulator.Snapshot(), simulator.Options(), checkpoint);
  if (!immediate) return fail("Could not restore the decisive checkpoint for Immediate Result.");
  {
    LiveBattleSession immediateSession(*immediate, screen.PlayerSide(), true);
    immediateSession.GetImmediateResult();
    if (!immediate->Finished() || !SameResults(results, immediate->Results()))
      return fail("Immediate Result during the live outro changed results or did not finish.");
  }
  std::fprintf(stderr, "LIVE_BATTLE_SMOKE outro passed: decided tick %lld, duration %lld ticks, results unchanged\n",
               static_cast<long long>(decisiveTick), static_cast<long long>(outroTicks));
  return true;
}

// The quit question is an ordinary game window over a live battle: the battle screen must give it the window until
// it is answered (it used to reopen on the next frame and hide it), and come back when it is cancelled.
bool QuitQuestionStaysUp(RebellionApplication& app, std::string& error)
{
  auto fail = [&](const char* message) { error = message; return false; };
  if (app.m_state != kStateTactical) return fail("A live battle did not leave the application tactical.");
  Step(app);
  if (!space_battle::IsOpen()) return fail("The live battle screen did not open.");
  // Raised while the battle screen is open: the menu's Quit closes the screen first, but a window close or Cmd+Q
  // reaches the game as a posted close request, by which time the screen may have reopened. (The harness has no
  // main window for that request to reach, so the question is raised directly.)
  AppSetState(kStateConfirmQuit);
  for (int frame = 0; frame < 20; ++frame) Step(app);
  if (!g_appDialog) return fail("The quit question did not stay up over the live battle.");
  if (space_battle::IsOpen()) return fail("The live battle screen reopened over the quit question.");
  DestroyAppDialog();
  Step(app);
  if (!space_battle::IsOpen()) return fail("The live battle screen did not return after the quit was cancelled.");
  return true;
}

} // namespace

bool RunLiveBattleSmoke(RebellionApplication& app, const std::string& savePath,
                        const std::function<bool()>& save, const std::function<bool()>& reload,
                        std::string& error)
{
  auto fail = [&](const char* message) { error = message; return false; };
  auto* session = GetGameSession();
  if (!session || !session->m_human) return fail("Live smoke requires a local human controller.");
  session->m_viewReady = 1;
  app.m_state = kStateStrategic;
  app.ShowStrategicWindow(0);
  auto* flow = GetBattleFlow();
  for (int frame = 0; frame < 100000 && flow->StepDriver()->m_phase != 13; ++frame) Step(app);
  if (!PrepareLanBattleSmoke(error)) return false;
  auto* heldDriver = flow->StepDriver();
  bool liveStarted = false;
  for (int frame = 0; frame < 100000; ++frame) {
    if (flow->m_state == kFlowBattleWaitForType) {
      session->m_human->m_voteState = 2;
      session->m_human->m_voteTime.m_value = 0.0f;
    }
    Step(app);
    if (flow->LiveSession()) { liveStarted = true; break; }
    if (flow->m_state == kFlowTacticalAbstract) return fail("Take Command unexpectedly entered headless resolution.");
  }
  if (!liveStarted) return fail("Live battle did not start.");
  const int strategicTick = GetGameTickCount();
  if (flow->StepDriver() != heldDriver || !heldDriver->m_hold) return fail("Strategic driver was not retained and held.");
  auto& live = *flow->LiveSession();
  auto& screen = live.Screen();
  if (!screen.Paused() || live.Simulator().Tick() != 0) return fail("Live briefing did not begin paused at tick zero.");
  if (!DrawCapture(screen, savePath + ".briefing.png")) return fail("Could not capture the live briefing.");
  screen.HandleInput(battle_ui::KeyEvent{battle_ui::Key::Return});
  battle_ui::DrawList engagedFrame;
  screen.Build(engagedFrame);
  screen.HandleInput(battle_ui::KeyEvent{battle_ui::Key::Num1});
  const auto center = screen.Layout().view.Center();
  screen.HandleInput(battle_ui::MouseButtonEvent{center, battle_ui::MouseButton::Right, true});
  screen.HandleInput(battle_ui::MouseButtonEvent{center, battle_ui::MouseButton::Right, false});
  screen.HandleInput(battle_ui::MouseWheel{center, 0.0f, 1.0f});
  screen.HandleInput(battle_ui::KeyEvent{battle_ui::Key::Tab});
  screen.SetPaused(false);
  screen.Update(0.5);
  screen.SetPaused(true);
  if (live.Simulator().Tick() == 0 || live.Simulator().CommandLog().empty())
    return fail("Scripted battle inputs did not advance the simulator and submit an order.");
  const auto hash = live.Simulator().StateHash();
  const auto tick = live.Simulator().Tick();
  const auto commandCount = live.Simulator().CommandLog().size();
  const auto cameraCenter = screen.GetCamera().Center();
  const auto cameraZoom = screen.GetCamera().Zoom();
  const auto cameraView = screen.GetCamera().View();
  const auto selected = screen.GetSelection().Units();
  if (!DrawCapture(screen, savePath + ".live.png")) return fail("Could not capture the live battle.");
  if (!save()) return fail("Mid-battle atomic save failed.");
  if (!reload()) return fail("Mid-battle reload failed.");
  flow = GetBattleFlow();
  session = GetGameSession();
  if (!flow || !flow->LiveSession()) return fail("Saved game did not restore the live session.");
  auto& restored = *flow->LiveSession();
  auto& restoredScreen = restored.Screen();
  if (restored.Simulator().StateHash() != hash || restored.Simulator().Tick() != tick
      || restored.Simulator().CommandLog().size() != commandCount)
    return fail("Simulator checkpoint or command log changed across save/reload.");
  if (restoredScreen.GetCamera().Center().x != cameraCenter.x || restoredScreen.GetCamera().Center().y != cameraCenter.y
      || restoredScreen.GetCamera().Zoom() != cameraZoom || restoredScreen.GetCamera().View() != cameraView
      || restoredScreen.GetSelection().Units() != selected || !restoredScreen.Paused())
    return fail("Battle UI state changed across save/reload.");
  if (GetGameTickCount() != strategicTick || !flow->StepDriver() || !flow->StepDriver()->m_hold)
    return fail("Strategic clock or hold changed during the live battle.");
  if (!DrawCapture(restoredScreen, savePath + ".restored.png")) return fail("Could not capture the restored battle.");
  if (!QuitQuestionStaysUp(app, error)) return false;
  const auto snapshot = restored.Simulator().Snapshot();
  const auto* sameBattle = &restored.Simulator();
  auto immediate = battle::Battle::RestoreState(snapshot, restored.Simulator().Options(), restored.Simulator().SaveState());
  if (!immediate) return fail("Could not restore a live checkpoint for Immediate Result.");
  {
    LiveBattleSession immediateSession(*immediate, restoredScreen.PlayerSide(), true);
    immediateSession.GetImmediateResult();
    for (int frame = 0; frame < 10000 && !immediate->Finished(); ++frame) immediateSession.Advance();
    if (!immediate->Finished() || immediate->Tick() < tick || immediate->CommandLog().size() < commandCount)
      return fail("Immediate Result did not finish an undecided live battle.");
  }
  if (!CheckLiveOutro(restored, savePath, error)) return false;
  if (&restored.Simulator() != sameBattle || !restored.Simulator().Finished() || restored.Simulator().Tick() < tick
      || restored.Simulator().CommandLog().size() < commandCount)
    return fail("The live outro did not finish the existing battle.");
  const auto results = restored.Simulator().Results();
  bool applied = false;
  bool doneVote = false;
  for (int frame = 0; frame < 100000; ++frame) {
    if (flow->m_state == kFlowBattleFinish && !applied) {
      if (!CheckResults(snapshot, results)) return fail("Battle results did not reach strategic ships and squadrons.");
      applied = true;
    }
    if (flow->m_state == kFlowBattleWaitForDone) {
      doneVote = true;
      session->m_human->m_voteState = 3;
      session->m_human->m_voteTime.m_value = 0.0f;
    }
    if (flow->m_state == kFlowStrategic && applied && doneVote) {
      if (flow->LiveSession() || flow->StepDriver()->m_hold) return fail("Live battle did not release strategic play.");
      std::fprintf(stderr, "LIVE_BATTLE_SMOKE passed: saved tick %lld, %zu commands, %zu ships, %zu squadrons\n",
                   static_cast<long long>(tick), commandCount, results.ships.size(), results.squadrons.size());
      return true;
    }
    Step(app);
  }
  return fail("Live battle did not complete results and done-vote flow.");
}

} // namespace reb::native
