#pragma once
#include <vector>

#include "battle_ui/hud_settings.h"
#include "space_battle/commands.h"
#include "space_battle/view_model.h"

// Doctrine edits as commands (tactical_combat.md §5.1-§5.3). Edits on a group set the group's doctrine; edits on
// ships or squadrons set per-unit overrides, which `clear` drops again. Each command carries only the edited fields.
namespace reb::battle_ui
{

// What the doctrine card edits: one group, or a set of units of one kind.
struct DoctrineSubject {
  bool flight = false;
  battle::GroupId group = battle::kNoGroup;
  std::vector<battle::UnitIndex> units; // when not a group

  bool IsGroup() const
  {
    return group != battle::kNoGroup;
  }
  bool Empty() const
  {
    return group == battle::kNoGroup && units.empty();
  }
  friend bool operator==(const DoctrineSubject&, const DoctrineSubject&) = default;
};

battle::CommandTarget TargetOf(const DoctrineSubject& subject);

// Sets the subject's `fields` (ship or flight fields, by the subject's kind) to the values in the doctrines.
battle::Command SetDoctrineCommand(const DoctrineSubject& subject, const battle::ShipDoctrine& ship,
                                   const battle::FlightDoctrine& flight, battle::DoctrineOverrideMask fields);
// Drops the units' overrides of `fields`, so they follow their group again.
battle::Command ClearOverridesCommand(const DoctrineSubject& subject, battle::DoctrineOverrideMask fields);
battle::Command PresetCommand(const DoctrineSubject& subject, const DoctrinePreset& preset);

// Copies the masked fields between doctrines, as the simulator applies them.
void CopyFields(battle::ShipDoctrine& to, const battle::ShipDoctrine& from, battle::DoctrineOverrideMask fields);
void CopyFields(battle::FlightDoctrine& to, const battle::FlightDoctrine& from, battle::DoctrineOverrideMask fields);

const battle::ViewGroup* FindGroup(const battle::ViewModel& view, battle::GroupId group);
// The group's members that are still on the plane (selectable), in member order.
std::vector<battle::UnitIndex> ActiveMembers(const battle::ViewModel& view, const battle::ViewGroup& group);

// The doctrine the card edits for the player's selection. A group picked in the roster (`focus`) stays the subject;
// otherwise a selection that is exactly a group's active members (two or more) edits the group, one unit edits that
// unit's overrides, and a mixed selection edits overrides on the units of the first unit's kind.
DoctrineSubject ResolveSubject(const battle::ViewModel& view, battle::Side side,
                               const std::vector<battle::UnitIndex>& orderable, battle::GroupId focus);

// Target priority list edits: reorder, replace or append (`slot` == count), remove.
battle::TargetPriorities MovePriority(battle::TargetPriorities priorities, int from, int to);
battle::TargetPriorities SetPriority(battle::TargetPriorities priorities, int slot, battle::TargetTag tag);
battle::TargetPriorities RemovePriority(battle::TargetPriorities priorities, int slot);

} // namespace reb::battle_ui
