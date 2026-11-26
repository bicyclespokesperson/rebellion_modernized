// What lies on the plane besides the ships: gravity wells and tractor beams, and the Tab overlay's firing arcs, range
// rings, target lines and projected positions (§10.1). Plane shapes go through plane_shapes.h, so they foreshorten
// with the angled view.
#include <array>
#include <cstdio>
#include <string>

#include "battle_ui/world_painter_internal.h"
#include "space_battle/battle.h"

namespace reb::battle_ui::painter
{
namespace
{

constexpr float kArcHalfWidth = 50.0f * kPi / 180.0f;
constexpr std::array<float, battle::kArcCount> kArcCenters{0.0f, kPi, kPi * 0.5f, -kPi * 0.5f}; // fore, aft, port, stbd
constexpr float kProjectionSeconds = 10.0f;

Color KindColor(const Theme& theme, Side side, int kind)
{
  switch (static_cast<battle::WeaponKind>(kind)) {
    case battle::WeaponKind::Turbolaser:
      return FactionColor(theme, side);
    case battle::WeaponKind::Ion:
      return theme.ion;
    case battle::WeaponKind::Laser:
      return kLaserYellow;
  }
  return theme.text;
}

std::string_view KindName(int kind)
{
  switch (static_cast<battle::WeaponKind>(kind)) {
    case battle::WeaponKind::Turbolaser:
      return "TURBOLASER";
    case battle::WeaponKind::Ion:
      return "ION";
    case battle::WeaponKind::Laser:
      return "LASER";
  }
  return "";
}

// Each weapon kind keeps its own line pattern as well as its colour, so the rings stay apart without colour.
Stroke KindStroke(int kind, float ui)
{
  switch (static_cast<battle::WeaponKind>(kind)) {
    case battle::WeaponKind::Turbolaser:
      return Stroke{1.0f * ui};
    case battle::WeaponKind::Ion:
      return Stroke{1.0f * ui, 10.0f * ui, 5.0f * ui};
    case battle::WeaponKind::Laser:
      return Stroke{1.2f * ui, 2.0f * ui, 4.0f * ui};
  }
  return Stroke{1.0f * ui};
}

// A small tag naming a ring, where the ring crosses the upper right of its centre; a tag that would overlap one
// already placed is left out.
void RingTag(const Painter& p, Vec2 center, float radius, const std::string& text, Color color,
             std::vector<Rect>& placed)
{
  const Point c = p.Screen(center);
  const float rx = p.Px(radius);
  const float ry = rx * p.camera.VerticalScale();
  const Point at{c.x + rx * 0.7071f, c.y - ry * 0.7071f};
  const float size = 9.0f * p.ui;
  const float width = p.scene.text ? p.scene.text->Width(text, size, FontWeight::Bold) : 5.5f * p.ui * text.size();
  const Rect rect{at.x + 4.0f * p.ui, at.y - 13.0f * p.ui, width + 8.0f * p.ui, 12.0f * p.ui};
  if (!p.camera.Viewport().Contains(at) || std::ranges::any_of(placed, [&](const Rect& other) {
        return !Intersection(rect, other).Empty();
      })) {
    return;
  }
  placed.push_back(rect);
  p.out.Line(at, {rect.x, rect.Bottom()}, color.WithAlpha(120), Stroke{1.0f * p.ui});
  p.out.FillRect(rect, Rgb(0x02050a, 190));
  p.out.FillRect({rect.x, rect.Bottom() - 1.0f * p.ui, rect.w, 1.0f * p.ui}, color.WithAlpha(150));
  p.out.Text(text, {rect.x + 4.0f * p.ui, rect.Center().y}, size, color.WithAlpha(230), Align::Start, VAlign::Middle,
             FontWeight::Bold);
}

std::string Kilometers(float meters)
{
  char text[16];
  std::snprintf(text, sizeof text, "%.1f km", meters / 1000.0f);
  return text;
}

// With one unit selected, its firing arcs as coverage that brightens toward its reach, labelled range rings and its
// tractor reach; with several, fainter arcs only, so that a task force's overlapping coverage stays readable.
void PaintArcsAndRanges(const Painter& p, const ViewUnit& unit, const battle::ViewUnitDetail& detail, bool single,
                        std::vector<Rect>& tags)
{
  const battle::WeaponGrid& grid = detail.weapons;
  for (int arc = 0; arc < battle::kArcCount; ++arc) {
    int strongestKind = -1;
    int strongest = 0;
    float reach = 0.0f;
    for (int kind = 0; kind < battle::kWeaponKinds; ++kind) {
      const int points = grid.points[arc][kind];
      if (points <= 0) {
        continue;
      }
      reach = std::max(reach, static_cast<float>(grid.range[kind]) * battle::kMetersPerRangePoint);
      if (points > strongest) {
        strongest = points;
        strongestKind = kind;
      }
    }
    if (strongestKind < 0) {
      continue;
    }
    const float start = unit.heading + kArcCenters[arc] - kArcHalfWidth;
    const float sweep = 2.0f * kArcHalfWidth;
    const Color color = KindColor(p.scene.theme, unit.side, strongestKind);
    PlaneSectorShaded(p.out, p.camera, unit.position, 0.0f, reach, start, sweep, color.WithAlpha(single ? 4 : 0),
                      color.WithAlpha(single ? 46 : 7));
    if (single) {
      PlaneArc(p.out, p.camera, unit.position, reach, start, sweep, color.WithAlpha(70),
               Stroke{1.0f * p.ui, 0, 0, 0, 4.0f * p.ui}, Blend::Additive);
    }
    PlaneArc(p.out, p.camera, unit.position, reach, start, sweep, color.WithAlpha(single ? 190 : 80),
             Stroke{1.5f * p.ui});
    if (single) {
      for (const float edge : {start, start + sweep}) {
        const Vec2 end{unit.position.x + std::cos(edge) * reach, unit.position.y + std::sin(edge) * reach};
        p.out.Line(p.Screen(unit.position), p.Screen(end), color.WithAlpha(50), Stroke{1.0f * p.ui});
      }
    }
  }
  if (!single) {
    return;
  }
  for (int kind = 0; kind < battle::kWeaponKinds; ++kind) {
    const float range = static_cast<float>(grid.range[kind]) * battle::kMetersPerRangePoint;
    if (range <= 0.0f) {
      continue;
    }
    const Color color = KindColor(p.scene.theme, unit.side, kind);
    PlaneCircle(p.out, p.camera, unit.position, range, color.WithAlpha(120), KindStroke(kind, p.ui));
    RingTag(p, unit.position, range, std::string(KindName(kind)) + " " + Kilometers(range), color, tags);
  }
  if (detail.tractorRange > 0.0f) {
    PlaneCircle(p.out, p.camera, unit.position, detail.tractorRange, p.scene.theme.ion.WithAlpha(130),
                Stroke{1.0f * p.ui, 1.5f * p.ui, 3.5f * p.ui});
    RingTag(p, unit.position, detail.tractorRange, "TRACTOR " + Kilometers(detail.tractorRange), p.scene.theme.ion,
            tags);
  }
}

// Where each selected unit will be in a few seconds: a dotted track and a ghost chevron pointing along it.
void PaintProjections(const Painter& p)
{
  for (const ViewUnit& unit : p.scene.view.units) {
    if (!Selectable(unit) || !p.scene.selection.Contains(unit.unit)) {
      continue;
    }
    const Vec2 ahead{unit.position.x + unit.velocity.x * kProjectionSeconds,
                     unit.position.y + unit.velocity.y * kProjectionSeconds};
    const Point from = p.Screen(unit.position);
    const Point to = p.Screen(ahead);
    const float length = Distance(from, to);
    if (length < 4.0f * p.ui) {
      continue;
    }
    const Color color = FactionColor(p.scene.theme, unit.side).WithAlpha(150);
    p.out.Line(from, to, color, Stroke{1.0f * p.ui, 1.5f * p.ui, 4.0f * p.ui});
    const Point dir = (to - from) * (1.0f / length);
    const Point side{-dir.y, dir.x};
    const float s = 5.0f * p.ui;
    p.out.Polyline({to - dir * s + side * s, to, to - dir * s - side * s}, color, Stroke{1.5f * p.ui});
  }
}

// Brackets around a target, in the colour of the side aiming at it.
void PaintTargetReticle(const Painter& p, const ViewUnit& target, Color color)
{
  const Point center = p.Screen(target.position);
  const float r = std::max(DrawnRadiusPx(p, target) + 6.0f * p.ui, 11.0f * p.ui);
  for (int i = 0; i < 4; ++i) {
    const float mid = kPi * 0.25f + static_cast<float>(i) * kPi * 0.5f;
    p.out.Arc(center, r, mid - 0.32f, 0.64f, color, Stroke{1.5f * p.ui});
  }
}

float WellMarkerRadiusPx(const Painter& p, const battle::ViewWell& well)
{
  const ViewUnit* projector = FindUnit(p.scene.view, well.projector);
  return std::max(12.0f * p.ui, projector ? DrawnRadiusPx(p, *projector) + 4.0f * p.ui : 0.0f);
}

} // namespace

// The well has no range: it holds every enemy ship on the field, so only the projector is marked, with ripples
// running outward from it.
void PaintWells(const Painter& p)
{
  for (const battle::ViewWell& well : p.scene.view.wells) {
    const Color color = FactionColor(p.scene.theme, well.side);
    const Point at = p.Screen(well.center);
    const float r = WellMarkerRadiusPx(p, well);
    for (int ripple = 0; ripple < 2; ++ripple) {
      const float phase = Modulo(static_cast<float>(p.scene.seconds * 0.5) + ripple * 0.5f, 1.0f);
      p.out.Circle(at, r * (1.0f + phase * 0.8f), color.WithAlpha(static_cast<uint8_t>(90.0f * (1.0f - phase))),
                   Stroke{1.0f * p.ui});
    }
    const Color marker = color.WithAlpha(180);
    p.out.Circle(at, r, marker, Stroke{1.0f * p.ui});
    for (int i = 0; i < 4; ++i) {
      const Point direction = ScreenDirection(static_cast<float>(i) * kPi * 0.5f);
      p.out.Line(at + direction * (r + 4.0f * p.ui), at + direction * (r - 3.0f * p.ui), marker, Stroke{1.5f * p.ui});
    }
  }
}

namespace
{

// A circle on the plane that gets an explanatory label: a gravity-well projector's marker, or a side's planetary
// battery cover.
struct ZoneLabel {
  Vec2 center;
  float radius;
  Side side;
  std::string title;
  std::string help;
};

std::vector<ZoneLabel> ZoneLabels(const Painter& p)
{
  std::vector<ZoneLabel> labels;
  const battle::ViewModel& view = p.scene.view;
  for (const battle::ViewWell& well : view.wells) {
    const bool own = well.side == p.scene.playerSide;
    const std::string name = well.projector >= 0 && static_cast<size_t>(well.projector) < p.scene.names.size()
                                 ? p.scene.names[well.projector] : "Unknown projector";
    labels.push_back({well.center, WellMarkerRadiusPx(p, well) / p.Px(1.0f), well.side,
                      std::string(own ? "YOUR GRAVITY WELL · " : "ENEMY GRAVITY WELL · ") + name,
                      own ? "No enemy ship can jump to hyperspace while it survives"
                          : "None of your ships can jump to hyperspace until it is destroyed"});
  }
  if (view.coverRadius > 0.0f) {
    for (const Side side : {Side::Alliance, Side::Empire}) {
      std::vector<const battle::ViewDefense*> batteries;
      bool ion = false;
      for (const battle::ViewDefense& defense : view.defenses) {
        if (defense.side == side && DefenseVisible(defense, p.scene.playerSide)) {
          batteries.push_back(&defense);
          ion = ion || defense.weapon == battle::WeaponKind::Ion;
        }
      }
      if (batteries.empty()) {
        continue;
      }
      const bool own = side == p.scene.playerSide;
      std::string help = own ? "Fires on enemy capital ships inside this ring" : "Fires on your capital ships inside this ring";
      help += ion ? " · ion knocks out engines" : "";
      labels.push_back({view.planetCenter, view.coverRadius, side,
                        std::string(own ? "YOUR PLANETARY DEFENSES · " : "ENEMY PLANETARY DEFENSES · ")
                            + DescribeBatteries(batteries),
                        help});
    }
  }
  return labels;
}

} // namespace

void PaintZoneLabels(const Painter& p)
{
  const Rect bounds = p.scene.labelBounds.Empty() ? p.camera.Viewport() : p.scene.labelBounds;
  const Rect viewport = bounds.Inset(6.0f * p.ui);
  std::vector<Rect> placed;
  for (const ZoneLabel& zone : ZoneLabels(p)) {
    const Point center = p.Screen(zone.center);
    const float rx = p.Px(zone.radius);
    const float ry = rx * p.camera.VerticalScale();
    Point top{center.x, center.y - ry};
    if (!viewport.Contains(top)) {
      bool found = false;
      for (int i = 0; i < 144; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) / 144.0f;
        const Point point{center.x + rx * std::cos(angle), center.y - ry * std::sin(angle)};
        if (viewport.Contains(point) && (!found || point.y < top.y)) {
          top = point;
          found = true;
        }
      }
      if (!found) {
        continue;
      }
    }
    const auto width = [&](const std::string& text, FontWeight weight) {
      return p.scene.text ? p.scene.text->Width(text, 10.0f * p.ui, weight)
                          : 5.6f * p.ui * static_cast<float>(text.size());
    };
    const float line = p.scene.text ? p.scene.text->LineHeight(10.0f * p.ui) : 13.0f * p.ui;
    const float w = std::min(viewport.w, std::max(width(zone.title, FontWeight::Bold), width(zone.help, FontWeight::Regular))
                                             + 12.0f * p.ui);
    // The screen's pause/status banner occupies the top of the world viewport.
    Rect label{std::clamp(top.x - w * 0.5f, viewport.x, viewport.Right() - w),
               std::max(top.y + 8.0f * p.ui, viewport.y + 44.0f * p.ui),
               w, 2.0f * line + 8.0f * p.ui};
    // Keep every label readable, even for concentric circles: pack them downward in stable order.
    bool collision = true;
    while (collision) {
      collision = false;
      for (const Rect& other : placed) {
        if (!Intersection(label, other).Empty()) {
          label.y = other.Bottom() + 4.0f * p.ui;
          collision = true;
        }
      }
    }
    if (label.Bottom() > viewport.Bottom()) {
      continue;
    }
    placed.push_back(label);
    const Color color = FactionColor(p.scene.theme, zone.side);
    p.out.Line(top, {label.Center().x, label.y}, color.WithAlpha(90), Stroke{1.0f * p.ui});
    p.out.FillRect(label, Rgb(0x050c18, 220));
    p.out.FillRect({label.x, label.y, 2.0f * p.ui, label.h}, color.WithAlpha(180));
    p.out.Text(zone.title, {label.Center().x, label.y + 4.0f * p.ui}, 10.0f * p.ui, color.WithAlpha(220),
               Align::Center, VAlign::Top, FontWeight::Bold, label.w - 12.0f * p.ui);
    p.out.Text(zone.help, {label.Center().x, label.y + 4.0f * p.ui + line}, 10.0f * p.ui, p.scene.theme.textDim,
               Align::Center, VAlign::Top, FontWeight::Regular, label.w - 12.0f * p.ui);
  }
}

void PaintArcsAndRanges(const Painter& p)
{
  std::vector<Rect> tags;
  for (const battle::ViewUnitDetail& detail : p.scene.selectedDetails) {
    if (const ViewUnit* unit = FindUnit(p.scene.view, detail.unit); unit && Selectable(*unit)) {
      PaintArcsAndRanges(p, *unit, detail, p.scene.selectedDetails.size() == 1, tags);
    }
  }
}

void PaintTractors(const Painter& p)
{
  for (const battle::ViewTractor& tractor : p.scene.view.tractors) {
    const ViewUnit* from = FindUnit(p.scene.view, tractor.from);
    const ViewUnit* to = FindUnit(p.scene.view, tractor.to);
    if (!from || !to) {
      continue;
    }
    const Point a = p.Screen(from->position);
    const Point b = p.Screen(to->position);
    const float length = Distance(a, b);
    if (length < 1.0f) {
      continue;
    }
    const Point dir = (b - a) * (1.0f / length);
    const Point normal{-dir.y, dir.x};
    const float spread = std::max(p.Px(to->radius) * 0.7f, 4.0f * p.ui);
    const float pulse = 0.6f + 0.4f * static_cast<float>(std::sin(p.scene.seconds * 6.0));
    const Color color = p.scene.theme.ion;
    // A cone of light, strongest at the emitter, with waves running back along it as the beam pulls.
    const Color source = color.WithAlpha(static_cast<uint8_t>(70 * pulse + 30));
    const Color far = color.WithAlpha(static_cast<uint8_t>(20 * pulse + 10));
    const float mouth = 1.5f * p.ui;
    p.out.FillConvexShaded({a + normal * mouth, b + normal * spread, b - normal * spread, a - normal * mouth},
                           {source, far, far, source}, Blend::Additive);
    p.out.Line(a, b, color.WithAlpha(static_cast<uint8_t>(130 * pulse)), Stroke{1.0f * p.ui, 0, 0, 0, 2.0f * p.ui},
               Blend::Additive);
    constexpr int kWaves = 4;
    for (int wave = 0; wave < kWaves; ++wave) {
      const float t = 1.0f - Modulo(static_cast<float>(p.scene.seconds * 0.6) + static_cast<float>(wave) / kWaves, 1.0f);
      const Point at = a + (b - a) * t;
      const float half = mouth + (spread - mouth) * t;
      p.out.Line(at + normal * half, at - normal * half, color.WithAlpha(static_cast<uint8_t>(110 * std::sin(t * kPi))),
                 Stroke{1.0f * p.ui}, Blend::Additive);
    }
    p.out.Glow(a, 6.0f * p.ui, color.WithAlpha(150));
  }
}

// Solid while firing, marching dashes while only assigned: for the selection, or for all of the player's units when
// nothing is selected. Lines run between positions on the plane, under the ships in both views; a selected unit's
// target is bracketed.
void PaintTargetLines(const Painter& p)
{
  const bool all = p.scene.selection.Empty();
  std::vector<battle::UnitIndex> bracketed;
  for (const ViewUnit& unit : p.scene.view.units) {
    if (!Selectable(unit) || unit.target == battle::kNoUnit) {
      continue;
    }
    const bool selected = p.scene.selection.Contains(unit.unit);
    if (!selected && !(all && unit.side == p.scene.playerSide)) {
      continue;
    }
    const ViewUnit* target = FindUnit(p.scene.view, unit.target);
    if (!target) {
      continue;
    }
    const Color color = FactionColor(p.scene.theme, unit.side).WithAlpha(selected ? 210 : 90);
    const Point from = p.Screen(unit.position);
    const Point to = p.Screen(target->position);
    if (unit.firing) {
      if (selected) {
        p.out.Line(from, to, color.WithAlpha(50), Stroke{1.0f * p.ui, 0, 0, 0, 3.0f * p.ui}, Blend::Additive);
      }
      p.out.Line(from, to, color, Stroke{1.5f * p.ui});
    } else {
      const float march = -static_cast<float>(std::fmod(p.scene.seconds * 24.0, 1000.0)) * p.ui;
      p.out.Line(from, to, color, Stroke{1.5f * p.ui, 7.0f * p.ui, 5.0f * p.ui, march});
    }
    if (selected && std::ranges::find(bracketed, target->unit) == bracketed.end()) {
      bracketed.push_back(target->unit);
      PaintTargetReticle(p, *target, color);
    }
  }
  PaintProjections(p);
}

} // namespace reb::battle_ui::painter
