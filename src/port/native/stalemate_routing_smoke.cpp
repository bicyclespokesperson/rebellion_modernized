#include "stalemate_routing_smoke.h"

#include <cstdio>
#include <vector>

#include "app/battle_flow.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "events/game_manager.h"
#include "fleet/capship.h"
#include "fleet/fleet.h"
#include "fleet/fleet_iterator.h"
#include "galaxy/system.h"
#include "gameobj/game_root.h"
#include "gameobj/object_queries.h"

// The strategic half of a both-sides-present battle result, as BattleResults::Apply leaves it for a stalemate without
// losses: the system's winner is the side that held it first, and GameManager::RunFollowUps routes the other side
// away (GameRoot::PrepareBattle, markAutorouting, ApplyEvent307). The simulator is not run: its stalemate outcome and
// the held-first winner are covered by the space battle tests.
namespace reb::native {
namespace {

constexpr uint32_t kUsableFlag = 0x1;
constexpr uint32_t kDestroyedFlag = 0x8;
constexpr uint32_t kEnrouteFlag = 0x10;
constexpr int kAllianceSide = 1;
constexpr int kEmpireSide = 2;

void Step(RebellionApplication& app)
{
  app.m_waitElapsed = 1;
  int stepped = 1;
  AppStepGame(&stepped);
}

ObjectKey KeyOf(GameObjNotifier* object)
{
  ObjectKey key;
  object->GetKey(&key);
  return key;
}

System* ParentSystem(GameObjNotifier* fleet)
{
  GameObjNotifier* parent = fleet ? fleet->m_parent : nullptr;
  if (!parent || parent->GetFamily() < family::kSystem || parent->GetFamily() >= family::kSystem + 8) return nullptr;
  return static_cast<System*>(parent);
}

std::vector<CapShip*> ShipsOf(GameObjNotifier* fleet)
{
  std::vector<CapShip*> ships;
  CapShipIterator iterator(fleet, 1, fleet->ControlSide());
  for (iterator.Begin(); iterator.Current(); iterator.Advance()) {
    ships.push_back(static_cast<CapShip*>(iterator.Current()));
  }
  return ships;
}

std::vector<Fleet*> FleetsInSystems(int side)
{
  std::vector<Fleet*> fleets;
  for (GameObjNotifier* object = GetFirstMasterInFamilies_08_10(); object;
       object = static_cast<GameObj*>(object)->FindNextSibling(&family::kFleets, 1)) {
    auto* fleet = static_cast<Fleet*>(object);
    if (object->ControlSide() == static_cast<uint32_t>(side) && (fleet->m_stateFlags & kUsableFlag) != 0
        && ParentSystem(object) && !ShipsOf(object).empty()) {
      fleets.push_back(fleet);
    }
  }
  return fleets;
}

GameObj* Lookup(const ObjectKey& key)
{
  return static_cast<GameObj*>(GetMasterInFamilies_01_ff(&key));
}

bool Destroyed(const ObjectKey& key)
{
  const GameObj* object = Lookup(key);
  return object == nullptr || (object->m_stateFlags & kDestroyedFlag) != 0;
}

// Whether the ship's fleet is still sitting in `system` rather than destroyed or routed to another one.
bool StaysIn(const ObjectKey& ship, const System* system)
{
  const GameObj* object = Lookup(ship);
  if (object == nullptr || (object->m_stateFlags & kDestroyedFlag) != 0) return false;
  const auto* fleet = static_cast<GameObj*>(object->m_parent);
  return fleet && ParentSystem(object->m_parent) == system && (fleet->m_stateFlags & kEnrouteFlag) == 0;
}

void Describe(const char* label, const ObjectKey& key)
{
  const GameObj* object = Lookup(key);
  if (!object) {
    std::fprintf(stderr, "STALEMATE_ROUTING_SMOKE %s: deleted\n", label);
    return;
  }
  const auto* fleet = static_cast<GameObj*>(object->m_parent);
  const System* system = fleet ? ParentSystem(object->m_parent) : nullptr;
  std::fprintf(stderr, "STALEMATE_ROUTING_SMOKE %s: flags 0x%x, fleet flags 0x%x, system %u\n", label,
               object->m_stateFlags, fleet ? fleet->m_stateFlags : 0u,
               system ? static_cast<unsigned>(KeyOf(const_cast<System*>(system)).m_value) : 0u);
}

// BattleResults' contestedWinner: the battle control field names the side that arrived last.
int HeldFirstWinner(const System* system)
{
  const uint32_t control = (system->m_controlBits >> 6) & 3;
  return control == 3 ? 3 : 2 - (control != 1);
}

} // namespace

bool RunStalemateRoutingSmoke(RebellionApplication& app, std::string& error)
{
  auto fail = [&](const std::string& message) { error = message; return false; };
  GameSession* session = GetGameSession();
  BattleFlow* flow = GetBattleFlow();
  GameManager* manager = GetGameManager();
  if (!session || !flow || !manager || !manager->Root()) return fail("No running campaign.");
  session->m_viewReady = 1;
  app.m_state = kStateStrategic;
  app.ShowStrategicWindow(0);
  for (int frame = 0; frame < 100000 && flow->StepDriver()->m_phase != 13; ++frame) Step(app);

  const std::vector<Fleet*> alliance = FleetsInSystems(kAllianceSide);
  std::vector<Fleet*> empire = FleetsInSystems(kEmpireSide);
  if (alliance.empty() || empire.empty()) return fail("The campaign has no opposing fleets with capital ships.");
  System* system = ParentSystem(alliance.front());
  std::erase_if(empire, [&](Fleet* fleet) { return ParentSystem(fleet) == system; });
  if (empire.size() < 2) return fail("The smoke needs two Empire fleets outside the Alliance fleet's system.");
  empire.resize(2);

  NotifyContext ctx;
  ctx.m_flag04 = 1;
  ctx.m_flag08 = 3;
  ObjectKey systemKey = KeyOf(system);
  for (Fleet* fleet : empire) {
    if (!fleet->Reparent(&systemKey, &ctx)) return fail("Could not move an Empire fleet into the battle system.");
  }
  // The second fleet's ships lose both hyperdrives: they have nowhere they can go.
  std::vector<ObjectKey> routable;
  std::vector<ObjectKey> stranded;
  for (CapShip* ship : ShipsOf(empire[0])) routable.push_back(KeyOf(ship));
  for (CapShip* ship : ShipsOf(empire[1])) {
    if (ship->HasPrimaryHyperdrive()) ship->SetPrimaryHyperdriveCH(1, &ctx);
    if (ship->HasBackupHyperdrive()) ship->SetBackupHyperdriveCH(1, &ctx);
    if (ship->GetTransitSpeed(0) != 0) return fail("Knocking out both hyperdrives left a ship able to jump.");
    stranded.push_back(KeyOf(ship));
  }
  // Squadrons aboard the routed fleet leave with it. Every other one is left in the system, as BattleResults leaves a
  // squadron no carrier took back: one with a hyperdrive is routed on its own, one without is stranded.
  std::vector<ObjectKey> carried;
  std::vector<ObjectKey> leftWithDrive;
  for (CapShip* ship : ShipsOf(empire[0])) {
    std::vector<GameObjNotifier*> squads;
    FightSquadIterator aboard(ship, 1, kEmpireSide);
    for (aboard.Begin(); aboard.Current(); aboard.Advance()) squads.push_back(aboard.Current());
    for (GameObjNotifier* squad : squads) {
      const bool leave = (carried.size() + stranded.size() + leftWithDrive.size()) % 2 == 1;
      if (!leave) {
        carried.push_back(KeyOf(squad));
        continue;
      }
      if (!static_cast<GameObj*>(squad)->Reparent(&systemKey, &ctx)) return fail("Could not leave a squadron behind.");
      (static_cast<GameObj*>(squad)->GetTransitSpeed(0) == 0 ? stranded : leftWithDrive).push_back(KeyOf(squad));
    }
  }
  std::vector<ObjectKey> holders;
  FleetIterator held(system, 1, kAllianceSide);
  for (held.Begin(); held.Current(); held.Advance()) {
    for (CapShip* ship : ShipsOf(held.Current())) holders.push_back(KeyOf(ship));
  }

  if (!system->RecomputeBlockadeAndBattlePending(&ctx) || (system->m_flags & system_flag::kBattlePending) == 0)
    return fail("The opposing fleets did not create a pending battle.");
  GameRoot* root = manager->Root();
  root->StartPendingBattle();
  if (root->m_key8c.m_value != systemKey.m_value) return fail("The pending battle was not started in the system.");
  const int winner = HeldFirstWinner(system);
  if (winner != kAllianceSide) return fail("The Alliance, there first, does not hold the contested system.");
  system->SetDataFlag4(1);
  system->SetData64(winner);
  manager->RunFollowUps();

  for (const ObjectKey& key : routable) Describe("routed", key);
  for (const ObjectKey& key : stranded) Describe("stranded", key);
  for (const ObjectKey& key : holders) Describe("holder", key);
  for (const ObjectKey& key : holders) {
    if (!StaysIn(key, system)) return fail("A ship of the side holding the system was moved or destroyed.");
  }
  for (const ObjectKey& key : routable) {
    if (Destroyed(key)) return fail("A loser's ship that could jump was destroyed instead of routed away.");
    if (StaysIn(key, system)) return fail("A loser's ship that could jump was not routed away.");
  }
  for (const ObjectKey& key : stranded) {
    if (!Destroyed(key)) return fail("A loser's ship without a working hyperdrive survived with nowhere to go.");
  }
  for (const ObjectKey& key : carried) {
    if (Destroyed(key)) return fail("A squadron aboard a routed carrier was destroyed.");
  }
  for (const ObjectKey& key : leftWithDrive) {
    const GameObj* squad = Lookup(key);
    if (Destroyed(key)) return fail("A squadron with a hyperdrive was destroyed instead of routed away.");
    if (squad->m_parent == system && (squad->m_stateFlags & kEnrouteFlag) == 0)
      return fail("A squadron with a hyperdrive was not routed away.");
  }
  // Nothing of the loser may stay behind to restart the battle, not even the stranded ships' emptied fleet.
  if (system->HasFleetsOrFighterSquadrons(kEmpireSide)) return fail("The loser still has units in the system.");
  if (!system->HasFleetsOrFighterSquadrons(kAllianceSide)) return fail("The holder lost its place in the system.");
  for (int frame = 0; frame < 200; ++frame) Step(app);
  if ((system->m_flags & system_flag::kBattlePending) != 0) return fail("The battle is pending again.");
  for (const ObjectKey& key : routable) {
    if (Destroyed(key)) return fail("A routed ship was lost on the way out.");
  }
  std::fprintf(stderr,
               "STALEMATE_ROUTING_SMOKE passed: %zu ships routed with %zu squadrons aboard, %zu squadrons routed alone, "
               "%zu stranded units destroyed, %zu holder ships kept\n",
               routable.size(), carried.size(), leftWithDrive.size(), stranded.size(), holders.size());
  return true;
}

} // namespace reb::native
