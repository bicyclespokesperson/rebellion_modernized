#pragma once
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "space_battle/battle.h"
#include "space_battle/battle_queries.h"
#include "space_battle/battle_state.h"
#include "space_battle/math2d.h"
#include "space_battle/sim_internal.h"

// Synthetic ships and squadrons for the space battle tests. They are invented stat blocks shaped like the archetypes
// (a bow-heavy wedge, an even broadside, a laser picket, ...), not records from the game data.
namespace sbtest
{
using namespace reb::battle;

[[noreturn]] inline void Fail(const char* expression, const char* file, int line)
{
  std::fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
  std::abort();
}

#define SB_CHECK(condition)                                                                                            \
  do {                                                                                                                 \
    if (!(condition)) {                                                                                                \
      ::sbtest::Fail(#condition, __FILE__, __LINE__);                                                                  \
    }                                                                                                                  \
  } while (false)

#define SB_CHECK_NEAR(value, expected, tolerance) SB_CHECK(std::fabs((value) - (expected)) <= (tolerance))

inline UnitIdentity Identity(Side side, const std::string& name, uint32_t typeBits = 0)
{
  static uint32_t nextId = 1;
  UnitIdentity identity;
  identity.family = 0x14;
  identity.id = nextId++;
  identity.side = side;
  identity.typeBits = typeBits;
  identity.name = name;
  return identity;
}

inline CapitalShipEntry BaseShip(Side side, const std::string& name)
{
  CapitalShipEntry ship;
  ship.identity = Identity(side, name);
  ship.hullMax = ship.hull = 2000;
  ship.shieldMax = 300;
  ship.sublight = 4;
  ship.maneuverability = 1;
  ship.detection = 10;
  ship.hyperdrive = 80;
  ship.hyperdriveBackup = 120;
  ship.shieldRecharge = 15;
  ship.weaponRecharge = 18;
  ship.damageControl = 25;
  return ship;
}

inline void SetArc(CapitalShipEntry& ship, Arc arc, int turbolaser, int ion, int laser)
{
  ship.weapons.points[static_cast<size_t>(arc)] = {turbolaser, ion, laser};
}

// Bow-heavy line ship: twice the bow firepower of either broadside.
inline CapitalShipEntry Wedge(Side side, const std::string& name = "Wedge")
{
  CapitalShipEntry ship = BaseShip(side, name);
  ship.hullMax = ship.hull = 2600;
  SetArc(ship, Arc::Fore, 120, 80, 0);
  SetArc(ship, Arc::Aft, 30, 30, 0);
  SetArc(ship, Arc::Port, 60, 40, 0);
  SetArc(ship, Arc::Starboard, 60, 40, 0);
  ship.weapons.range = {50, 36, 0};
  ship.weaponRecharge = 20;
  ship.tractorPower = 3;
  ship.tractorRange = 20;
  ship.fighterCapacity = 6;
  return ship;
}

// Broadside line ship: each broadside nearly twice the bow.
inline CapitalShipEntry Broadside(Side side, const std::string& name = "Broadside")
{
  CapitalShipEntry ship = BaseShip(side, name);
  ship.hullMax = ship.hull = 2300;
  ship.maneuverability = 2;
  SetArc(ship, Arc::Fore, 60, 30, 0);
  SetArc(ship, Arc::Aft, 50, 30, 0);
  SetArc(ship, Arc::Port, 110, 70, 0);
  SetArc(ship, Arc::Starboard, 110, 70, 0);
  ship.weapons.range = {48, 34, 0};
  ship.damageControl = 45;
  ship.tractorPower = 2;
  ship.tractorRange = 20;
  return ship;
}

// Laser picket: lasers in every arc, fast and fragile.
inline CapitalShipEntry Picket(Side side, const std::string& name = "Picket")
{
  CapitalShipEntry ship = BaseShip(side, name);
  ship.hullMax = ship.hull = 520;
  ship.shieldMax = 280;
  ship.sublight = 6;
  ship.maneuverability = 3;
  ship.detection = 25;
  for (Arc arc : {Arc::Fore, Arc::Aft, Arc::Port, Arc::Starboard}) {
    SetArc(ship, arc, 0, 0, 140);
  }
  ship.weapons.range = {0, 0, 18};
  ship.weaponRecharge = 10;
  ship.hyperdriveBackup = 0;
  return ship;
}

inline CapitalShipEntry CarrierShip(Side side, const std::string& name = "Carrier")
{
  CapitalShipEntry ship = BaseShip(side, name);
  ship.hullMax = ship.hull = 950;
  ship.shieldMax = 210;
  ship.sublight = 2;
  SetArc(ship, Arc::Fore, 0, 0, 30);
  SetArc(ship, Arc::Port, 0, 0, 55);
  SetArc(ship, Arc::Starboard, 0, 0, 55);
  ship.weapons.range = {0, 0, 17};
  ship.weaponRecharge = 4;
  ship.fighterCapacity = 6;
  ship.damageControl = 5;
  return ship;
}

inline CapitalShipEntry InterdictorShip(Side side, const std::string& name = "Interdictor")
{
  CapitalShipEntry ship = BaseShip(side, name);
  ship.hullMax = ship.hull = 1250;
  ship.shieldMax = 580;
  SetArc(ship, Arc::Fore, 10, 0, 80);
  SetArc(ship, Arc::Port, 20, 10, 60);
  SetArc(ship, Arc::Starboard, 20, 10, 60);
  ship.weapons.range = {50, 35, 17};
  ship.weaponRecharge = 7;
  ship.gravityWell = 4;
  return ship;
}

inline CapitalShipEntry TransportShip(Side side, const std::string& name = "Transport")
{
  CapitalShipEntry ship = BaseShip(side, name);
  ship.hullMax = ship.hull = 400;
  ship.shieldMax = 120;
  ship.sublight = 2;
  ship.troopCapacity = 2;
  ship.weaponRecharge = 0;
  ship.hyperdrive = 100;
  ship.hyperdriveBackup = 0;
  return ship;
}

inline SquadronEntry BaseSquadron(Side side, const std::string& name, UnitIndex carrier)
{
  SquadronEntry squadron;
  squadron.identity = Identity(side, name);
  squadron.identity.family = 0x1c;
  squadron.carrier = carrier;
  squadron.squadSize = squadron.currentSize = 12;
  squadron.sublight = 8;
  squadron.maneuverability = 5;
  squadron.detection = 12;
  squadron.weaponsPerCraft.points[0] = {0, 0, 6};
  squadron.weaponsPerCraft.range = {0, 0, 12};
  return squadron;
}

// Cheap unshielded interceptor without a hyperdrive.
inline SquadronEntry TieLike(Side side, UnitIndex carrier, const std::string& name = "Tie")
{
  SquadronEntry squadron = BaseSquadron(side, name, carrier);
  squadron.identity.typeBits = 0x581;
  squadron.sublight = 9;
  return squadron;
}

// Shielded multirole fighter with torpedoes and a hyperdrive.
inline SquadronEntry XwingLike(Side side, UnitIndex carrier, const std::string& name = "Xwing")
{
  SquadronEntry squadron = BaseSquadron(side, name, carrier);
  squadron.identity.typeBits = 0x541;
  squadron.shieldPerCraft = 5;
  squadron.hyperdrive = 60;
  squadron.torpedoesPerCraft = 4;
  squadron.torpedoRange = 7;
  squadron.weaponsPerCraft.points[0] = {0, 0, 7};
  return squadron;
}

// Ion-armed bomber.
inline SquadronEntry BomberLike(Side side, UnitIndex carrier, const std::string& name = "Bomber")
{
  SquadronEntry squadron = BaseSquadron(side, name, carrier);
  squadron.identity.typeBits = 0x542;
  squadron.shieldPerCraft = 6;
  squadron.hyperdrive = 60;
  squadron.sublight = 7;
  squadron.maneuverability = 4;
  squadron.torpedoesPerCraft = 8;
  squadron.torpedoRange = 7;
  squadron.weaponsPerCraft.points[0] = {0, 3, 5};
  squadron.weaponsPerCraft.range = {0, 12, 12};
  return squadron;
}

class SnapshotBuilder
{
  public:
  explicit SnapshotBuilder(uint32_t seed = 7)
  {
    m_snapshot.params.systemName = "Test";
    m_snapshot.params.seed = seed;
    m_snapshot.params.heldFirstBy = Side::Alliance;
    for (SideParams& side : m_snapshot.params.sides) {
      side.canRetreat = true;
      side.retreatDays = 10.0f;
    }
  }
  UnitIndex Add(UnitEntry entry)
  {
    m_snapshot.units.push_back(std::move(entry));
    return static_cast<UnitIndex>(m_snapshot.units.size() - 1);
  }
  UnitIndex AddCharacter(Side side, UnitIndex ship, bool admiral, bool commander, int leadership, int combat)
  {
    CharacterEntry character;
    character.identity = Identity(side, admiral ? "Admiral" : "Commander");
    character.identity.family = 0x30;
    character.ship = ship;
    character.admiral = admiral;
    character.commander = commander;
    character.leadership = leadership;
    character.combat = combat;
    return Add(character);
  }
  UnitIndex AddDeathStar(bool completed = true, bool shieldFacility = false)
  {
    DeathStarEntry station;
    station.identity = Identity(Side::Empire, "Death Star");
    station.identity.family = 0x18;
    station.completed = completed;
    station.shieldFacility = shieldFacility;
    station.hullMax = station.hull = 20000;
    station.hangarFree = 10;
    return Add(station);
  }
  SideParams& Side(reb::battle::Side side)
  {
    return m_snapshot.params.sides[static_cast<size_t>(side)];
  }
  BattleParams& Params()
  {
    return m_snapshot.params;
  }
  std::vector<DefenseEntry>& Defenses()
  {
    return m_snapshot.defenses;
  }
  const BattleSnapshot& Snapshot() const
  {
    return m_snapshot;
  }

  private:
  BattleSnapshot m_snapshot;
};

// Both sides commanded by nobody: only doctrine and submitted commands act.
inline BattleOptions Uncommanded()
{
  BattleOptions options;
  options.aiControlled = {false, false};
  return options;
}

inline BattleState& Mutable(Battle& battle)
{
  return const_cast<BattleState&>(battle.State());
}

inline Command MakeCommand(Side side, OrderPayload order, std::vector<UnitIndex> units = {},
                           std::vector<GroupId> groups = {})
{
  Command command;
  command.side = side;
  command.order = std::move(order);
  command.target.units = std::move(units);
  command.target.groups = std::move(groups);
  return command;
}

// Puts a unit at a position and heading, as if it had moved there.
inline void Place(Battle& battle, UnitIndex unit, Vec2 position, float heading)
{
  UnitState& state = Mutable(battle).units[unit];
  state.position = state.previousPosition = position;
  state.heading = state.previousHeading = heading;
  if (ShipState* ship = Mutable(battle).Ship(unit)) {
    ship->desiredHeading = heading;
    ship->goal = position;
    ship->speed = 0.0f;
  }
}

inline double Milliseconds(std::chrono::steady_clock::duration duration)
{
  return std::chrono::duration<double, std::milli>(duration).count();
}

} // namespace sbtest
