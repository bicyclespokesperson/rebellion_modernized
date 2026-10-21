#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

#include "battle_ui/plane_shapes.h"
#include "battle_ui/world_painter.h"

// Shared by the world painter's parts: world_painter.cpp (units and markers), world_backdrop.cpp (space, the planet
// and the grid), world_overlay.cpp (wells, tractors and the Tab overlay) and world_effects.cpp (bolts, effects and
// damage).
namespace reb::battle_ui::painter
{

using battle::Side;
using battle::Vec2;
using battle::ViewUnit;
using battle::ViewUnitKind;

inline const Color kSpaceDeep = Rgb(0x04060b);
inline const Color kSpaceTactical = Rgb(0x050c15);
inline const Color kGridMinor = Rgb(0x4a78a8, 34);
inline const Color kGridMajor = Rgb(0x5a8cbe, 64);
inline const Color kBounds = Rgb(0x5a8cbe, 110);
inline const Color kAllianceHull = Rgb(0xbdb6aa);
inline const Color kEmpireHull = Rgb(0xa7afb9);
inline const Color kAllianceEngine = Rgb(0xff8a6a);
inline const Color kEmpireEngine = Rgb(0x9fd0ff);
inline const Color kLaserYellow = Rgb(0xf0d060);
inline const Color kExplosionCore = Rgb(0xffe6a0);
inline const Color kExplosionOuter = Rgb(0xff7a28);

struct Painter {
  DrawList& out;
  const WorldScene& scene;
  const Camera& camera;
  float ui;
  bool detailed; // the detailed look (zoomed in, overlay off), else the tactical one (§10.4)

  bool Angled() const
  {
    return camera.View() == CameraView::Angled;
  }
  Point Screen(Vec2 world, float height = 0.0f) const
  {
    return camera.WorldToScreen(world, height);
  }
  float Px(float meters) const
  {
    return camera.PixelsFor(meters);
  }
  SpriteView View() const
  {
    return Angled() ? SpriteView::Angled : SpriteView::TopDown;
  }
  // Fighters fly above the plane in the angled view; the top-down view shows height with offset shadows instead.
  float CraftHeight() const
  {
    return Angled() ? Camera::kFlightHeight : 0.0f;
  }
};

inline uint32_t Hash(uint32_t x)
{
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

inline float Unit01(uint32_t x)
{
  return static_cast<float>(Hash(x) & 0xffffff) / static_cast<float>(0x1000000);
}

// A point in hull units (x from stern -1 to bow +1, y to port) of a ship at `position` and `heading`, `scale` meters
// per unit.
inline Vec2 HullToWorld(Vec2 position, float heading, float scale, Point hull)
{
  const float c = std::cos(heading);
  const float s = std::sin(heading);
  return {position.x + (hull.x * c - hull.y * s) * scale, position.y + (hull.x * s + hull.y * c) * scale};
}

// A point of a screen-space figure pointing along `angle`.
inline Point SymbolPoint(Point center, float angle, float size, Point local)
{
  const Point forward = ScreenDirection(angle);
  const Point left = ScreenDirection(angle + kPi * 0.5f);
  return center + forward * (local.x * size) + left * (local.y * size);
}

inline void Quad(std::vector<Vertex>& vertices, Point center, float half, Color color)
{
  const Point a{center.x - half, center.y - half};
  const Point b{center.x + half, center.y - half};
  const Point c{center.x + half, center.y + half};
  const Point d{center.x - half, center.y + half};
  vertices.insert(vertices.end(), {{a, color}, {b, color}, {c, color}, {a, color}, {c, color}, {d, color}});
}

inline float Modulo(float value, float modulus)
{
  return value - std::floor(value / modulus) * modulus;
}

// The hull length a capital ship or the Death Star is drawn at: the pack's class length when the art is used (the
// simulator's sizes follow it), else the simulator's diameter.
float DrawnLengthMeters(const Painter& p, const ViewUnit& unit);
// How far a unit's drawing reaches from its position on screen, for placing markers.
float DrawnRadiusPx(const Painter& p, const ViewUnit& unit);

// world_backdrop.cpp
void PaintBackdrop(const Painter& p);
void PaintGrid(const Painter& p);

// world_overlay.cpp
void PaintWells(const Painter& p);
// Labels for gravity wells and planetary battery cover, packed so that none overlap.
void PaintZoneLabels(const Painter& p);
void PaintArcsAndRanges(const Painter& p);
void PaintTractors(const Painter& p);
void PaintTargetLines(const Painter& p);

// world_planetary.cpp: planetary batteries' cover ring and their emplacements on the limb (§6.13)
void PaintPlanetaryCover(const Painter& p);
void PaintEmplacements(const Painter& p);

// world_effects.cpp
void PaintShots(const Painter& p);
void PaintEffects(const Painter& p);
// Visible critical damage on a ship drawn `lengthMeters` long: sputtering engines, venting, scorched sections.
void PaintDamage(const Painter& p, const ViewUnit& unit, float lengthMeters);

} // namespace reb::battle_ui::painter
