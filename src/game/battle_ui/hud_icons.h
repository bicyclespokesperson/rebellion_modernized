#pragma once
#include <cstdint>

#include "battle_ui/doctrine_text.h"
#include "battle_ui/draw_list.h"

// Small vector icons for the command HUD, drawn into a square with the draw list's primitives so they stay sharp at
// any UI scale.
namespace reb::battle_ui
{

enum class Icon : uint8_t {
  // doctrine settings
  Role,
  Range,
  Approach,
  Facing,
  Targets,
  Allocation,
  Subsystem,
  Power,
  Withdraw,
  Formation,
  Mission,
  Dogfight,
  Rearm,
  Launch,
  // orders
  Attack,
  Move,
  Hold,
  Escort,
  Tractor,
  Recall,
  Strike,
  Recover,
  DeathStar,
  PlanetaryBattery,
  // commander signals
  ConcentrateFire,
  HoldTheLine,
  Scramble,
  CoordinatedJump,
  // critical systems
  Engines,
  ShieldGenerator,
  WeaponGenerator,
  Hyperdrive,
  // controls
  ChevronDown,
  ChevronRight,
  ChevronUp,
  Close,
  Plus,
  Grip,
  Gear,
  Eye,
  FastForward,
  Override,
  Preset,
};

Icon FieldIcon(DoctrineField field);
Icon SignalIcon(battle::SignalKind signal);

// Draws `icon` centred in `box` (the smaller side sets its size) with lines `scale` UI pixels thick per pixel.
void DrawIcon(DrawList& draw, Icon icon, const Rect& box, Color color, float scale);

} // namespace reb::battle_ui
