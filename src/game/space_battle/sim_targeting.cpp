#include <algorithm>
#include <cmath>

#include "space_battle/math2d.h"
#include "space_battle/sim_internal.h"

namespace reb::battle::sim
{
namespace
{
using namespace tuning;

// The ships and squadrons this group is protecting: what makes an enemy "threatening".
std::vector<UnitIndex> ProtectedUnits(const BattleState& state, const GroupState& group)
{
  std::vector<UnitIndex> protectedUnits;
  if (!group.flight && group.shipDoctrine.screenTarget != kNoUnit) {
    protectedUnits.push_back(group.shipDoctrine.screenTarget);
  }
  if (group.flight && group.flightDoctrine.escortTarget != kNoUnit) {
    protectedUnits.push_back(group.flightDoctrine.escortTarget);
  }
  if (!group.orders.empty() && group.orders.front().kind == OrderKind::Escort && !group.orders.front().targets.empty()) {
    protectedUnits.push_back(group.orders.front().targets.front());
  }
  return protectedUnits;
}

UnitIndex CurrentTargetOf(const BattleState& state, UnitIndex unit)
{
  if (const ShipState* ship = state.Ship(unit)) {
    return ship->primaryTarget;
  }
  if (const SquadronState* squadron = state.Squadron(unit)) {
    return squadron->target;
  }
  return kNoUnit;
}

float ReachRadius(const BattleState& state, UnitIndex target)
{
  if (const SquadronState* squadron = state.Squadron(target)) {
    return squadron->spread;
  }
  return state.Profile(target).radius;
}

float RangeFactor(const BattleState& state, UnitIndex ship, UnitIndex target)
{
  const UnitProfile& profile = state.Profile(ship);
  const bool fighter = state.units[target].kind == UnitKind::Squadron;
  float reach = 0.0f;
  for (int i = 0; i < profile.batteryCount; ++i) {
    const BatteryProfile& battery = profile.batteries[i];
    if ((fighter ? battery.dpsVsFighter : battery.dpsVsCapital) > 0.0f) {
      reach = std::max(reach, battery.range);
    }
  }
  const float distance = Distance(state.units[ship].position, state.units[target].position) - ReachRadius(state, target);
  return distance <= reach ? 1.0f : 1.0f / (1.0f + (distance - reach) / kRangeScoreFalloff);
}

float ArcFactor(const BattleState& state, UnitIndex ship, UnitIndex target)
{
  const UnitProfile& profile = state.Profile(ship);
  const bool fighter = state.units[target].kind == UnitKind::Squadron;
  const auto& arcs = fighter ? profile.arcVsFighter : profile.arcVsCapital;
  const auto best = static_cast<Arc>(std::max_element(arcs.begin(), arcs.end()) - arcs.begin());
  const UnitState& self = state.units[ship];
  const float bearing = AngleDelta(self.heading, AngleOf(state.units[target].position - self.position));
  const float turn = std::max(0.0f, std::fabs(AngleDelta(ArcCenter(best), bearing)) - kArcHalfWidth);
  const float rate = std::max(TurnRate(state, ship), 0.01f);
  return 1.0f / (1.0f + turn / rate / kTurnScoreSeconds);
}

int32_t NearestCraftInReach(const BattleState& state, Vec2 from, float heading, const BatteryProfile& battery,
                            const SquadronState& squadron)
{
  int32_t best = -1;
  float bestDistance = battery.range * battery.range;
  for (int i = 0; i < squadron.craftCount; ++i) {
    const int32_t index = squadron.firstCraft + i;
    const CraftState& craft = state.craft[index];
    if (!craft.alive) {
      continue;
    }
    const float distance = DistanceSq(from, craft.position);
    if (distance <= bestDistance && InArc(battery.arc, AngleDelta(heading, AngleOf(craft.position - from)))) {
      bestDistance = distance;
      best = index;
    }
  }
  return best;
}

} // namespace

void ScoreGroupTargets(BattleState& state, GroupState& group)
{
  group.candidates.clear();
  const std::vector<UnitIndex> protectedUnits = ProtectedUnits(state, group);
  const TargetPriorities& priorities = group.flight ? group.flightDoctrine.priorities : group.shipDoctrine.priorities;
  for (UnitIndex i = 0; i < static_cast<UnitIndex>(state.units.size()); ++i) {
    const UnitState& unit = state.units[i];
    if (unit.side == group.side || !unit.inSpace
        || (unit.kind != UnitKind::CapitalShip && unit.kind != UnitKind::Squadron)) {
      continue;
    }
    ScoredTarget target;
    target.unit = i;
    target.tags = state.Tags(i);
    if (std::find(protectedUnits.begin(), protectedUnits.end(), CurrentTargetOf(state, i)) != protectedUnits.end()) {
      target.tags |= TagBit(TargetTag::Threatening);
    }
    const float weight = PriorityWeight(priorities, target.tags, &target.rank, &target.tag);
    const float health = EffectiveHealth(state, i) / std::max(1.0f, MaxEffectiveHealth(state, i));
    target.stateFactor = 1.5f - 0.5f * std::clamp(health, 0.0f, 1.0f);
    if (priorities.finishDamaged) {
      float dps = 0.0f;
      for (UnitIndex member : group.members) {
        dps += ExpectedDps(state, member, i, DpsGeometry::BestArc);
      }
      const float seconds = TimeToKill(state, i, dps);
      if (seconds >= 0.0f && seconds < kFinishSeconds) {
        target.stateFactor *= kFinishBoost;
      }
    }
    target.score = weight * target.stateFactor;
    group.candidates.push_back(target);
  }
  std::stable_sort(group.candidates.begin(), group.candidates.end(),
                   [](const ScoredTarget& a, const ScoredTarget& b) { return a.score > b.score; });
}

float ShipTargetScore(const BattleState& state, UnitIndex ship, const ScoredTarget& target)
{
  if (!state.InSpace(target.unit)) {
    return 0.0f;
  }
  const ShipDoctrine doctrine = state.EffectiveShipDoctrine(ship);
  const float weight = PriorityWeight(doctrine.priorities, target.tags, nullptr, nullptr);
  const float potential = ExpectedDps(state, ship, target.unit, DpsGeometry::BestArc);
  return weight * target.stateFactor * potential * RangeFactor(state, ship, target.unit)
         * ArcFactor(state, ship, target.unit);
}

void AllocateFire(BattleState& state, GroupState& group)
{
  if (group.flight) {
    return;
  }
  std::vector<UnitIndex> members;
  for (UnitIndex member : group.members) {
    if (state.InSpace(member)) {
      members.push_back(member);
    }
  }
  std::stable_sort(members.begin(), members.end(), [&](UnitIndex a, UnitIndex b) {
    return state.Profile(a).firepower * WeaponOutput(state, a) > state.Profile(b).firepower * WeaponOutput(state, b);
  });
  std::vector<float> committed(state.units.size(), 0.0f);
  for (const ShipState& other : state.ships) {
    if (state.units[other.unit].side == group.side && state.units[other.unit].group != group.id
        && state.InSpace(other.primaryTarget) && state.InSpace(other.unit)) {
      committed[other.primaryTarget] += ExpectedDps(state, other.unit, other.primaryTarget, DpsGeometry::BestArc);
    }
  }
  const SideState& side = state.sides[SideIndex(group.side)];
  UnitIndex concentrateOn = kNoUnit;
  const bool anyConcentrate = std::any_of(members.begin(), members.end(), [&](UnitIndex member) {
    return state.EffectiveShipDoctrine(member).allocation == FireAllocation::Concentrate;
  });
  if (anyConcentrate) {
    float best = 0.0f;
    for (const ScoredTarget& target : group.candidates) {
      float total = 0.0f;
      for (UnitIndex member : members) {
        total += ShipTargetScore(state, member, target);
      }
      if (total > best) {
        best = total;
        concentrateOn = target.unit;
      }
    }
  }
  for (UnitIndex member : members) {
    ShipState& ship = *state.Ship(member);
    Intent& intent = state.units[member].intent;
    intent.fromOrder = false;
    const std::vector<UnitOrder>& orders = state.ActiveOrders(member);
    UnitIndex chosen = kNoUnit;
    const FireAllocation allocation = state.EffectiveShipDoctrine(member).allocation;
    if (!orders.empty() && orders.front().kind == OrderKind::Attack && !orders.front().targets.empty()) {
      chosen = orders.front().targets.front();
      intent.fromOrder = true;
    } else if (!orders.empty() && orders.front().kind == OrderKind::Approach && !orders.front().targets.empty()
               && state.units[orders.front().targets.front()].kind == UnitKind::CapitalShip) {
      chosen = orders.front().targets.front();
      intent.fromOrder = true;
    } else if (side.concentrateTicks > 0 && state.InSpace(side.concentrateTarget)
               && Distance(state.units[member].position, state.units[side.concentrateTarget].position)
                      <= kConcentrateRange) {
      chosen = side.concentrateTarget;
      intent.fromOrder = true;
    } else if (allocation == FireAllocation::Concentrate) {
      chosen = concentrateOn;
    } else {
      float bestScore = 0.0f;
      float bestOpenScore = 0.0f;
      UnitIndex best = kNoUnit;
      UnitIndex bestOpen = kNoUnit;
      for (const ScoredTarget& target : group.candidates) {
        const float score = ShipTargetScore(state, member, target);
        if (score <= 0.0f) {
          continue;
        }
        if (score > bestScore) {
          bestScore = score;
          best = target.unit;
        }
        // Auto: skip targets that already have enough fire committed to die within the window.
        const float net = committed[target.unit] - Regeneration(state, target.unit);
        const bool open = net * kOverkillWindowSeconds < EffectiveHealth(state, target.unit);
        if (allocation == FireAllocation::Auto && open && score > bestOpenScore) {
          bestOpenScore = score;
          bestOpen = target.unit;
        }
      }
      chosen = allocation == FireAllocation::Auto && bestOpen != kNoUnit ? bestOpen : best;
    }
    ship.primaryTarget = state.InSpace(chosen) ? chosen : kNoUnit;
    if (ship.primaryTarget != kNoUnit) {
      committed[ship.primaryTarget] += ExpectedDps(state, member, ship.primaryTarget, DpsGeometry::BestArc);
    }
  }
  // Intent details: rank, others assigned, time to destroy at the committed rate.
  std::vector<int16_t> assigned(state.units.size(), 0);
  for (const ShipState& other : state.ships) {
    if (state.units[other.unit].side == group.side && state.InSpace(other.unit) && state.InSpace(other.primaryTarget)) {
      ++assigned[other.primaryTarget];
    }
  }
  int focusCount = 0;
  group.focus = kNoUnit;
  for (UnitIndex member : members) {
    const ShipState& ship = *state.Ship(member);
    Intent& intent = state.units[member].intent;
    intent.target = ship.primaryTarget;
    if (ship.primaryTarget == kNoUnit) {
      continue;
    }
    const ShipDoctrine doctrine = state.EffectiveShipDoctrine(member);
    PriorityWeight(doctrine.priorities, state.Tags(ship.primaryTarget), &intent.priorityRank, &intent.tag);
    intent.othersAssigned = static_cast<int16_t>(assigned[ship.primaryTarget] - 1);
    intent.timeToKill = TimeToKill(state, ship.primaryTarget, committed[ship.primaryTarget]);
    const int count = static_cast<int>(std::count_if(members.begin(), members.end(), [&](UnitIndex other) {
      return state.Ship(other)->primaryTarget == ship.primaryTarget;
    }));
    if (count > focusCount) {
      focusCount = count;
      group.focus = ship.primaryTarget;
    }
  }
}

UnitIndex SelectBatteryTarget(const BattleState& state, UnitIndex shipIndex, int batteryIndex, int32_t* craftOut)
{
  const ShipState& ship = *state.Ship(shipIndex);
  const UnitState& self = state.units[shipIndex];
  const BatteryProfile& battery = state.Profile(shipIndex).batteries[batteryIndex];
  const GroupState* group = state.Group(self.group);
  const ShipDoctrine doctrine = state.EffectiveShipDoctrine(shipIndex);
  const SideState& side = state.sides[SideIndex(self.side)];
  const float shooterTurn = TurnPoints(state, shipIndex);
  *craftOut = -1;

  const auto reachable = [&](UnitIndex target, int32_t* craft) {
    const UnitState& other = state.units[target];
    if (!other.inSpace) {
      return false;
    }
    if (other.kind == UnitKind::Squadron) {
      if (battery.dpsVsFighter <= 0.0f) {
        return false;
      }
      const SquadronState& squadron = *state.Squadron(target);
      if (Distance(self.position, squadron.centroid) - squadron.spread > battery.range) {
        return false;
      }
      *craft = NearestCraftInReach(state, self.position, self.heading, battery, squadron);
      return *craft >= 0;
    }
    if (other.kind != UnitKind::CapitalShip || battery.dpsVsCapital <= 0.0f) {
      return false;
    }
    const float distance = Distance(self.position, other.position) - state.Profile(target).radius;
    return distance <= battery.range && InArc(battery.arc, AngleDelta(self.heading, AngleOf(other.position - self.position)));
  };

  if (doctrine.allBatteriesOnTarget && state.InSpace(ship.primaryTarget)
      && state.ShieldFraction(ship.primaryTarget) <= 0.0f) {
    int32_t craft = -1;
    if (reachable(ship.primaryTarget, &craft)) {
      *craftOut = craft;
      return ship.primaryTarget;
    }
  }
  if (group == nullptr) {
    return kNoUnit;
  }
  UnitIndex best = kNoUnit;
  float bestValue = 0.0f;
  for (const ScoredTarget& target : group->candidates) {
    int32_t craft = -1;
    if (!reachable(target.unit, &craft)) {
      continue;
    }
    float suitability = 0.0f;
    if (state.units[target.unit].kind == UnitKind::Squadron) {
      suitability = battery.dpsVsFighter * Evasion(shooterTurn, TurnPoints(state, target.unit));
    } else {
      const float ion = battery.kind == WeaponKind::Ion && state.ShieldFraction(target.unit) <= 0.0f ? kIonValueShieldsDown : 1.0f;
      suitability = battery.dpsVsCapital * ion;
    }
    float value = PriorityWeight(doctrine.priorities, target.tags, nullptr, nullptr) * target.stateFactor * suitability;
    if (target.unit == ship.primaryTarget) {
      value *= kPrimaryTargetBonus;
    }
    if (side.concentrateTicks > 0 && target.unit == side.concentrateTarget) {
      value *= kPrimaryTargetBonus;
    }
    if (value > bestValue) {
      bestValue = value;
      best = target.unit;
      *craftOut = craft;
    }
  }
  return best;
}

} // namespace reb::battle::sim
