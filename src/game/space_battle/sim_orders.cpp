#include <algorithm>
#include <cmath>

#include "space_battle/doctrine.h"
#include "space_battle/deployment_geometry.h"
#include "space_battle/math2d.h"
#include "space_battle/sim_internal.h"

namespace reb::battle::sim
{
namespace
{
using namespace tuning;

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};

// Every unit the command names, directly or through its groups, sorted and unique.
std::vector<UnitIndex> SelectedUnits(const BattleState& state, const CommandTarget& target)
{
  std::vector<UnitIndex> units = target.units;
  for (GroupId id : target.groups) {
    if (const GroupState* group = state.Group(id)) {
      units.insert(units.end(), group->members.begin(), group->members.end());
    }
  }
  std::sort(units.begin(), units.end());
  units.erase(std::unique(units.begin(), units.end()), units.end());
  return units;
}

bool AnyUnit(const BattleState& state, const std::vector<UnitIndex>& units, auto&& predicate)
{
  return std::any_of(units.begin(), units.end(), [&](UnitIndex unit) { return predicate(state.units[unit], unit); });
}

bool IsEnemyCombatant(const BattleState& state, Side side, UnitIndex unit)
{
  return state.Valid(unit) && state.units[unit].side != side
         && (state.units[unit].kind == UnitKind::CapitalShip || state.units[unit].kind == UnitKind::Squadron);
}

bool IsEnemyShip(const BattleState& state, Side side, UnitIndex unit)
{
  return state.Valid(unit) && state.units[unit].side != side && state.units[unit].kind == UnitKind::CapitalShip;
}

bool IsFriendly(const BattleState& state, Side side, UnitIndex unit)
{
  return state.Valid(unit) && state.units[unit].side == side
         && (state.units[unit].kind == UnitKind::CapitalShip || state.units[unit].kind == UnitKind::Squadron);
}

UnitIndex SideStation(const BattleState& state, Side side)
{
  for (const StationState& station : state.stations) {
    if (state.units[station.unit].side == side && state.InSpace(station.unit)) {
      return station.unit;
    }
  }
  return kNoUnit;
}

UnitIndex EnemyStation(const BattleState& state, Side side)
{
  return SideStation(state, Opponent(side));
}

bool TargetsValid(const BattleState& state, const Command& command)
{
  for (UnitIndex unit : command.target.units) {
    if (!state.Valid(unit) || state.units[unit].side != command.side || state.units[unit].kind == UnitKind::Passive) {
      return false;
    }
  }
  for (GroupId id : command.target.groups) {
    const GroupState* group = state.Group(id);
    if (group == nullptr || group->side != command.side) {
      return false;
    }
  }
  return true;
}

UnitOrder MakeOrder(OrderKind kind)
{
  UnitOrder order;
  order.kind = kind;
  return order;
}

Vec2 Centroid(const BattleState& state, const std::vector<UnitIndex>& units)
{
  Vec2 sum;
  int count = 0;
  for (UnitIndex unit : units) {
    if (state.InSpace(unit)) {
      sum += state.units[unit].position;
      ++count;
    }
  }
  return count > 0 ? sum * (1.0f / static_cast<float>(count)) : sum;
}

// Movement-type orders go to the groups named (members follow in formation) and to the units named (which then act
// on their own until their orders run out).
void QueueOrder(BattleState& state, const Command& command, UnitOrder order, auto&& applies)
{
  for (GroupId id : command.target.groups) {
    GroupState& group = state.groups[id];
    if (!AnyUnit(state, group.members, applies)) {
      continue;
    }
    UnitOrder groupOrder = order;
    if (groupOrder.kind == OrderKind::Hold) {
      groupOrder.position = Centroid(state, group.members);
    }
    if (!command.queue) {
      group.orders.clear();
      for (UnitIndex member : group.members) {
        state.units[member].orders.clear();
      }
    }
    group.orders.push_back(groupOrder);
    group.committed = true;
  }
  for (UnitIndex unit : command.target.units) {
    if (!applies(state.units[unit], unit)) {
      continue;
    }
    UnitOrder unitOrder = order;
    if (unitOrder.kind == OrderKind::Hold) {
      unitOrder.position = state.units[unit].position;
    }
    if (!command.queue) {
      state.units[unit].orders.clear();
    }
    state.units[unit].orders.push_back(unitOrder);
  }
}

bool IsShip(const UnitState& unit, UnitIndex)
{
  return unit.kind == UnitKind::CapitalShip;
}

bool IsSquadron(const UnitState& unit, UnitIndex)
{
  return unit.kind == UnitKind::Squadron;
}

bool IsCombatant(const UnitState& unit, UnitIndex)
{
  return unit.kind == UnitKind::CapitalShip || unit.kind == UnitKind::Squadron;
}

// Units moved together keep their places relative to the selection's centroid, turned to the requested facing.
void MoveSelection(BattleState& state, const Command& command, const UnitOrder& order)
{
  std::vector<UnitIndex> units;
  Vec2 centroid;
  Vec2 heading;
  for (UnitIndex unit : command.target.units) {
    if (state.InSpace(unit) && IsCombatant(state.units[unit], unit)) {
      units.push_back(unit);
      centroid += state.units[unit].position;
      heading += FromAngle(state.units[unit].heading);
    }
  }
  if (units.empty()) {
    return;
  }
  centroid = centroid * (1.0f / static_cast<float>(units.size()));
  const float turn = order.hasFacing ? AngleDelta(AngleOf(heading), order.facing) : 0.0f;
  for (UnitIndex unit : units) {
    UnitOrder own = order;
    own.position = Clamped(order.position + Rotated(state.units[unit].position - centroid, turn), kBattleHalfExtent);
    if (!command.queue) {
      state.units[unit].orders.clear();
    }
    state.units[unit].orders.push_back(own);
  }
}

// Briefing: the selection keeps its shape, centred on the chosen point, inside the side's deployment zone.
void Deploy(BattleState& state, Side side, const std::vector<UnitIndex>& selected, const OrderDeploy& order)
{
  std::vector<UnitIndex> units;
  Vec2 centroid;
  Vec2 heading;
  for (UnitIndex unit : selected) {
    if (state.InSpace(unit)) {
      units.push_back(unit);
      centroid += state.units[unit].position;
      heading += FromAngle(state.units[unit].heading);
    }
  }
  if (units.empty()) {
    return;
  }
  centroid = centroid * (1.0f / static_cast<float>(units.size()));
  float turn = order.facing ? AngleDelta(AngleOf(heading), *order.facing) : 0.0f;
  const DeploymentBounds bounds{DeploymentCenter(state, side), state.sides[SideIndex(side)].deploymentRadius,
                                state.planetCenter, kPlanetRadius, kBattleHalfExtent};
  std::vector<DeploymentHull> hulls;
  const auto makeHulls = [&](float angle) {
    hulls.clear();
    for (UnitIndex index : units) {
      const bool solid = state.units[index].kind != UnitKind::Squadron;
      hulls.push_back({Rotated(state.units[index].position - centroid, angle),
                       solid ? state.Profile(index).radius : 0.0f, solid});
    }
  };
  makeHulls(turn);
  Vec2 anchor = centroid;
  bool fits = DeploymentFits(hulls, anchor, bounds) || FindDeploymentAnchor(hulls, anchor, bounds);
  if (!fits && turn != 0.0f) {
    // Some orientations cannot fit the zone at any translation. Keep the largest feasible part of the turn.
    const float requested = turn;
    float low = 0.0f;
    float high = 1.0f;
    for (int i = 0; i < 20; ++i) {
      const float fraction = (low + high) * 0.5f;
      makeHulls(requested * fraction);
      Vec2 candidate = centroid;
      if (FindDeploymentAnchor(hulls, candidate, bounds)) {
        low = fraction;
      } else {
        high = fraction;
      }
    }
    turn = requested * low;
    makeHulls(turn);
    anchor = centroid;
    fits = DeploymentFits(hulls, anchor, bounds) || FindDeploymentAnchor(hulls, anchor, bounds);
  }
  const Vec2 center = fits ? ClampDeploymentDrag(hulls, anchor, order.position, bounds) : order.position;
  for (UnitIndex index : units) {
    UnitState& unit = state.units[index];
    Vec2 position = center + Rotated(unit.position - centroid, turn);
    if (!fits) {
      const DeploymentHull hull{{}, unit.kind == UnitKind::Squadron ? 0.0f : state.Profile(index).radius,
                                unit.kind != UnitKind::Squadron};
      FindDeploymentAnchor(std::span{&hull, 1}, position, bounds);
    }
    const Vec2 oldPosition = unit.position;
    unit.position = unit.previousPosition = position;
    unit.heading = unit.previousHeading = WrapAngle(unit.heading + turn);
    if (ShipState* ship = state.Ship(index)) {
      ship->goal = position;
      ship->desiredHeading = unit.heading;
    }
    if (SquadronState* squadron = state.Squadron(index)) {
      squadron->anchor = position + Rotated(squadron->anchor - oldPosition, turn);
      squadron->centroid = position + Rotated(squadron->centroid - oldPosition, turn);
      for (int i = 0; i < squadron->craftCount; ++i) {
        CraftState& craft = state.craft[squadron->firstCraft + i];
        craft.position = position + Rotated(craft.position - oldPosition, turn);
        craft.previousPosition = craft.position;
        craft.heading = craft.previousHeading = WrapAngle(craft.heading + turn);
      }
    }
    if (GroupState* group = state.Group(unit.group)) {
      group->hasGoal = false;
    }
  }
}

void ApplySignal(BattleState& state, Side side, const OrderSignal& signal)
{
  SideState& self = state.sides[SideIndex(side)];
  --self.signalsReady;
  if (self.signalRechargeTicks <= 0) {
    self.signalRechargeTicks = Ticks(kSignalRechargeSeconds);
  }
  switch (signal.signal) {
    case SignalKind::ConcentrateFire:
      self.concentrateTarget = signal.target;
      self.concentrateTicks = Ticks(kConcentrateSeconds);
      LogEvent(state, side, "Signal: concentrate fire on " + UnitName(state, signal.target), signal.target);
      break;
    case SignalKind::HoldTheLine:
      self.holdLineTicks = Ticks(kHoldTheLineSeconds);
      LogEvent(state, side, "Signal: hold the line");
      break;
    case SignalKind::Scramble:
      self.scrambleTicks = Ticks(kScrambleSeconds);
      LogEvent(state, side, "Signal: scramble all fighters");
      break;
    case SignalKind::CoordinatedJump:
      self.coordinatedJumpTicks = Ticks(kCoordinatedJumpSeconds);
      // Ships already spooling share in it too; ships that start within the window spool faster (sim_special.cpp).
      for (ShipState& ship : state.ships) {
        if (ship.withdrawing && state.units[ship.unit].side == side && state.InSpace(ship.unit)) {
          const auto remaining = static_cast<float>(ship.spoolTotal - ship.spoolTicks);
          ship.spoolTotal = ship.spoolTicks + static_cast<int32_t>(remaining * (1.0f - kCoordinatedJumpSpeedup));
        }
      }
      LogEvent(state, side, "Signal: coordinated jump");
      break;
  }
}

std::string NextGroupName(const BattleState& state, Side side, bool flight)
{
  int count = 0;
  for (const GroupState& group : state.groups) {
    count += group.side == side && group.flight == flight ? 1 : 0;
  }
  return flight ? "Flight " + std::to_string(count + 1) : "Task Force " + std::to_string(count + 1);
}

void AssignGroup(BattleState& state, const Command& command, GroupId target)
{
  const std::vector<UnitIndex> units = SelectedUnits(state, command.target);
  if (units.empty()) {
    return;
  }
  const bool flight = state.units[units.front()].kind == UnitKind::Squadron;
  if (target == kNoGroup) {
    const GroupState* from = state.Group(state.units[units.front()].group);
    target = CreateGroup(state, command.side, flight, NextGroupName(state, command.side, flight));
    if (from != nullptr) {
      state.groups[target].shipDoctrine = from->shipDoctrine;
      state.groups[target].flightDoctrine = from->flightDoctrine;
    }
    state.groups[target].committed = true;
  }
  for (UnitIndex unit : units) {
    if ((state.units[unit].kind == UnitKind::Squadron) == flight) {
      SetGroup(state, unit, target);
    }
  }
}

void LaunchUnits(BattleState& state, const std::vector<UnitIndex>& units)
{
  for (UnitIndex unit : units) {
    if (SquadronState* squadron = state.Squadron(unit)) {
      squadron->stayDocked = false;
      squadron->launchOrdered = true;
      std::erase_if(state.units[unit].orders, [](const UnitOrder& order) { return order.kind == OrderKind::Recall; });
      if (GroupState* group = state.Group(state.units[unit].group)) {
        std::erase_if(group->orders, [](const UnitOrder& order) { return order.kind == OrderKind::Recall; });
      }
    } else if (state.Ship(unit) != nullptr) {
      for (SquadronState& squadron : state.squadrons) {
        if (squadron.carrier == unit && squadron.place == SquadronPlace::Docked) {
          squadron.stayDocked = false;
          squadron.launchOrdered = true;
        }
      }
    }
  }
}

} // namespace

bool ValidateCommand(const BattleState& state, const Command& command)
{
  if (state.finished || !TargetsValid(state, command)) {
    return false;
  }
  const Side side = command.side;
  const std::vector<UnitIndex> units = SelectedUnits(state, command.target);
  const auto has = [&](auto&& predicate) { return AnyUnit(state, units, predicate); };
  const auto canRetreat = state.snapshot.params.sides[SideIndex(side)].canRetreat;
  return std::visit(
      Overloaded{
          [&](const OrderAttack& order) {
            return has(IsCombatant) && !order.targets.empty()
                   && std::all_of(order.targets.begin(), order.targets.end(),
                                  [&](UnitIndex unit) { return IsEnemyCombatant(state, side, unit); });
          },
          [&](const OrderMove& order) {
            return has(IsCombatant) && std::isfinite(order.position.x) && std::isfinite(order.position.y)
                   && (!order.facing || std::isfinite(*order.facing));
          },
          [&](const OrderApproach& order) { return has(IsCombatant) && IsEnemyCombatant(state, side, order.target); },
          [&](const OrderHold&) { return has(IsCombatant); },
          [&](const OrderEscort& order) { return has(IsCombatant) && IsFriendly(state, side, order.target); },
          [&](const OrderTractor& order) {
            return has([&](const UnitState&, UnitIndex unit) { return state.Profile(unit).tractorPower > 0.0f; })
                   && (order.target == kNoUnit || IsEnemyShip(state, side, order.target));
          },
          [&](const OrderLaunch&) {
            return has([&](const UnitState& unit, UnitIndex index) {
              return unit.kind == UnitKind::Squadron || state.Profile(index).hangarCapacity > 0;
            });
          },
          [&](const OrderRecall&) { return has(IsSquadron); },
          [&](const OrderStrike& order) { return has(IsSquadron) && IsEnemyShip(state, side, order.target); },
          [&](const OrderRecover&) { return has(IsSquadron); },
          [&](const OrderWithdraw&) {
            return canRetreat
                   && has([](const UnitState& unit, UnitIndex) { return unit.kind != UnitKind::Passive; });
          },
          [&](const OrderSetPower&) { return has(IsShip); },
          [&](const OrderSetShipDoctrine&) { return has(IsShip); },
          [&](const OrderSetFlightDoctrine&) { return has(IsSquadron); },
          [&](const OrderAssignGroup& order) {
            if (units.empty()) {
              return false;
            }
            const bool flight = state.units[units.front()].kind == UnitKind::Squadron;
            const GroupState* group = state.Group(order.group);
            return order.group == kNoGroup || (group != nullptr && group->side == side && group->flight == flight);
          },
          [&](const OrderSignal& order) {
            const SideState& self = state.sides[SideIndex(side)];
            return self.signalsReady > 0 && self.commandRating > 0.0f
                   && (order.signal != SignalKind::ConcentrateFire || IsEnemyCombatant(state, side, order.target));
          },
          [&](const OrderFireSuperlaser& order) {
            const UnitIndex station = SideStation(state, side);
            return station != kNoUnit && SuperlaserReady(state, station) && IsEnemyShip(state, side, order.target)
                   && state.InSpace(order.target);
          },
          [&](const OrderDeathStarRun&) {
            const UnitIndex station = EnemyStation(state, side);
            return has(IsSquadron) && station != kNoUnit
                   && state.stations[state.units[station].slot].runFlight == kNoGroup;
          },
          [&](const OrderDeploy& order) {
            return state.tick == 0 && std::isfinite(order.position.x) && std::isfinite(order.position.y)
                   && (!order.facing || std::isfinite(*order.facing))
                   && has([&](const UnitState& unit, UnitIndex) { return unit.inSpace; });
          },
      },
      command.order);
}

void ApplyCommand(BattleState& state, const Command& command)
{
  const std::vector<UnitIndex> units = SelectedUnits(state, command.target);
  std::visit(
      Overloaded{
          [&](const OrderAttack& order) {
            UnitOrder queued = MakeOrder(OrderKind::Attack);
            queued.targets = order.targets;
            QueueOrder(state, command, queued, IsCombatant);
          },
          [&](const OrderMove& order) {
            UnitOrder queued = MakeOrder(OrderKind::Move);
            queued.position = Clamped(order.position, kBattleHalfExtent);
            queued.hasFacing = order.facing.has_value();
            queued.facing = order.facing.value_or(0.0f);
            Command groupsOnly = command;
            groupsOnly.target.units.clear();
            QueueOrder(state, groupsOnly, queued, IsCombatant);
            MoveSelection(state, command, queued);
          },
          [&](const OrderApproach& order) {
            UnitOrder queued = MakeOrder(OrderKind::Approach);
            queued.targets = {order.target};
            queued.approach = order.approach;
            QueueOrder(state, command, queued, IsCombatant);
          },
          [&](const OrderHold&) { QueueOrder(state, command, MakeOrder(OrderKind::Hold), IsCombatant); },
          [&](const OrderEscort& order) {
            UnitOrder queued = MakeOrder(OrderKind::Escort);
            queued.targets = {order.target};
            QueueOrder(state, command, queued, IsCombatant);
          },
          [&](const OrderTractor& order) {
            const auto hasTractor = [&](const UnitState&, UnitIndex unit) {
              return state.Profile(unit).tractorPower > 0.0f;
            };
            if (order.target == kNoUnit) {
              for (UnitIndex unit : units) {
                if (ShipState* ship = state.Ship(unit)) {
                  ship->tractorTarget = kNoUnit;
                  std::erase_if(state.units[unit].orders,
                                [](const UnitOrder& queued) { return queued.kind == OrderKind::Tractor; });
                }
              }
              return;
            }
            UnitOrder queued = MakeOrder(OrderKind::Tractor);
            queued.targets = {order.target};
            CommandTarget direct;
            direct.units = units;
            Command perUnit = command;
            perUnit.target = direct;
            QueueOrder(state, perUnit, queued, hasTractor);
          },
          [&](const OrderLaunch&) { LaunchUnits(state, units); },
          [&](const OrderRecall&) { QueueOrder(state, command, MakeOrder(OrderKind::Recall), IsSquadron); },
          [&](const OrderStrike& order) {
            UnitOrder queued = MakeOrder(OrderKind::Strike);
            queued.targets = {order.target};
            QueueOrder(state, command, queued, IsSquadron);
          },
          [&](const OrderRecover&) { QueueOrder(state, command, MakeOrder(OrderKind::Recover), IsSquadron); },
          [&](const OrderWithdraw&) {
            QueueOrder(state, command, MakeOrder(OrderKind::Withdraw), IsCombatant);
            if (!command.queue) {
              for (UnitIndex unit : units) {
                BeginWithdrawal(state, unit);
              }
            }
          },
          [&](const OrderSetPower& order) {
            ShipDoctrine doctrine;
            doctrine.power = order.power;
            for (GroupId id : command.target.groups) {
              ApplyShipFields(state.groups[id].shipDoctrine, doctrine, kShipPower);
            }
            for (UnitIndex unit : command.target.units) {
              ApplyShipFields(state.units[unit].shipDoctrine, doctrine, kShipPower);
              state.units[unit].overrides |= kShipPower;
            }
          },
          [&](const OrderSetShipDoctrine& order) {
            for (GroupId id : command.target.groups) {
              if (!state.groups[id].flight) {
                ApplyShipFields(state.groups[id].shipDoctrine, order.doctrine, order.fields);
                state.groups[id].committed = true;
              }
            }
            for (UnitIndex unit : command.target.units) {
              if (state.units[unit].kind == UnitKind::CapitalShip) {
                ApplyShipFields(state.units[unit].shipDoctrine, order.doctrine, order.fields);
                state.units[unit].overrides = (state.units[unit].overrides | (order.fields & kAllShipFields)) & ~order.clear;
              }
            }
          },
          [&](const OrderSetFlightDoctrine& order) {
            for (GroupId id : command.target.groups) {
              if (state.groups[id].flight) {
                ApplyFlightFields(state.groups[id].flightDoctrine, order.doctrine, order.fields);
              }
            }
            for (UnitIndex unit : command.target.units) {
              if (state.units[unit].kind == UnitKind::Squadron) {
                ApplyFlightFields(state.units[unit].flightDoctrine, order.doctrine, order.fields);
                state.units[unit].overrides
                    = (state.units[unit].overrides | (order.fields & kAllFlightFields)) & ~order.clear;
              }
            }
          },
          [&](const OrderAssignGroup& order) { AssignGroup(state, command, order.group); },
          [&](const OrderSignal& order) { ApplySignal(state, command.side, order); },
          [&](const OrderFireSuperlaser& order) {
            const UnitIndex unit = SideStation(state, command.side);
            StationState& station = state.stations[state.units[unit].slot];
            station.firingAt = order.target;
            station.fireDelayTicks = Ticks(kSuperlaserDelaySeconds);
            station.chargeTicks = Ticks(kSuperlaserRechargeSeconds);
            LogEvent(state, command.side, "Death Star: superlaser firing at " + UnitName(state, order.target),
                     order.target);
            AddEffect(state, ViewEffectKind::Superlaser, state.units[order.target].position,
                      state.Profile(order.target).radius, order.target);
          },
          [&](const OrderDeathStarRun&) {
            const UnitIndex unit = EnemyStation(state, command.side);
            GroupId flight = kNoGroup;
            for (GroupId id : command.target.groups) {
              flight = state.groups[id].flight ? id : flight;
            }
            if (flight == kNoGroup) {
              for (UnitIndex member : units) {
                if (state.units[member].kind == UnitKind::Squadron) {
                  flight = state.units[member].group;
                  break;
                }
              }
            }
            if (flight == kNoGroup) {
              return;
            }
            state.stations[state.units[unit].slot].runFlight = flight;
            GroupState& group = state.groups[flight];
            group.orders.clear();
            for (UnitIndex member : group.members) {
              state.units[member].orders.clear();
            }
            UnitOrder queued = MakeOrder(OrderKind::Run);
            queued.targets = {unit};
            group.orders.push_back(queued);
            LogEvent(state, command.side, group.name + ": attacking the Death Star");
          },
          [&](const OrderDeploy& order) { Deploy(state, command.side, units, order); },
      },
      command.order);
  // Orders are immediate: the groups and ships involved reconsider on this tick.
  for (UnitIndex unit : units) {
    if (ShipState* ship = state.Ship(unit)) {
      ship->nextThinkTick = static_cast<int32_t>(state.tick);
    }
    if (GroupState* group = state.Group(state.units[unit].group)) {
      group->nextThinkTick = static_cast<int32_t>(state.tick);
    }
    if (SquadronState* squadron = state.Squadron(unit)) {
      squadron->nextThinkTick = static_cast<int32_t>(state.tick);
    }
  }
}

namespace
{
bool OrderDone(const BattleState& state, UnitOrder& order, bool last, Vec2 position)
{
  switch (order.kind) {
    case OrderKind::Attack:
      std::erase_if(order.targets, [&](UnitIndex target) { return !state.InSpace(target); });
      return order.targets.empty();
    case OrderKind::Approach:
    case OrderKind::Escort:
    case OrderKind::Tractor:
    case OrderKind::Strike:
    case OrderKind::Run:
      return order.targets.empty() || !state.InSpace(order.targets.front());
    case OrderKind::Move:
      return !last && Distance(position, order.position) < kMoveArrivalDistance * 2.0f;
    default:
      return false;
  }
}

void PopQueue(BattleState& state, std::vector<UnitOrder>& orders, Vec2 position)
{
  while (!orders.empty() && OrderDone(state, orders.front(), orders.size() == 1, position)) {
    orders.erase(orders.begin());
  }
}
} // namespace

void PopFinishedOrders(BattleState& state)
{
  for (UnitIndex i = 0; i < static_cast<UnitIndex>(state.units.size()); ++i) {
    UnitState& unit = state.units[i];
    if (unit.orders.empty()) {
      continue;
    }
    PopQueue(state, unit.orders, unit.position);
    if (!unit.orders.empty() && unit.orders.front().kind == OrderKind::Withdraw && unit.inSpace) {
      BeginWithdrawal(state, i);
    }
  }
  for (GroupState& group : state.groups) {
    if (group.orders.empty()) {
      continue;
    }
    PopQueue(state, group.orders, Centroid(state, group.members));
    if (!group.orders.empty() && group.orders.front().kind == OrderKind::Withdraw) {
      for (UnitIndex member : group.members) {
        if (state.units[member].orders.empty() && state.units[member].inSpace) {
          BeginWithdrawal(state, member);
        }
      }
    }
  }
}

} // namespace reb::battle::sim
