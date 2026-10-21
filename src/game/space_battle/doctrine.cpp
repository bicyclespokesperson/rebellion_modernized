#include "space_battle/doctrine.h"

#include <algorithm>

namespace reb::battle
{
namespace
{
bool Has(DoctrineOverrideMask mask, DoctrineOverrideMask field)
{
  return (mask & field) != 0;
}
} // namespace

void ApplyShipFields(ShipDoctrine& into, const ShipDoctrine& from, DoctrineOverrideMask mask)
{
  if (Has(mask, kShipRole)) {
    into.role = from.role;
  }
  if (Has(mask, kShipRange)) {
    into.range = from.range;
  }
  if (Has(mask, kShipApproach)) {
    into.approach = from.approach;
  }
  if (Has(mask, kShipFacing)) {
    into.facing = from.facing;
  }
  if (Has(mask, kShipPriorities)) {
    into.priorities = from.priorities;
  }
  if (Has(mask, kShipAllocation)) {
    into.allocation = from.allocation;
  }
  if (Has(mask, kShipSubsystem)) {
    into.subsystem = from.subsystem;
  }
  if (Has(mask, kShipPower)) {
    into.power = from.power;
  }
  if (Has(mask, kShipWithdraw)) {
    into.withdraw = from.withdraw;
  }
  if (Has(mask, kShipFormation)) {
    into.formation = from.formation;
  }
  if (Has(mask, kShipBreakFormation)) {
    into.breakFormation = from.breakFormation;
  }
  if (Has(mask, kShipAllBatteriesOnTarget)) {
    into.allBatteriesOnTarget = from.allBatteriesOnTarget;
  }
  if (Has(mask, kShipScreenTarget)) {
    into.screenTarget = from.screenTarget;
  }
}

void ApplyFlightFields(FlightDoctrine& into, const FlightDoctrine& from, DoctrineOverrideMask mask)
{
  if (Has(mask, kFlightMission)) {
    into.mission = from.mission;
  }
  if (Has(mask, kFlightPriorities)) {
    into.priorities = from.priorities;
  }
  if (Has(mask, kFlightDogfight)) {
    into.dogfight = from.dogfight;
  }
  if (Has(mask, kFlightRearm)) {
    into.rearm = from.rearm;
  }
  if (Has(mask, kFlightLaunch)) {
    into.launch = from.launch;
  }
  if (Has(mask, kFlightEscortTarget)) {
    into.escortTarget = from.escortTarget;
  }
  if (Has(mask, kFlightPatrolCenter)) {
    into.patrolCenter = from.patrolCenter;
  }
  if (Has(mask, kFlightPatrolRadius)) {
    into.patrolRadius = from.patrolRadius;
  }
}

TargetPriorities MakePriorities(std::initializer_list<TargetTag> tags, bool finishDamaged)
{
  TargetPriorities priorities;
  priorities.count = 0;
  for (TargetTag tag : tags) {
    if (priorities.count < TargetPriorities::kMaxTags) {
      priorities.tags[priorities.count++] = tag;
    }
  }
  priorities.finishDamaged = finishDamaged;
  return priorities;
}

ShipDoctrine DefaultShipDoctrine(Archetype archetype, int ordinal)
{
  ShipDoctrine doctrine;
  switch (archetype) {
    case Archetype::BowLine:
      doctrine.role = Role::Line;
      doctrine.approach = Approach::Direct;
      doctrine.priorities = MakePriorities({TargetTag::Heavy, TargetTag::Super, TargetTag::Medium, TargetTag::Light}, true);
      doctrine.formation = Formation::LineAbreast;
      break;
    case Archetype::BroadsideLine:
      doctrine.role = Role::Line;
      doctrine.approach = ordinal % 2 == 0 ? Approach::LeftHook : Approach::RightHook;
      doctrine.priorities = MakePriorities({TargetTag::Heavy, TargetTag::Medium, TargetTag::Super, TargetTag::Light}, true);
      doctrine.formation = Formation::Column;
      break;
    case Archetype::Picket:
      doctrine.role = Role::Screen;
      doctrine.range = EngagementRange::Close;
      doctrine.priorities = MakePriorities({TargetTag::Bomber, TargetTag::Fighter, TargetTag::Light, TargetTag::Nearest}, false);
      doctrine.formation = Formation::Loose;
      break;
    case Archetype::EscortFrigate:
      doctrine.role = Role::Screen;
      doctrine.facing = Facing::BowOn;
      doctrine.priorities = MakePriorities({TargetTag::Bomber, TargetTag::Fighter, TargetTag::Light, TargetTag::Medium}, false);
      doctrine.formation = Formation::Loose;
      break;
    case Archetype::Carrier:
      doctrine.role = Role::Carrier;
      doctrine.range = EngagementRange::StandOff;
      doctrine.priorities = MakePriorities({TargetTag::Bomber, TargetTag::Fighter, TargetTag::Nearest}, false);
      doctrine.formation = Formation::Column;
      break;
    case Archetype::Interdictor:
      doctrine.role = Role::Picket;
      doctrine.range = EngagementRange::StandOff;
      doctrine.priorities = MakePriorities({TargetTag::Bomber, TargetTag::Fighter, TargetTag::Nearest}, false);
      doctrine.formation = Formation::Loose;
      break;
    case Archetype::Transport:
      doctrine.role = Role::Reserve;
      doctrine.range = EngagementRange::StandOff;
      doctrine.priorities = MakePriorities({TargetTag::Fighter, TargetTag::Nearest}, false);
      doctrine.withdraw = WithdrawRule::HullBelow50;
      doctrine.formation = Formation::Column;
      break;
    case Archetype::Interceptor:
    case Archetype::Multirole:
    case Archetype::Bomber:
    case Archetype::Station:
      break;
  }
  return doctrine;
}

FlightDoctrine DefaultFlightDoctrine(Archetype archetype)
{
  FlightDoctrine doctrine;
  switch (archetype) {
    case Archetype::Bomber:
      doctrine.mission = FlightMission::Strike;
      doctrine.priorities
          = MakePriorities({TargetTag::ShieldsDown, TargetTag::Carrier, TargetTag::Transport, TargetTag::Nearest}, true);
      doctrine.dogfight = DogfightRule::AvoidUnlessAttacked;
      break;
    case Archetype::Multirole:
      doctrine.mission = FlightMission::Strike;
      doctrine.priorities
          = MakePriorities({TargetTag::ShieldsDown, TargetTag::Carrier, TargetTag::Damaged, TargetTag::Nearest}, true);
      break;
    default:
      doctrine.mission = FlightMission::Intercept;
      doctrine.priorities = MakePriorities({TargetTag::Bomber, TargetTag::Fighter, TargetTag::Nearest}, false);
      break;
  }
  return doctrine;
}

} // namespace reb::battle
