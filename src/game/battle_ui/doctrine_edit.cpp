#include "battle_ui/doctrine_edit.h"

#include <algorithm>

#include "battle_ui/selection.h"

namespace reb::battle_ui
{

using battle::DoctrineOverrideMask;
using battle::UnitIndex;

battle::CommandTarget TargetOf(const DoctrineSubject& subject)
{
  battle::CommandTarget target;
  if (subject.IsGroup()) {
    target.groups = {subject.group};
  } else {
    target.units = subject.units;
  }
  return target;
}

battle::Command SetDoctrineCommand(const DoctrineSubject& subject, const battle::ShipDoctrine& ship,
                                   const battle::FlightDoctrine& flight, DoctrineOverrideMask fields)
{
  battle::Command command;
  command.target = TargetOf(subject);
  if (subject.flight) {
    command.order = battle::OrderSetFlightDoctrine{flight, fields, 0};
  } else {
    command.order = battle::OrderSetShipDoctrine{ship, fields, 0};
  }
  return command;
}

battle::Command ClearOverridesCommand(const DoctrineSubject& subject, DoctrineOverrideMask fields)
{
  battle::Command command;
  command.target = TargetOf(subject);
  if (subject.flight) {
    command.order = battle::OrderSetFlightDoctrine{{}, 0, fields};
  } else {
    command.order = battle::OrderSetShipDoctrine{{}, 0, fields};
  }
  return command;
}

battle::Command PresetCommand(const DoctrineSubject& subject, const DoctrinePreset& preset)
{
  return SetDoctrineCommand(subject, preset.ship, preset.flightDoctrine, preset.fields);
}

void CopyFields(battle::ShipDoctrine& to, const battle::ShipDoctrine& from, DoctrineOverrideMask fields)
{
  const auto copy = [fields](DoctrineOverrideMask bit, auto& destination, const auto& source) {
    if (fields & bit) {
      destination = source;
    }
  };
  copy(battle::kShipRole, to.role, from.role);
  copy(battle::kShipRange, to.range, from.range);
  copy(battle::kShipApproach, to.approach, from.approach);
  copy(battle::kShipFacing, to.facing, from.facing);
  copy(battle::kShipPriorities, to.priorities, from.priorities);
  copy(battle::kShipAllocation, to.allocation, from.allocation);
  copy(battle::kShipSubsystem, to.subsystem, from.subsystem);
  copy(battle::kShipPower, to.power, from.power);
  copy(battle::kShipWithdraw, to.withdraw, from.withdraw);
  copy(battle::kShipFormation, to.formation, from.formation);
  copy(battle::kShipBreakFormation, to.breakFormation, from.breakFormation);
  copy(battle::kShipAllBatteriesOnTarget, to.allBatteriesOnTarget, from.allBatteriesOnTarget);
  copy(battle::kShipScreenTarget, to.screenTarget, from.screenTarget);
}

void CopyFields(battle::FlightDoctrine& to, const battle::FlightDoctrine& from, DoctrineOverrideMask fields)
{
  const auto copy = [fields](DoctrineOverrideMask bit, auto& destination, const auto& source) {
    if (fields & bit) {
      destination = source;
    }
  };
  copy(battle::kFlightMission, to.mission, from.mission);
  copy(battle::kFlightPriorities, to.priorities, from.priorities);
  copy(battle::kFlightDogfight, to.dogfight, from.dogfight);
  copy(battle::kFlightRearm, to.rearm, from.rearm);
  copy(battle::kFlightLaunch, to.launch, from.launch);
  copy(battle::kFlightEscortTarget, to.escortTarget, from.escortTarget);
  copy(battle::kFlightPatrolCenter, to.patrolCenter, from.patrolCenter);
  copy(battle::kFlightPatrolRadius, to.patrolRadius, from.patrolRadius);
}

const battle::ViewGroup* FindGroup(const battle::ViewModel& view, battle::GroupId group)
{
  const auto it = std::ranges::find(view.groups, group, &battle::ViewGroup::id);
  return it == view.groups.end() ? nullptr : &*it;
}

std::vector<UnitIndex> ActiveMembers(const battle::ViewModel& view, const battle::ViewGroup& group)
{
  std::vector<UnitIndex> members;
  for (UnitIndex unit : group.members) {
    if (const battle::ViewUnit* found = FindUnit(view, unit); found && Selectable(*found)) {
      members.push_back(unit);
    }
  }
  return members;
}

DoctrineSubject ResolveSubject(const battle::ViewModel& view, battle::Side side,
                               const std::vector<UnitIndex>& orderable, battle::GroupId focus)
{
  if (const battle::ViewGroup* group = FindGroup(view, focus); group && group->side == side) {
    return {group->flight, group->id, {}};
  }
  if (orderable.empty()) {
    return {};
  }
  const battle::ViewUnit* first = FindUnit(view, orderable.front());
  if (!first) {
    return {};
  }
  const bool flight = first->kind == battle::ViewUnitKind::Squadron;
  if (orderable.size() > 1) {
    if (const battle::ViewGroup* group = FindGroup(view, first->group)) {
      std::vector<UnitIndex> members = ActiveMembers(view, *group);
      std::ranges::sort(members);
      if (members == orderable) {
        return {group->flight, group->id, {}};
      }
    }
  }
  DoctrineSubject subject{flight, battle::kNoGroup, {}};
  for (UnitIndex unit : orderable) {
    const battle::ViewUnit* found = FindUnit(view, unit);
    if (found && (found->kind == battle::ViewUnitKind::Squadron) == flight
        && found->kind != battle::ViewUnitKind::DeathStar) {
      subject.units.push_back(unit);
    }
  }
  return subject;
}

battle::TargetPriorities MovePriority(battle::TargetPriorities priorities, int from, int to)
{
  const int count = priorities.count;
  if (from < 0 || from >= count || to < 0 || to >= count || from == to) {
    return priorities;
  }
  const auto begin = priorities.tags.begin();
  if (from < to) {
    std::rotate(begin + from, begin + from + 1, begin + to + 1);
  } else {
    std::rotate(begin + to, begin + from, begin + from + 1);
  }
  return priorities;
}

battle::TargetPriorities SetPriority(battle::TargetPriorities priorities, int slot, battle::TargetTag tag)
{
  if (slot < 0 || slot > priorities.count || slot >= battle::TargetPriorities::kMaxTags) {
    return priorities;
  }
  const auto begin = priorities.tags.begin();
  const auto end = begin + priorities.count;
  if (const auto existing = std::find(begin, end, tag); existing != end) {
    // A tag appears once: picking one already in the list moves it to the slot instead.
    const int at = static_cast<int>(existing - begin);
    return MovePriority(priorities, at, std::min(slot, priorities.count - 1));
  }
  priorities.tags[slot] = tag;
  if (slot == priorities.count) {
    ++priorities.count;
  }
  return priorities;
}

battle::TargetPriorities RemovePriority(battle::TargetPriorities priorities, int slot)
{
  if (slot < 0 || slot >= priorities.count) {
    return priorities;
  }
  const auto begin = priorities.tags.begin();
  std::rotate(begin + slot, begin + slot + 1, begin + priorities.count);
  --priorities.count;
  priorities.tags[priorities.count] = battle::TargetTag::Nearest;
  return priorities;
}

} // namespace reb::battle_ui
