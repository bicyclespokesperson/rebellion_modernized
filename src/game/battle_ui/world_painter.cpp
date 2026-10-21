// The battle world's units and markers, and the order everything is painted in. In the angled view, ships and craft
// are depth-sorted (painter's order: farther from the camera first) and fighters fly above their shadows on the plane.
#include "battle_ui/world_painter.h"

#include <algorithm>
#include <string>

#include "battle_ui/silhouettes.h"
#include "battle_ui/world_painter_internal.h"

namespace reb::battle_ui
{
namespace painter
{
namespace
{

constexpr float kCraftLength = 24.0f;     // meters, as drawn without art
constexpr float kMinCraftPx = 5.0f;       // UI pixels
constexpr float kMinCraftSpritePx = 9.0f; // UI pixels
constexpr float kSymbolBelow = 9.0f;      // UI pixels: a ship smaller than this on screen becomes a symbol
constexpr size_t kCrowd = 24;             // more capital ships on screen than this: only key units are labelled
constexpr size_t kLabelledSelection = 4;  // larger selections label only their group leaders
constexpr float kLabelSize = 11.0f;       // UI pixels

const Color kSelectOwn = Rgb(0xd8f0ff);
const Color kSelectOther = Rgb(0xff7a6a);

// ---- capital ships -------------------------------------------------------------------------------------------

void PaintSilhouette(const Painter& p, const Silhouette& silhouette, Vec2 position, float heading, float scale,
                     Color fill, Color outline, Color detail)
{
  const auto toScreen = [&](const std::vector<Point>& hull) {
    std::vector<Point> points;
    points.reserve(hull.size());
    for (const Point& h : hull) {
      points.push_back(p.Screen(HullToWorld(position, heading, scale, h)));
    }
    return points;
  };
  for (const std::vector<Point>& part : silhouette.fills) {
    p.out.FillConvex(toScreen(part), fill);
  }
  if (detail.a > 0) {
    for (const std::vector<Point>& line : silhouette.details) {
      p.out.Polyline(toScreen(line), detail, Stroke{std::max(1.0f, 0.8f * p.ui)});
    }
  }
  if (outline.a > 0 && !silhouette.outline.empty()) {
    p.out.Polyline(toScreen(silhouette.outline), outline, Stroke{std::max(1.0f, 1.2f * p.ui)}, true);
  }
}

void PaintEngines(const Painter& p, const Silhouette& silhouette, const ViewUnit& unit, float scale)
{
  const bool disrupted = (unit.flags & battle::kViewDisrupted) != 0;
  if (disrupted && std::fmod(p.scene.seconds * 7.0 + unit.unit, 2.0) < 1.0) {
    return;
  }
  const float spoolBoost = (unit.flags & battle::kViewWithdrawing) ? 1.0f + 1.5f * unit.spool : 1.0f;
  const Color color = unit.side == Side::Alliance ? kAllianceEngine : kEmpireEngine;
  const float radius = std::max(1.2f * p.ui, p.Px(scale) * 0.028f) * spoolBoost;
  for (const Point& engine : silhouette.engines) {
    const Point at = p.Screen(HullToWorld(unit.position, unit.heading, scale, {engine.x - 0.02f, engine.y}));
    p.out.FillCircle(at, radius * 2.0f, color.WithAlpha(45), Blend::Additive);
    p.out.FillCircle(at, radius, color.WithAlpha(200), Blend::Additive);
  }
}

// Zoomed out, a ship is a faction-coloured symbol by archetype, pointing along its projected heading.
void PaintSymbol(const Painter& p, Point center, float angle, Side side, TacticalSymbol symbol)
{
  const Color color = FactionColor(p.scene.theme, side);
  const Color fill = color.WithAlpha(190);
  const auto at = [&](float size, Point local) {
    return SymbolPoint(center, angle, size, local);
  };
  const auto polygon = [&](float size, std::initializer_list<Point> points) {
    std::vector<Point> screen;
    for (const Point& point : points) {
      screen.push_back(at(size, point));
    }
    return screen;
  };
  const float s = 8.0f * p.ui;
  switch (symbol) {
    case TacticalSymbol::Line:
    case TacticalSymbol::Heavy:
      {
        const float size = symbol == TacticalSymbol::Heavy ? s * 1.35f : s;
        p.out.FillConvex(polygon(size, {{1.0f, 0.0f}, {-0.75f, 0.7f}, {-0.35f, 0.0f}}), fill);
        p.out.FillConvex(polygon(size, {{1.0f, 0.0f}, {-0.35f, 0.0f}, {-0.75f, -0.7f}}), fill);
        p.out.Polyline(polygon(size, {{1.0f, 0.0f}, {-0.75f, 0.7f}, {-0.35f, 0.0f}, {-0.75f, -0.7f}}), color,
                       Stroke{1.0f * p.ui}, true);
        break;
      }
    case TacticalSymbol::Picket:
    case TacticalSymbol::Escort:
      p.out.FillConvex(polygon(s * 0.9f, {{1.0f, 0.0f}, {0.0f, 0.5f}, {-0.9f, 0.0f}, {0.0f, -0.5f}}), fill);
      if (symbol == TacticalSymbol::Escort) {
        p.out.Line(at(s, {-0.9f, 0.6f}), at(s, {-0.9f, -0.6f}), color, Stroke{1.5f * p.ui});
      }
      break;
    case TacticalSymbol::Carrier:
      p.out.FillConvex(polygon(s, {{0.9f, 0.5f}, {-0.9f, 0.5f}, {-0.9f, -0.5f}, {0.9f, -0.5f}}), fill);
      p.out.Line(at(s, {0.7f, 0.0f}), at(s, {-0.7f, 0.0f}), kSpaceDeep.WithAlpha(200), Stroke{1.5f * p.ui});
      break;
    case TacticalSymbol::Interdictor:
      p.out.FillConvex(polygon(s, {{1.0f, 0.0f}, {-0.8f, 0.7f}, {-0.8f, -0.7f}}), fill);
      p.out.Circle(center, s * 1.25f, color, Stroke{1.0f * p.ui});
      break;
    case TacticalSymbol::Transport:
      p.out.FillConvex(polygon(s * 0.75f, {{1.0f, 0.4f},
                                           {0.4f, 1.0f},
                                           {-0.4f, 1.0f},
                                           {-1.0f, 0.4f},
                                           {-1.0f, -0.4f},
                                           {-0.4f, -1.0f},
                                           {0.4f, -1.0f},
                                           {1.0f, -0.4f}}),
                       fill.WithAlpha(120));
      p.out.Circle(center, s * 0.8f, color, Stroke{1.0f * p.ui});
      break;
    case TacticalSymbol::Fighter:
      p.out.FillConvex(polygon(s * 0.7f, {{1.0f, 0.0f}, {-0.8f, 0.7f}, {-0.3f, 0.0f}}), fill);
      p.out.FillConvex(polygon(s * 0.7f, {{1.0f, 0.0f}, {-0.3f, 0.0f}, {-0.8f, -0.7f}}), fill);
      break;
    case TacticalSymbol::DeathStar:
      p.out.FillCircle(center, s * 1.6f, fill.WithAlpha(120));
      p.out.Circle(center, s * 1.6f, color, Stroke{1.5f * p.ui});
      break;
  }
}

void PaintCapitalShip(const Painter& p, const ViewUnit& unit)
{
  const ClassLook look = LookFor(unit.typeIndex, unit.kind);
  if (p.Px(unit.radius) < kSymbolBelow * p.ui) {
    PaintSymbol(p, p.Screen(unit.position), p.camera.ProjectedAngle(unit.heading), unit.side, look.symbol);
    return;
  }
  const Color faction = FactionColor(p.scene.theme, unit.side);
  const Silhouette& silhouette = SilhouetteFor(look.shape);
  const float lengthMeters = DrawnLengthMeters(p, unit);
  if (p.scene.sprites.Has(unit.typeIndex, p.View())) {
    DrawSprite sprite;
    sprite.typeIndex = unit.typeIndex;
    sprite.view = p.View();
    sprite.center = p.Screen(unit.position);
    sprite.heading = p.camera.ScreenHeading(unit.heading);
    sprite.length = p.Px(lengthMeters);
    sprite.tint = p.detailed ? Color{} : Mix(Color{}, faction, 0.6f);
    p.out.Sprite(sprite);
  } else if (p.detailed) {
    const Color hull = unit.side == Side::Alliance ? kAllianceHull : kEmpireHull;
    PaintSilhouette(p, silhouette, unit.position, unit.heading, unit.radius, hull,
                    Mix(hull, faction, 0.55f).WithAlpha(230), Mix(hull, kSpaceDeep, 0.45f).WithAlpha(200));
    PaintEngines(p, silhouette, unit, unit.radius);
  } else {
    PaintSilhouette(p, silhouette, unit.position, unit.heading, unit.radius, faction.WithAlpha(70),
                    faction.WithAlpha(235), faction.WithAlpha(80));
  }
  if (p.detailed) {
    PaintDamage(p, unit, lengthMeters);
  }
  if (unit.flags & battle::kViewDisrupted) {
    const float flicker = 0.5f + 0.5f * static_cast<float>(std::sin(p.scene.seconds * 23.0 + unit.unit));
    PlaneCircle(p.out, p.camera, unit.position, lengthMeters * 0.58f,
                p.scene.theme.ion.WithAlpha(static_cast<uint8_t>(60 + 120 * flicker)),
                Stroke{1.5f * p.ui, 3.0f * p.ui, 5.0f * p.ui}, Blend::Additive);
  }
}

void PaintDeathStar(const Painter& p, const ViewUnit& unit)
{
  const Point center = p.Screen(unit.position);
  const Color faction = FactionColor(p.scene.theme, unit.side);
  if (p.scene.sprites.Has(unit.typeIndex, p.View())) {
    const float length = std::max(p.Px(DrawnLengthMeters(p, unit)), 20.0f * p.ui);
    p.out.Sprite({unit.typeIndex, p.View(), SpriteLayer::Color, center, p.camera.ScreenHeading(unit.heading), length,
                  p.detailed ? Color{} : Mix(Color{}, faction, 0.6f)});
    return;
  }
  const float radius = std::max(p.Px(unit.radius), 10.0f * p.ui);
  p.out.FillCircle(center, radius, p.detailed ? Rgb(0x8a9098) : faction.WithAlpha(60));
  p.out.Line(center + Point{-radius, 0.0f}, center + Point{radius, 0.0f}, Rgb(0x30343a, 200), Stroke{1.5f * p.ui});
  const Point dish = center + Point{radius * 0.35f, -radius * 0.4f};
  p.out.FillCircle(dish, radius * 0.22f, Rgb(0x5a6068));
  p.out.Circle(center, radius, faction.WithAlpha(220), Stroke{1.5f * p.ui});
}

// ---- craft -----------------------------------------------------------------------------------------------------

struct CraftLook {
  bool sprite = false;
  float length = 0.0f; // on screen
  Color body;
};

CraftLook CraftLookFor(const Painter& p, const ViewUnit& squadron)
{
  CraftLook look;
  look.sprite = p.scene.sprites.Has(squadron.typeIndex, p.View());
  // The pack draws fighters several times their true size so they read beside capital ships; either way they keep a
  // minimum size on screen.
  const float minimum = (look.sprite ? kMinCraftSpritePx : kMinCraftPx) * p.ui;
  const float meters = look.sprite && p.scene.sprites.LengthMeters(squadron.typeIndex) > 0.0f
                           ? p.scene.sprites.LengthMeters(squadron.typeIndex)
                           : kCraftLength;
  look.length = std::max(p.Px(meters), minimum);
  look.body = Mix(squadron.side == Side::Alliance ? kAllianceHull : kEmpireHull,
                  FactionColor(p.scene.theme, squadron.side), 0.35f);
  return look;
}

void PaintCraftShape(const Painter& p, const ViewUnit& squadron, const CraftLook& look, Point at, float angle,
                     Color color, float heading)
{
  if (look.sprite) {
    p.out.Sprite({squadron.typeIndex, p.View(), SpriteLayer::Color, at, heading, look.length, color});
    return;
  }
  const Silhouette& silhouette = SilhouetteFor(LookFor(squadron.typeIndex, squadron.kind).shape);
  for (const std::vector<Point>& part : silhouette.fills) {
    std::vector<Point> points;
    for (const Point& h : part) {
      points.push_back(SymbolPoint(at, angle, look.length * 0.5f, h));
    }
    p.out.FillConvex(std::move(points), color);
  }
}

// Top-down, height shows as a shadow offset down and to the right; angled, the shadow lies on the plane under the
// raised craft.
void PaintCraftShadow(const Painter& p, const ViewUnit& squadron, const CraftLook& look, const battle::ViewCraft& craft)
{
  const Color shadow{0, 0, 0, 90};
  if (p.Angled()) {
    const Point at = p.Screen(craft.position);
    p.out.FillConvex({at + Point{-look.length * 0.45f, 0.0f}, at + Point{0.0f, look.length * 0.2f},
                      at + Point{look.length * 0.45f, 0.0f}, at + Point{0.0f, -look.length * 0.2f}},
                     shadow);
    return;
  }
  const Point offset{look.length * 0.35f + 2.0f * p.ui, look.length * 0.5f + 3.0f * p.ui};
  PaintCraftShape(p, squadron, look, p.Screen(craft.position) + offset, p.camera.ProjectedAngle(craft.heading), shadow,
                  p.camera.ScreenHeading(craft.heading));
}

void PaintCraftBody(const Painter& p, const ViewUnit& squadron, const CraftLook& look, const battle::ViewCraft& craft)
{
  PaintCraftShape(p, squadron, look, p.Screen(craft.position, p.CraftHeight()), p.camera.ProjectedAngle(craft.heading),
                  look.sprite ? Color{} : look.body, p.camera.ScreenHeading(craft.heading));
}

// The tactical look marks a squadron as one symbol; in the angled view it floats at flight height on a stem from its
// spot on the plane.
void PaintSquadronSymbol(const Painter& p, const ViewUnit& squadron)
{
  const Point base = p.Screen(squadron.position);
  const Point raised = p.Screen(squadron.position, p.CraftHeight());
  if (p.Angled()) {
    p.out.Line(base, raised, FactionColor(p.scene.theme, squadron.side).WithAlpha(90), Stroke{1.0f * p.ui});
    p.out.FillCircle(base, 1.5f * p.ui, FactionColor(p.scene.theme, squadron.side).WithAlpha(120));
  }
  PaintSymbol(p, raised, p.camera.ProjectedAngle(squadron.heading), squadron.side, TacticalSymbol::Fighter);
}

// In the detailed angled view a flight's height shows on the plane: a faint ring where it is and a stem up to it.
void PaintFlightMarker(const Painter& p, const ViewUnit& squadron)
{
  const Color color = FactionColor(p.scene.theme, squadron.side);
  p.out.Line(p.Screen(squadron.position), p.Screen(squadron.position, p.CraftHeight()), color.WithAlpha(60),
             Stroke{1.0f * p.ui, 3.0f * p.ui, 3.0f * p.ui});
  PlaneCircle(p.out, p.camera, squadron.position, std::max(squadron.radius, p.camera.MetersFor(6.0f * p.ui)),
              color.WithAlpha(50), Stroke{1.0f * p.ui});
}

// ---- drawing order ---------------------------------------------------------------------------------------------

struct Drawable {
  float depth = 0.0f; // screen y of the spot on the plane: larger is nearer the camera in the angled view
  const ViewUnit* unit = nullptr;
  const battle::ViewCraft* craft = nullptr;
};

void PaintDrawable(const Painter& p, const Drawable& item, const CraftLook* craftLook)
{
  if (item.craft) {
    PaintCraftBody(p, *item.unit, *craftLook, *item.craft);
  } else if (item.unit->kind == ViewUnitKind::DeathStar) {
    PaintDeathStar(p, *item.unit);
  } else {
    PaintCapitalShip(p, *item.unit);
  }
}

// Top-down: ships in unit order, then the craft's shadows over them, then the craft. Angled: the craft's shadows on the
// plane first, then ships and craft together from the farthest to the nearest.
void PaintUnits(const Painter& p)
{
  const battle::ViewModel& view = p.scene.view;
  std::vector<CraftLook> craftLooks(view.units.size());
  std::vector<Drawable> ships;
  std::vector<Drawable> craft;
  for (size_t i = 0; i < view.units.size(); ++i) {
    const ViewUnit& unit = view.units[i];
    if (!Selectable(unit)) {
      continue;
    }
    if (unit.kind != ViewUnitKind::Squadron) {
      ships.push_back({p.Screen(unit.position).y, &unit, nullptr});
    } else if (!p.detailed) {
      PaintSquadronSymbol(p, unit);
    } else {
      craftLooks[i] = CraftLookFor(p, unit);
      if (p.Angled()) {
        PaintFlightMarker(p, unit);
      }
    }
  }
  if (p.detailed) {
    for (const battle::ViewCraft& one : view.craft) {
      const auto owner = std::ranges::find(view.units, one.squadron, &ViewUnit::unit);
      if (owner != view.units.end() && Selectable(*owner) && owner->kind == ViewUnitKind::Squadron) {
        craft.push_back({p.Screen(one.position).y, &*owner, &one});
      }
    }
  }
  const auto lookOf = [&](const Drawable& item) {
    return &craftLooks[static_cast<size_t>(item.unit - view.units.data())];
  };
  const auto shadows = [&] {
    for (const Drawable& item : craft) {
      PaintCraftShadow(p, *item.unit, *lookOf(item), *item.craft);
    }
  };
  if (!p.Angled()) {
    for (const Drawable& item : ships) {
      PaintDrawable(p, item, nullptr);
    }
    shadows();
    for (const Drawable& item : craft) {
      PaintDrawable(p, item, lookOf(item));
    }
    return;
  }
  shadows();
  std::vector<Drawable> all = std::move(ships);
  all.insert(all.end(), craft.begin(), craft.end());
  std::ranges::stable_sort(all, {}, &Drawable::depth);
  for (const Drawable& item : all) {
    PaintDrawable(p, item, item.craft ? lookOf(item) : nullptr);
  }
}

// ---- markers ---------------------------------------------------------------------------------------------------

// How far below its position a unit's bars and name go: under its drawing, and in the angled view under its ring.
float MarkerDropPx(const Painter& p, const ViewUnit& unit)
{
  const float drop = DrawnRadiusPx(p, unit) * p.camera.VerticalScale() + (p.Angled() ? 6.0f * p.ui : 0.0f);
  return std::max(drop, 8.0f * p.ui);
}

void PaintBars(const Painter& p, const ViewUnit& unit)
{
  const Theme& theme = p.scene.theme;
  const float width = std::clamp(DrawnRadiusPx(p, unit) * 1.5f, 26.0f * p.ui, 70.0f * p.ui);
  const float height = 3.0f * p.ui;
  const Point center = p.Screen(unit.position);
  float y = center.y + MarkerDropPx(p, unit) + 5.0f * p.ui;
  const float x = center.x - width * 0.5f;
  p.out.FillRect({x - 1.0f, y - 1.0f, width + 2.0f, height * 2.0f + 3.0f}, Rgb(0x000000, 150));
  p.out.FillRect({x, y, width * std::clamp(unit.shieldFraction, 0.0f, 1.0f), height}, theme.shield);
  y += height + 1.0f;
  p.out.FillRect({x, y, width * std::clamp(unit.hullFraction, 0.0f, 1.0f), height},
                 HullColor(theme, unit.hullFraction));
}

Rect LabelRect(const Painter& p, const ViewUnit& unit, const std::string& text, bool bars)
{
  const float size = kLabelSize * p.ui;
  const TextMeasurer* measure = p.scene.text;
  const float width = measure ? measure->Width(text, size) : 0.56f * size * static_cast<float>(text.size());
  const float height = measure ? measure->LineHeight(size) : 1.3f * size;
  const Point center = p.Screen(unit.position);
  return {center.x - width * 0.5f, center.y + MarkerDropPx(p, unit) + (bars ? 13.0f : 4.0f) * p.ui, width, height};
}

struct LabelCandidate {
  size_t index = 0; // in the view's unit list
  int rank = 0;     // 0 hovered, 1 selected, 2 group leader, 3 any other
  float sizePx = 0.0f;
  bool bars = false;
};

// Higher-ranked and then larger units claim their space first; a label that would overlap one already placed is
// dropped, except the hovered unit's.
void PaintLabels(const Painter& p, std::vector<LabelCandidate> candidates)
{
  std::ranges::stable_sort(candidates, [](const LabelCandidate& a, const LabelCandidate& b) {
    return a.rank != b.rank ? a.rank < b.rank : a.sizePx > b.sizePx;
  });
  const float gap = 2.0f * p.ui;
  std::vector<Rect> placed;
  for (const LabelCandidate& candidate : candidates) {
    const ViewUnit& unit = p.scene.view.units[candidate.index];
    const std::string& name = p.scene.names[static_cast<size_t>(unit.unit)];
    const Rect rect = LabelRect(p, unit, name, candidate.bars);
    const Rect padded = rect.Inset(-gap);
    const bool clear = std::ranges::none_of(placed, [&](const Rect& other) {
      return !Intersection(padded, other).Empty();
    });
    if (!clear && candidate.rank > 0) {
      continue;
    }
    placed.push_back(rect);
    // A shadow keeps the faction-coloured name readable over art tinted the same colour.
    const float shadow = std::max(1.0f, p.ui);
    p.out.Text(name, {rect.Center().x + shadow, rect.y + shadow}, kLabelSize * p.ui, Rgb(0x000000, 200), Align::Center,
               VAlign::Top);
    p.out.Text(name, {rect.Center().x, rect.y}, kLabelSize * p.ui,
               FactionColor(p.scene.theme, unit.side).WithAlpha(220), Align::Center, VAlign::Top);
  }
}

// Top-down: corner brackets around the unit. Angled: a ring on the plane under it, drawn before the ships.
void PaintSelectionMarker(const Painter& p, const ViewUnit& unit, bool hovered)
{
  const bool own = unit.side == p.scene.playerSide;
  const Color color = own ? kSelectOwn : kSelectOther;
  const bool selected = p.scene.selection.Contains(unit.unit);
  if (p.Angled()) {
    const float radius = p.camera.MetersFor(std::max(DrawnRadiusPx(p, unit), 9.0f * p.ui) + 5.0f * p.ui);
    PlaneCircle(p.out, p.camera, unit.position, radius, color.WithAlpha(selected ? 220 : 110),
                Stroke{(selected ? 1.8f : 1.0f) * p.ui});
    if (selected) {
      PlaneDisc(p.out, p.camera, unit.position, radius, color.WithAlpha(18));
    }
    return;
  }
  const Point center = p.Screen(unit.position);
  const float r = std::max(DrawnRadiusPx(p, unit), 9.0f * p.ui) + 5.0f * p.ui;
  if (hovered && !selected) {
    p.out.Circle(center, r, color.WithAlpha(110), Stroke{1.0f * p.ui});
    return;
  }
  const float arm = std::max(4.0f * p.ui, r * 0.35f);
  const Stroke stroke{1.5f * p.ui};
  for (const Point corner : {Point{-1, -1}, Point{1, -1}, Point{1, 1}, Point{-1, 1}}) {
    const Point c = center + Point{corner.x * r, corner.y * r};
    p.out.Polyline({c + Point{-corner.x * arm, 0.0f}, c, c + Point{0.0f, -corner.y * arm}}, color, stroke);
  }
}

void PaintStatus(const Painter& p, const ViewUnit& unit)
{
  const Point center = p.Screen(unit.position);
  const float r = std::max(DrawnRadiusPx(p, unit), 9.0f * p.ui) + 9.0f * p.ui;
  if (unit.flags & battle::kViewWithdrawing) {
    p.out.Circle(center, r, Rgb(0xffffff, 40), Stroke{2.0f * p.ui});
    p.out.Arc(center, r, kPi * 0.5f, -kTwoPi * std::clamp(unit.spool, 0.0f, 1.0f), Rgb(0xe8f4ff, 220),
              Stroke{2.0f * p.ui});
  }
  if (unit.flags & battle::kViewFlagship) {
    const float barWidth = std::clamp(DrawnRadiusPx(p, unit) * 1.5f, 26.0f * p.ui, 70.0f * p.ui);
    const Point badge{center.x - barWidth * 0.5f - 7.0f * p.ui, center.y + MarkerDropPx(p, unit) + 8.0f * p.ui};
    const float s = 3.5f * p.ui;
    p.out.FillConvex({badge + Point{0, -s}, badge + Point{s, 0}, badge + Point{0, s}, badge + Point{-s, 0}},
                     Rgb(0xffd860, 230));
  }
}

// Labels stay readable in a crowd: zoomed out, or with many ships on screen, only the hovered unit, a small
// selection and each group's leader (its first active member) are named.
struct LabelPolicy {
  bool crowded = false;
  bool smallSelection = true;
  std::vector<bool> leader; // by position in the view's unit list
};

LabelPolicy MakeLabelPolicy(const Painter& p)
{
  const battle::ViewModel& view = p.scene.view;
  LabelPolicy policy;
  policy.smallSelection = p.scene.selection.Units().size() <= kLabelledSelection;
  size_t onScreen = 0;
  for (const ViewUnit& unit : view.units) {
    onScreen += Selectable(unit) && unit.kind != ViewUnitKind::Squadron
                && p.camera.Viewport().Contains(p.Screen(unit.position));
  }
  policy.crowded = p.camera.Look() != ZoomLook::Detailed || onScreen > kCrowd;
  policy.leader.assign(view.units.size(), false);
  for (const battle::ViewGroup& group : view.groups) {
    for (const battle::UnitIndex member : group.members) {
      const auto found = std::ranges::find(view.units, member, &ViewUnit::unit);
      if (found != view.units.end() && Selectable(*found)) {
        policy.leader[static_cast<size_t>(found - view.units.begin())] = true;
        break;
      }
    }
  }
  return policy;
}

void PaintUnitMarkers(const Painter& p)
{
  const battle::ViewModel& view = p.scene.view;
  const LabelPolicy labels = MakeLabelPolicy(p);
  const bool close = p.camera.Look() == ZoomLook::Detailed;
  std::vector<LabelCandidate> candidates;
  for (size_t i = 0; i < view.units.size(); ++i) {
    const ViewUnit& unit = view.units[i];
    if (!Selectable(unit)) {
      continue;
    }
    const bool selected = p.scene.selection.Contains(unit.unit);
    const bool hovered = p.scene.hovered == unit.unit;
    if ((selected || hovered) && !p.Angled()) {
      PaintSelectionMarker(p, unit, hovered);
    }
    PaintStatus(p, unit);
    // Bars and names follow the zoom, not the look: pausing switches to the tactical look without hiding them.
    const bool capital = unit.kind != ViewUnitKind::Squadron;
    const bool showBars = capital && (close || selected || hovered);
    if (showBars) {
      PaintBars(p, unit);
    }
    const bool named = unit.unit >= 0 && unit.unit < static_cast<battle::UnitIndex>(p.scene.names.size());
    const bool keyUnit = hovered || (selected && labels.smallSelection) || labels.leader[i];
    const bool labelled = labels.crowded ? keyUnit : keyUnit || selected || DrawnRadiusPx(p, unit) >= 14.0f * p.ui;
    if (named && capital && labelled) {
      const int rank = hovered ? 0 : selected ? 1 : labels.leader[i] ? 2 : 3;
      candidates.push_back({i, rank, DrawnRadiusPx(p, unit), showBars});
    }
    if (unit.kind == ViewUnitKind::Squadron) {
      const Point center = p.Screen(unit.position, p.CraftHeight());
      const float offset = p.detailed ? std::max(p.Px(unit.radius), 10.0f * p.ui) : 9.0f * p.ui;
      p.out.Text(std::to_string(unit.craft), {center.x + offset, center.y - offset}, 10.0f * p.ui,
                 FactionColor(p.scene.theme, unit.side).WithAlpha(220), Align::Start, VAlign::Bottom);
    }
  }
  PaintLabels(p, std::move(candidates));
}

// Marks on the plane under the ships in the angled view: selection and hover rings.
void PaintPlaneMarkers(const Painter& p)
{
  for (const ViewUnit& unit : p.scene.view.units) {
    if (Selectable(unit) && (p.scene.selection.Contains(unit.unit) || p.scene.hovered == unit.unit)) {
      PaintSelectionMarker(p, unit, p.scene.hovered == unit.unit);
    }
  }
}

} // namespace

float DrawnLengthMeters(const Painter& p, const ViewUnit& unit)
{
  const float art = p.scene.sprites.Has(unit.typeIndex, p.View()) ? p.scene.sprites.LengthMeters(unit.typeIndex) : 0.0f;
  return art > 0.0f ? art : unit.radius * 2.0f;
}

float DrawnRadiusPx(const Painter& p, const ViewUnit& unit)
{
  const float radius = p.Px(unit.radius);
  if (unit.kind == ViewUnitKind::Squadron) {
    return radius;
  }
  return std::max(radius, p.Px(DrawnLengthMeters(p, unit)) * 0.5f);
}

} // namespace painter

Color FactionColor(const Theme& theme, battle::Side side)
{
  return side == battle::Side::Alliance ? theme.alliance : theme.empire;
}

Color BoltColor(const Theme& theme, battle::Side side, battle::ViewShotKind kind)
{
  if (kind == battle::ViewShotKind::Ion) {
    return theme.ionBolt;
  }
  return side == battle::Side::Alliance ? theme.allianceBolt : theme.empireBolt;
}

void PaintWorld(DrawList& out, const WorldScene& scene)
{
  using namespace painter;
  // §10.4: the overlay is drawn in the tactical look, in either view.
  const bool detailed = scene.camera.Look() == ZoomLook::Detailed && !scene.overlay;
  const Painter p{out, scene, scene.camera, scene.camera.UiScale(), detailed};
  out.PushClip(scene.camera.Viewport());
  PaintBackdrop(p);
  PaintGrid(p);
  PaintPlanetaryCover(p);
  PaintWells(p);
  if (scene.overlay) {
    PaintArcsAndRanges(p);
  }
  if (p.Angled()) {
    PaintPlaneMarkers(p);
    if (scene.overlay) {
      PaintTargetLines(p);
    }
  }
  PaintTractors(p);
  PaintUnits(p);
  PaintEmplacements(p);
  PaintShots(p);
  PaintEffects(p);
  if (scene.overlay && !p.Angled()) {
    PaintTargetLines(p);
  }
  PaintUnitMarkers(p);
  PaintZoneLabels(p);
  out.PopClip();
}

} // namespace reb::battle_ui
