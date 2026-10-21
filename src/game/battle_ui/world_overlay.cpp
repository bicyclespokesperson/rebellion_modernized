// What lies on the plane besides the ships: gravity wells and tractor beams, and the Tab overlay's firing arcs, range
// rings, target lines and projected positions (§10.1). Plane shapes go through plane_shapes.h, so they foreshorten
// with the angled view.
#include <array>

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

// With one unit selected, its arcs, range rings and tractor reach; with several, fainter arcs only, so that a task
// force's overlapping coverage stays readable.
void PaintArcsAndRanges(const Painter& p, const ViewUnit& unit, const battle::ViewUnitDetail& detail, bool single)
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
    const Color color = KindColor(p.scene.theme, unit.side, strongestKind);
    PlaneSector(p.out, p.camera, unit.position, 0.0f, reach, start, 2.0f * kArcHalfWidth,
                color.WithAlpha(single ? 22 : 9));
    PlaneArc(p.out, p.camera, unit.position, reach, start, 2.0f * kArcHalfWidth, color.WithAlpha(single ? 150 : 70),
             Stroke{1.5f * p.ui});
    if (single) {
      for (const float edge : {start, start + 2.0f * kArcHalfWidth}) {
        const Vec2 end{unit.position.x + std::cos(edge) * reach, unit.position.y + std::sin(edge) * reach};
        p.out.Line(p.Screen(unit.position), p.Screen(end), color.WithAlpha(60), Stroke{1.0f * p.ui});
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
    const Stroke stroke
        = kind == 0 ? Stroke{1.0f * p.ui, 10.0f * p.ui, 6.0f * p.ui} : Stroke{1.0f * p.ui, 3.0f * p.ui, 6.0f * p.ui};
    PlaneCircle(p.out, p.camera, unit.position, range, KindColor(p.scene.theme, unit.side, kind).WithAlpha(110),
                stroke);
  }
  if (detail.tractorRange > 0.0f) {
    PlaneCircle(p.out, p.camera, unit.position, detail.tractorRange, p.scene.theme.ion.WithAlpha(120),
                Stroke{1.0f * p.ui, 2.0f * p.ui, 4.0f * p.ui});
  }
}

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
    if (Distance(from, to) < 4.0f * p.ui) {
      continue;
    }
    const Color color = FactionColor(p.scene.theme, unit.side).WithAlpha(140);
    p.out.Line(from, to, color, Stroke{1.0f * p.ui, 2.0f * p.ui, 4.0f * p.ui});
    PlaneCircle(p.out, p.camera, ahead, p.camera.MetersFor(4.0f * p.ui), color, Stroke{1.0f * p.ui});
  }
}

} // namespace

void PaintWells(const Painter& p)
{
  for (const battle::ViewWell& well : p.scene.view.wells) {
    if (well.radius <= 0.0f) {
      continue;
    }
    const Color color = FactionColor(p.scene.theme, well.side);
    PlaneCircle(p.out, p.camera, well.center, well.radius, color.WithAlpha(well.active ? 100 : 35),
                Stroke{1.0f * p.ui});
    if (well.active) {
      for (int ripple = 0; ripple < 2; ++ripple) {
        const float phase = Modulo(static_cast<float>(p.scene.seconds * 0.09) + ripple * 0.5f, 1.0f);
        PlaneCircle(p.out, p.camera, well.center, well.radius * (1.0f - phase * 0.65f),
                    color.WithAlpha(static_cast<uint8_t>(28.0f * std::sin(phase * kPi))), Stroke{1.0f * p.ui});
      }
    }
    const ViewUnit* projector = FindUnit(p.scene.view, well.projector);
    if (projector) {
      const Point at = p.Screen(projector->position);
      const float r = std::max(12.0f * p.ui, DrawnRadiusPx(p, *projector) + 4.0f * p.ui);
      const Color marker = color.WithAlpha(well.active ? 180 : 65);
      p.out.Circle(at, r, marker, Stroke{1.0f * p.ui});
      for (int i = 0; i < 4; ++i) {
        const Point direction = ScreenDirection(static_cast<float>(i) * kPi * 0.5f);
        p.out.Line(at + direction * (r + 4.0f * p.ui), at + direction * (r - 3.0f * p.ui), marker,
                   Stroke{1.5f * p.ui});
      }
    }
  }
}

namespace
{

// A circle on the plane that gets an explanatory label: a gravity well, or a side's planetary battery cover.
struct ZoneLabel {
  Vec2 center;
  float radius;
  Side side;
  bool active;
  std::string title;
  std::string help;
};

std::vector<ZoneLabel> ZoneLabels(const Painter& p)
{
  std::vector<ZoneLabel> labels;
  const battle::ViewModel& view = p.scene.view;
  for (const battle::ViewWell& well : view.wells) {
    if (well.radius <= 0.0f) {
      continue;
    }
    const bool own = well.side == p.scene.playerSide;
    const std::string name = well.projector >= 0 && static_cast<size_t>(well.projector) < p.scene.names.size()
                                 ? p.scene.names[well.projector] : "Unknown projector";
    labels.push_back({well.center, well.radius, well.side, well.active,
                      std::string(own ? "YOUR GRAVITY WELL · " : "ENEMY GRAVITY WELL · ") + name,
                      !well.active ? "Projector down · this well is inactive"
                      : own        ? "Enemy ships inside cannot jump to hyperspace"
                                   : "Your ships inside cannot jump to hyperspace"});
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
      labels.push_back({view.planetCenter, view.coverRadius, side, true,
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
  const Rect viewport = p.camera.Viewport().Inset(6.0f * p.ui);
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
    p.out.FillRect({label.x, label.y, 2.0f * p.ui, label.h}, color.WithAlpha(zone.active ? 180 : 70));
    p.out.Text(zone.title, {label.Center().x, label.y + 4.0f * p.ui}, 10.0f * p.ui, color.WithAlpha(220),
               Align::Center, VAlign::Top, FontWeight::Bold, label.w - 12.0f * p.ui);
    p.out.Text(zone.help, {label.Center().x, label.y + 4.0f * p.ui + line}, 10.0f * p.ui, p.scene.theme.textDim,
               Align::Center, VAlign::Top, FontWeight::Regular, label.w - 12.0f * p.ui);
  }
}

void PaintArcsAndRanges(const Painter& p)
{
  for (const battle::ViewUnitDetail& detail : p.scene.selectedDetails) {
    if (const ViewUnit* unit = FindUnit(p.scene.view, detail.unit); unit && Selectable(*unit)) {
      PaintArcsAndRanges(p, *unit, detail, p.scene.selectedDetails.size() == 1);
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
    p.out.FillConvex({a + normal * (1.5f * p.ui), b + normal * spread, b - normal * spread, a - normal * (1.5f * p.ui)},
                     color.WithAlpha(static_cast<uint8_t>(40 * pulse + 20)), Blend::Additive);
    p.out.Line(a, b, color.WithAlpha(static_cast<uint8_t>(120 * pulse)), Stroke{1.0f * p.ui}, Blend::Additive);
  }
}

// Solid while firing, dashed while only assigned: for the selection, or for all of the player's units when nothing is
// selected. Lines run between positions on the plane, under the ships in both views.
void PaintTargetLines(const Painter& p)
{
  const bool all = p.scene.selection.Empty();
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
    const Color color = FactionColor(p.scene.theme, unit.side).WithAlpha(selected ? 200 : 90);
    const Stroke stroke = unit.firing ? Stroke{1.5f * p.ui} : Stroke{1.5f * p.ui, 7.0f * p.ui, 5.0f * p.ui};
    p.out.Line(p.Screen(unit.position), p.Screen(target->position), color, stroke);
  }
  PaintProjections(p);
}

} // namespace reb::battle_ui::painter
