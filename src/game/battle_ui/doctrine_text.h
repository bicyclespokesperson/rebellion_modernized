#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "space_battle/commands.h"

// Names and one-sentence explanations for every doctrine setting and option (tactical_combat.md §5.2, §5.3), and a
// uniform way to read and write the choice fields, so the doctrine card, its tooltips and the presets share one table.
namespace reb::battle_ui
{

// The settings on the doctrine card, capital-ship fields first, then the flight card's.
enum class DoctrineField : uint8_t {
  Role,
  Range,
  Approach,
  Facing,
  Priorities,
  Allocation,
  Subsystem,
  Power,
  Withdraw,
  Formation,
  Mission,
  Dogfight,
  Rearm,
  Launch,
};

struct OptionText {
  std::string_view name;
  std::string_view shortName; // for the compact card
  std::string_view help;      // one sentence
};

struct FieldText {
  std::string_view label;
  std::string_view help;
};

const FieldText& Describe(DoctrineField field);
bool IsFlightField(DoctrineField field);
// The field's bit in the override mask of a ship's or a flight's doctrine (Priorities is on both cards).
battle::DoctrineOverrideMask FieldMask(DoctrineField field, bool flight);
// The card's fields in display order.
std::span<const DoctrineField> ShipFields();
std::span<const DoctrineField> FlightFields();

// Choice fields: every field but Priorities.
int OptionCount(DoctrineField field);
const OptionText& Option(DoctrineField field, int index);
int GetOption(DoctrineField field, const battle::ShipDoctrine& ship, const battle::FlightDoctrine& flight);
void SetOption(DoctrineField field, int index, battle::ShipDoctrine& ship, battle::FlightDoctrine& flight);
// True when the field's value differs between the two doctrines (of ships, or with `flight` of flights).
bool FieldDiffers(DoctrineField field, bool flight, const battle::ShipDoctrine& shipA,
                  const battle::FlightDoctrine& flightA, const battle::ShipDoctrine& shipB,
                  const battle::FlightDoctrine& flightB);

// Target tags (§5.3).
inline constexpr int kTargetTagCount = static_cast<int>(battle::TargetTag::Nearest) + 1;
enum class TagGroup : uint8_t { Size, Function, State, Fallback };
const OptionText& Describe(battle::TargetTag tag);
TagGroup GroupOf(battle::TargetTag tag);
std::string_view Name(TagGroup group);
// "Carriers, Interdictors, Damaged" (with "+ finish damaged" when set).
std::string PriorityText(const battle::TargetPriorities& priorities);

// Commander signals (§5.6).
const OptionText& Describe(battle::SignalKind signal);

} // namespace reb::battle_ui
