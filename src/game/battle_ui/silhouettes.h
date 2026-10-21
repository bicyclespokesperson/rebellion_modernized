#pragma once
#include <cstdint>
#include <vector>

#include "battle_ui/ui_types.h"
#include "space_battle/view_model.h"

// Procedural ship outlines, drawn when the sprite pack lacks a class (tactical_combat.md §10.5), and the tactical
// symbols used when zoomed out (§10.1). Outlines are in hull units: x from -1 (stern) to +1 (bow), y to port, so a
// shape is scaled by the unit's radius and rotated to its heading.
namespace reb::battle_ui
{

enum class HullShape : uint8_t {
  Wedge,
  SlimWedge, // Super Star Destroyer
  Interdictor,
  MonCalamari,
  LongHull,
  Corvette,
  Nebulon,
  Carrier,
  Picket,
  Transport,
  XWing,
  YWing,
  AWing,
  BWing,
  TieFighter,
  TieInterceptor,
  TieBomber,
  TieDefender,
  DeathStar,
};

enum class TacticalSymbol : uint8_t {
  Line,
  Heavy,
  Picket,
  Escort,
  Carrier,
  Interdictor,
  Transport,
  Fighter,
  DeathStar
};

struct Silhouette {
  std::vector<std::vector<Point>> fills;   // convex parts, filled
  std::vector<Point> outline;              // the hull's perimeter, stroked
  std::vector<std::vector<Point>> details; // open polylines for surface detail, stroked faintly
  std::vector<Point> engines;              // engine glow positions
};

struct ClassLook {
  HullShape shape = HullShape::LongHull;
  TacticalSymbol symbol = TacticalSymbol::Line;
};

// By tactical ship-type index (app/battle_handoff.cpp), falling back on the unit kind.
ClassLook LookFor(int32_t typeIndex, battle::ViewUnitKind kind);
const Silhouette& SilhouetteFor(HullShape shape);

} // namespace reb::battle_ui
