#include <algorithm>
#include <cmath>
#include <cstdio>

#include "space_battle/battle.h"
#include "space_battle/battle_queries.h"
#include "space_battle/math2d.h"
#include "space_battle/sim_internal.h"

namespace reb::battle
{
namespace
{
using namespace tuning;

constexpr std::array<const char*, kArcCount> kArcNames{"fore", "aft", "port", "starboard"};

const char* RangeName(EngagementRange range)
{
  switch (range) {
    case EngagementRange::Close:
      return "Close";
    case EngagementRange::Optimal:
      return "Optimal";
    case EngagementRange::StandOff:
      return "Stand-off";
  }
  return "";
}

const char* BlockerText(JumpBlocker blocker)
{
  switch (blocker) {
    case JumpBlocker::NoHyperdrive:
      return "hyperdrive knocked out";
    case JumpBlocker::Engines:
      return "engines down";
    case JumpBlocker::Tractored:
      return "held by tractor beams";
    case JumpBlocker::GravityWell:
      return "inside a gravity well";
    case JumpBlocker::SideCannotRetreat:
      return "no retreat route";
    case JumpBlocker::None:
      break;
  }
  return "";
}

std::string Kilometres(float meters)
{
  char text[32];
  std::snprintf(text, sizeof(text), "%.1f km", static_cast<double>(meters) / 1000.0);
  return text;
}

std::string TargetReason(const Intent& intent)
{
  if (intent.fromOrder) {
    return "ordered target";
  }
  if (intent.priorityRank < 0) {
    return "best available target";
  }
  return "priority " + std::to_string(intent.priorityRank + 1) + ", " + TagName(intent.tag);
}

std::string CarrierName(const BattleState& state, UnitIndex carrier)
{
  return carrier == kNoUnit ? std::string("the planet") : sim::UnitName(state, carrier);
}

std::string IntentText(const BattleState& state, UnitIndex unit)
{
  const Intent& intent = state.units[unit].intent;
  const auto targetName = [&] { return state.Valid(intent.target) ? sim::UnitName(state, intent.target) : std::string(); };
  switch (state.units[unit].fate) {
    case UnitFate::Destroyed:
      return "Destroyed";
    case UnitFate::Withdrawn:
      return "Withdrawn";
    default:
      break;
  }
  const SquadronState* squadron = state.Squadron(unit);
  switch (intent.kind) {
    case IntentKind::Engaging: {
      std::string text = "Targeting " + targetName() + ": " + TargetReason(intent);
      if (intent.arc >= 0) {
        text += "; in " + std::string(kArcNames[static_cast<size_t>(intent.arc)]) + " arc";
      }
      if (intent.timeToKill > 0.0f) {
        text += "; about " + std::to_string(static_cast<int>(std::lround(intent.timeToKill))) + " s to destroy";
      }
      if (intent.othersAssigned > 0) {
        text += "; " + std::to_string(intent.othersAssigned)
                + (intent.othersAssigned == 1 ? " other ship assigned" : " other ships assigned");
      }
      return text;
    }
    case IntentKind::Closing:
      return "Closing on " + targetName() + ": " + TargetReason(intent);
    case IntentKind::NoTarget: {
      std::string text = "No target in range";
      if (state.units[unit].kind == UnitKind::CapitalShip && intent.range > 0.0f) {
        text += ": holding " + std::string(RangeName(state.EffectiveShipDoctrine(unit).range)) + " at "
                + Kilometres(intent.range);
      }
      return text;
    }
    case IntentKind::Holding:
      return "Holding position";
    case IntentKind::Moving:
      return "Moving to ordered position";
    case IntentKind::Screening:
      return "Screening " + targetName();
    case IntentKind::Escorting:
      return "Escorting " + targetName();
    case IntentKind::Withdrawing:
      return "Withdrawing: hyperdrive " + std::to_string(static_cast<int>(intent.progress * 100.0f)) + "% spooled";
    case IntentKind::Trapped:
      return "Withdrawing, cannot jump: " + std::string(BlockerText(intent.blocker));
    case IntentKind::Docked:
      return "Docked aboard " + CarrierName(state, squadron != nullptr ? squadron->carrier : kNoUnit);
    case IntentKind::Rearming:
      return "Rearming aboard " + CarrierName(state, squadron != nullptr ? squadron->carrier : kNoUnit);
    case IntentKind::Returning:
      return "Returning to " + CarrierName(state, squadron != nullptr ? squadron->carrier : kNoUnit);
    case IntentKind::Intercepting:
      return "Intercepting " + targetName() + ": " + TargetReason(intent);
    case IntentKind::Striking:
      return "Attack run on " + targetName() + ": " + TargetReason(intent);
    case IntentKind::Patrolling:
      return "Patrolling";
    case IntentKind::Tractoring:
      return "Holding " + targetName() + " in a tractor beam";
    case IntentKind::TrenchRun:
      return "Trench run on " + targetName();
    case IntentKind::Disrupted:
      return "Disabled by ion fire";
    case IntentKind::HoldingCover:
      return "Under the planetary guns: " + targetName() + " is outside their cover";
    case IntentKind::Destroyed:
      return "Destroyed";
    case IntentKind::Withdrawn:
      return "Withdrawn";
    case IntentKind::None:
      break;
  }
  return "";
}

ViewShotKind ToViewShot(ShotKind kind)
{
  switch (kind) {
    case ShotKind::Turbolaser:
      return ViewShotKind::Turbolaser;
    case ShotKind::Ion:
      return ViewShotKind::Ion;
    case ShotKind::Laser:
      return ViewShotKind::Laser;
    case ShotKind::Torpedo:
      return ViewShotKind::Torpedo;
  }
  return ViewShotKind::Turbolaser;
}

uint32_t UnitFlags(const BattleState& state, UnitIndex index)
{
  const UnitState& unit = state.units[index];
  uint32_t flags = 0;
  if (unit.fate == UnitFate::Destroyed) {
    flags |= kViewDestroyed;
  }
  if (unit.fate == UnitFate::Withdrawn) {
    flags |= kViewWithdrawn;
  }
  if (state.Withdrawing(index) && unit.inSpace) {
    flags |= kViewWithdrawing;
  }
  if (const SquadronState* squadron = state.Squadron(index); squadron && squadron->place == SquadronPlace::Docked) {
    flags |= kViewDocked;
  }
  if (const ShipState* ship = state.Ship(index)) {
    if (ship->tractorHeld > 0.0f) {
      flags |= kViewTractored;
    }
    if (ship->tractorTarget != kNoUnit) {
      flags |= kViewTractoring;
    }
    if (ship->engineOffTicks > 0 || ship->systemsOffTicks > 0) {
      flags |= kViewDisrupted;
    }
  }
  if (unit.inSpace && unit.kind != UnitKind::DeathStar && InEnemyWell(state, index)) {
    flags |= kViewInEnemyWell;
  }
  if ((state.Profile(index).staticTags & TagBit(TargetTag::Flagship)) != 0) {
    flags |= kViewFlagship;
  }
  if (unit.kind != UnitKind::DeathStar && state.ShieldFraction(index) <= 0.0f) {
    flags |= kViewShieldsDown;
  }
  if (PlanetaryTarget(state, Opponent(unit.side), index)) {
    flags |= kViewUnderPlanetaryGuns;
  }
  return flags;
}

float SpoolProgress(const BattleState& state, UnitIndex index)
{
  int32_t progress = 0;
  int32_t total = 0;
  if (const ShipState* ship = state.Ship(index)) {
    progress = ship->spoolTicks;
    total = ship->spoolTotal;
  } else if (const SquadronState* squadron = state.Squadron(index)) {
    progress = squadron->spoolTicks;
    total = squadron->spoolTotal;
  } else if (const StationState* station = state.Station(index)) {
    progress = station->spoolTicks;
    total = station->spoolTotal;
  }
  return total > 0 ? static_cast<float>(progress) / static_cast<float>(total) : 0.0f;
}

ViewUnitKind ViewKind(UnitKind kind)
{
  switch (kind) {
    case UnitKind::Squadron:
      return ViewUnitKind::Squadron;
    case UnitKind::DeathStar:
      return ViewUnitKind::DeathStar;
    default:
      return ViewUnitKind::CapitalShip;
  }
}

const UnitIdentity& IdentityOf(const UnitEntry& entry)
{
  return std::visit([](const auto& unit) -> const UnitIdentity& { return unit.identity; }, entry);
}

} // namespace

// A bolt at a fighter is drawn to where that craft is now rather than where it was when the shot was fired (a fighter
// covers hundreds of metres during a bolt's flight), so a hit lands on the craft and a miss passes beside it. The
// simulation decided the hit when the shot was fired; this only moves the drawing.
Vec2 DrawnShotEnd(const BattleState& state, const PendingShot& shot, float alpha)
{
  if (shot.targetCraft < 0 || !state.craft[shot.targetCraft].alive) {
    return shot.to;
  }
  const CraftState& victim = state.craft[shot.targetCraft];
  const Vec2 at = Lerp(victim.previousPosition, victim.position, alpha);
  return shot.hit ? at : at + Rotated(Normalized(at - shot.from), 0.5f * kPi) * kCraftMissOffset;
}

void Battle::BuildViewModel(float alpha, ViewModel& out) const
{
  const BattleState& state = *m_state;
  alpha = std::clamp(alpha, 0.0f, 1.0f);
  // Continuous battle time: the latest step moved the world from tick - 1 to tick.
  const float now = std::max(0.0f, static_cast<float>(state.tick - 1) + alpha);
  out.tick = state.tick;
  out.battleSeconds = now / kTicksPerSecond;
  out.boundsMin = {-kBattleHalfExtent, -kBattleHalfExtent};
  out.boundsMax = {kBattleHalfExtent, kBattleHalfExtent};
  out.planetCenter = state.planetCenter;
  out.planetRadius = kPlanetRadius;
  out.decided = state.endReason != EndReason::None;
  out.endReason = static_cast<ViewEndReason>(state.endReason);
  out.finished = state.finished;

  // The target each unit is assigned to, and whether it shot at it within the last second.
  std::vector<UnitIndex> targets(state.units.size(), kNoUnit);
  for (const ShipState& ship : state.ships) {
    targets[ship.unit] = ship.primaryTarget;
  }
  for (const SquadronState& squadron : state.squadrons) {
    targets[squadron.unit] = squadron.target;
  }
  for (const StationState& station : state.stations) {
    targets[station.unit] = station.firingAt;
  }
  std::vector<bool> firing(state.units.size(), false);
  for (const PendingShot& shot : state.shots) {
    if (state.Valid(shot.shooter) && shot.target == targets[shot.shooter] && shot.fireTick + kTicksPerSecond >= state.tick) {
      firing[shot.shooter] = true;
    }
  }

  out.units.clear();
  for (UnitIndex i = 0; i < static_cast<UnitIndex>(state.units.size()); ++i) {
    const UnitState& unit = state.units[i];
    if (unit.kind == UnitKind::Passive) {
      continue;
    }
    const UnitProfile& profile = state.Profile(i);
    ViewUnit view;
    view.unit = i;
    view.kind = ViewKind(unit.kind);
    view.side = unit.side;
    view.typeIndex = IdentityOf(state.snapshot.units[i]).typeIndex;
    view.group = unit.group;
    view.position = Lerp(unit.previousPosition, unit.position, alpha);
    view.heading = LerpAngle(unit.previousHeading, unit.heading, alpha);
    view.radius = profile.radius;
    if (const SquadronState* squadron = state.Squadron(i)) {
      view.radius = std::max(50.0f, squadron->spread);
      view.craft = squadron->alive;
      view.craftMax = squadron->craftCount;
    }
    view.hullFraction = state.HullFraction(i);
    view.shieldFraction = state.ShieldFraction(i);
    view.spool = SpoolProgress(state, i);
    view.flags = UnitFlags(state, i);
    if (const ShipState* ship = state.Ship(i)) {
      view.criticals = ship->criticals;
    }
    view.intent = IntentText(state, i);
    view.target = unit.inSpace && state.InSpace(targets[i]) ? targets[i] : kNoUnit;
    view.firing = view.target != kNoUnit && firing[i];
    view.velocity = unit.kind == UnitKind::Squadron && unit.inSpace
                        ? (unit.position - unit.previousPosition) * static_cast<float>(kTicksPerSecond)
                        : unit.velocity;
    view.hullMax = profile.hullMax;
    view.jumpBlocked = unit.inSpace && !CanJump(state, i);
    if (const auto* station = std::get_if<DeathStarEntry>(&state.snapshot.units[i])) {
      view.deathStarShielded = station->shieldFacility;
    }
    if (const SquadronState* squadron = state.Squadron(i)) {
      view.deathStarRun = squadron->task == SquadronTask::TrenchRun;
      view.inTrench = squadron->runTicks > 0;
    }
    out.units.push_back(std::move(view));
  }

  out.craft.clear();
  for (const SquadronState& squadron : state.squadrons) {
    if (squadron.place != SquadronPlace::InSpace) {
      continue;
    }
    for (int i = 0; i < squadron.craftCount; ++i) {
      const CraftState& craft = state.craft[squadron.firstCraft + i];
      if (craft.alive) {
        out.craft.push_back({squadron.unit, Lerp(craft.previousPosition, craft.position, alpha),
                             LerpAngle(craft.previousHeading, craft.heading, alpha)});
      }
    }
  }

  out.shots.clear();
  for (const PendingShot& shot : state.shots) {
    const auto flight = static_cast<float>(std::max<int64_t>(1, shot.impactTick - shot.fireTick));
    const float progress = (now - static_cast<float>(shot.fireTick)) / flight;
    if (progress < 0.0f) {
      continue;
    }
    out.shots.push_back(
        {ToViewShot(shot.kind), shot.side, shot.from, DrawnShotEnd(state, shot, alpha), std::min(progress, 1.0f),
         shot.hit, shot.shieldImpact,
         shot.fireTick, shot.impactTick, shot.shooter, shot.target, shot.targetCraft, shot.battery});
  }

  out.coverRadius = state.defenses.empty() ? 0.0f : PlanetaryCoverRadius();
  out.defenses.clear();
  for (size_t i = 0; i < state.defenses.size(); ++i) {
    const DefenseProfile& profile = state.defenseProfiles[i];
    const DefenseState& defense = state.defenses[i];
    const DefenseEntry& entry = state.snapshot.defenses[i];
    ViewDefense view;
    view.side = profile.side;
    view.weapon = profile.weapon;
    view.name = entry.identity.className.empty() ? entry.identity.name : entry.identity.className;
    view.site = state.planetCenter + FromAngle(profile.siteAngle) * kPlanetRadius;
    view.dps = profile.dps;
    view.target = state.InSpace(defense.target) ? defense.target : kNoUnit;
    view.sinceFired = defense.lastFireTick >= 0 ? (now - static_cast<float>(defense.lastFireTick)) / kTicksPerSecond : -1.0f;
    view.briefingKnown = entry.identity.briefingKnown;
    out.defenses.push_back(std::move(view));
  }

  out.effects.clear();
  for (const Effect& effect : state.effects) {
    const float age = (now - static_cast<float>(effect.tick)) / kTicksPerSecond;
    if (age >= 0.0f) {
      out.effects.push_back({effect.kind, effect.position, effect.size, age, effect.unit, effect.tick});
    }
  }

  out.tractors.clear();
  out.wells.clear();
  for (const ShipState& ship : state.ships) {
    if (!state.InSpace(ship.unit)) {
      continue;
    }
    if (ship.tractorTarget != kNoUnit) {
      out.tractors.push_back({ship.unit, ship.tractorTarget});
    }
    if (state.Profile(ship.unit).gravityWell) {
      const UnitState& unit = state.units[ship.unit];
      out.wells.push_back({ship.unit, Lerp(unit.previousPosition, unit.position, alpha), kGravityWellRadius, unit.side,
                           WellActive(state, ship.unit)});
    }
  }

  out.groups.clear();
  for (const GroupState& group : state.groups) {
    out.groups.push_back(
        {group.id, group.side, group.flight, group.name, group.members, group.shipDoctrine, group.flightDoctrine});
  }

  for (Side side : {Side::Alliance, Side::Empire}) {
    const SideState& self = state.sides[SideIndex(side)];
    ViewSide& view = out.sides[SideIndex(side)];
    view.strength = self.strength;
    view.initialStrength = self.initialStrength;
    view.signalsReady = self.signalsReady;
    view.signalsMax = self.signalsMax;
    view.withdrawing = std::any_of(state.ships.begin(), state.ships.end(), [&](const ShipState& ship) {
      return ship.withdrawing && state.units[ship.unit].side == side && state.InSpace(ship.unit);
    });
  }

  out.superlaserCharge = -1.0f;
  for (const StationState& station : state.stations) {
    const auto* entry = std::get_if<DeathStarEntry>(&state.snapshot.units[station.unit]);
    if (state.InSpace(station.unit) && entry != nullptr && entry->completed) {
      out.superlaserCharge
          = 1.0f - static_cast<float>(station.chargeTicks) / static_cast<float>(sim::Ticks(kSuperlaserRechargeSeconds));
      break;
    }
  }

  out.deathStarRunOpen = std::any_of(state.stations.begin(), state.stations.end(), [&](const StationState& station) {
    return station.runFlight == kNoGroup && TrenchRunPossible(state, station.unit);
  });
  for (Side side : {Side::Alliance, Side::Empire}) {
    const bool briefing = state.tick == 0;
    const float radius = briefing ? state.sides[SideIndex(side)].deploymentRadius : 0.0f;
    out.deployZones[SideIndex(side)] = {DeploymentCenter(state, side), radius};
  }

  out.recentEvents.clear();
  for (const BattleEvent& event : state.events) {
    out.recentEvents.push_back({static_cast<float>(event.tick) / kTicksPerSecond, event.side, event.text, event.unit});
  }
}

ViewUnitDetail Battle::Detail(UnitIndex unit) const
{
  const BattleState& state = *m_state;
  ViewUnitDetail detail;
  if (!state.Valid(unit) || state.units[unit].kind == UnitKind::Passive) {
    return detail;
  }
  const UnitProfile& profile = state.Profile(unit);
  const UnitEntry& entry = state.snapshot.units[unit];
  detail.unit = unit;
  detail.name = IdentityOf(entry).name;
  detail.briefingKnown = IdentityOf(entry).briefingKnown;
  detail.className = ArchetypeName(profile.archetype);
  if (const auto* ship = std::get_if<CapitalShipEntry>(&entry)) {
    detail.weapons = ship->weapons;
    detail.shipDoctrine = state.EffectiveShipDoctrine(unit);
  } else if (const auto* squadron = std::get_if<SquadronEntry>(&entry)) {
    detail.weapons = squadron->weaponsPerCraft;
    detail.flightDoctrine = state.EffectiveFlightDoctrine(unit);
  }
  detail.tractorRange = profile.tractorRange;
  detail.overrides = state.units[unit].overrides;
  if (const SquadronState* squadron = state.Squadron(unit)) {
    for (int i = 0; i < squadron->craftCount; ++i) {
      const CraftState& craft = state.craft[squadron->firstCraft + i];
      detail.torpedoes += craft.alive ? craft.torpedoes : 0;
    }
  }
  detail.hangarCapacity = profile.hangarCapacity;
  if (detail.hangarCapacity > 0) {
    detail.hangarUsed = detail.hangarCapacity - sim::FreeHangarSlots(state, unit);
  }
  return detail;
}

} // namespace reb::battle
