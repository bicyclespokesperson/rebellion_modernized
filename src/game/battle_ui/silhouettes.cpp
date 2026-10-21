#include "battle_ui/silhouettes.h"

#include <array>

namespace reb::battle_ui
{
namespace
{

using Points = std::vector<Point>;

// Completes a perimeter from its port half, listed bow to stern, by mirroring it across the centreline.
Points Mirrored(const Points& half)
{
  Points out = half;
  for (auto it = half.rbegin(); it != half.rend(); ++it) {
    if (it->y != 0.0f) {
      out.push_back({it->x, -it->y});
    }
  }
  return out;
}

Points Box(float x0, float x1, float halfWidth, float yOffset = 0.0f)
{
  return {{x1, yOffset + halfWidth}, {x0, yOffset + halfWidth}, {x0, yOffset - halfWidth}, {x1, yOffset - halfWidth}};
}

Points Ellipse(Point center, float rx, float ry, int segments = 12)
{
  Points out;
  for (int i = 0; i < segments; ++i) {
    const float a = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
    out.push_back({center.x + rx * std::cos(a), center.y + ry * std::sin(a)});
  }
  return out;
}

Points Closed(Points points)
{
  points.push_back(points.front());
  return points;
}

Silhouette Wedge(float halfWidth, float bridgeWidth)
{
  Silhouette s;
  s.outline = Mirrored({{1.0f, 0.0f}, {-0.78f, halfWidth}, {-1.0f, halfWidth * 0.87f}});
  s.fills = {s.outline};
  s.details = {{{0.88f, 0.0f}, {-0.5f, 0.0f}},
               {{0.55f, halfWidth * 0.1f}, {-0.7f, halfWidth * 0.72f}},
               {{0.55f, -halfWidth * 0.1f}, {-0.7f, -halfWidth * 0.72f}},
               Closed(Box(-0.82f, -0.5f, bridgeWidth))};
  s.engines = {{-1.0f, halfWidth * 0.45f}, {-1.0f, 0.0f}, {-1.0f, -halfWidth * 0.45f}};
  return s;
}

Silhouette Interdictor()
{
  Silhouette s = Wedge(0.55f, 0.13f);
  for (const Point dome : {Point{-0.3f, 0.26f}, Point{-0.3f, -0.26f}, Point{-0.62f, 0.36f}, Point{-0.62f, -0.36f}}) {
    s.details.push_back(Closed(Ellipse(dome, 0.1f, 0.1f, 10)));
  }
  return s;
}

Silhouette MonCalamari()
{
  Silhouette s;
  for (int i = 0; i < 28; ++i) {
    const float t = kTwoPi * static_cast<float>(i) / 28.0f;
    const float x = std::cos(t);
    s.outline.push_back({x, 0.34f * std::sin(t) * (0.82f + 0.18f * x)});
  }
  s.fills = {s.outline};
  for (const float x : {0.55f, 0.1f, -0.4f}) {
    const float w = 0.34f * std::sqrt(std::max(0.0f, 1.0f - x * x)) * (0.82f + 0.18f * x) * 0.85f;
    s.details.push_back({{x + 0.06f, w}, {x - 0.04f, 0.0f}, {x + 0.06f, -w}});
  }
  s.details.push_back({{0.85f, 0.0f}, {-0.8f, 0.0f}});
  s.engines = {{-0.98f, 0.09f}, {-0.98f, -0.09f}};
  return s;
}

Silhouette LongHull()
{
  Silhouette s;
  s.outline = Mirrored({{1.0f, 0.0f}, {0.8f, 0.16f}, {0.2f, 0.19f}, {-0.85f, 0.21f}, {-1.0f, 0.14f}});
  s.fills = {s.outline};
  s.details = {{{0.85f, 0.0f}, {-0.9f, 0.0f}},
               {{0.35f, 0.17f}, {0.35f, -0.17f}},
               {{-0.3f, 0.2f}, {-0.3f, -0.2f}},
               Closed(Box(-0.72f, -0.5f, 0.08f))};
  s.engines = {{-1.0f, 0.07f}, {-1.0f, -0.07f}};
  return s;
}

Silhouette Corvette()
{
  Silhouette s;
  s.fills = {{{1.0f, -0.2f}, {1.0f, 0.2f}, {0.55f, 0.25f}, {0.55f, -0.25f}},
             Box(-0.46f, 0.56f, 0.09f),
             {{-0.45f, 0.3f}, {-1.0f, 0.33f}, {-1.0f, -0.33f}, {-0.45f, -0.3f}}};
  s.outline = Mirrored({{1.0f, 0.2f}, {0.55f, 0.25f}, {0.55f, 0.09f}, {-0.45f, 0.09f}, {-0.45f, 0.3f}, {-1.0f, 0.33f}});
  s.details = {{{0.95f, 0.0f}, {0.6f, 0.0f}}, {{-0.5f, 0.0f}, {-0.95f, 0.0f}}};
  s.engines = {{-1.0f, 0.24f}, {-1.0f, 0.08f}, {-1.0f, -0.08f}, {-1.0f, -0.24f}};
  return s;
}

Silhouette Nebulon()
{
  Silhouette s;
  s.fills = {{{1.0f, -0.1f}, {1.0f, 0.1f}, {0.85f, 0.2f}, {0.15f, 0.2f}, {0.15f, -0.2f}, {0.85f, -0.2f}},
             Box(-0.56f, 0.16f, 0.045f),
             Box(-1.0f, -0.55f, 0.18f)};
  s.outline = {{1.0f, -0.1f},     {1.0f, 0.1f},     {0.85f, 0.2f},  {0.15f, 0.2f},   {0.15f, 0.045f},
               {-0.55f, 0.045f},  {-0.55f, 0.18f},  {-1.0f, 0.18f}, {-1.0f, -0.18f}, {-0.55f, -0.18f},
               {-0.55f, -0.045f}, {0.15f, -0.045f}, {0.15f, -0.2f}, {0.85f, -0.2f}};
  s.details = {{{0.9f, 0.0f}, {0.25f, 0.0f}}, Closed(Box(0.3f, 0.55f, 0.1f))};
  s.engines = {{-1.0f, 0.1f}, {-1.0f, -0.1f}};
  return s;
}

Silhouette Carrier()
{
  Silhouette s;
  s.outline = Mirrored({{1.0f, 0.12f}, {0.75f, 0.3f}, {-1.0f, 0.3f}});
  s.fills = {s.outline};
  s.details
      = {{{0.7f, 0.0f}, {-0.9f, 0.0f}}, Closed(Box(-0.85f, 0.6f, 0.17f)), Closed(Box(-0.8f, -0.55f, 0.32f, 0.0f))};
  s.engines = {{-1.0f, 0.18f}, {-1.0f, 0.0f}, {-1.0f, -0.18f}};
  return s;
}

Silhouette Picket()
{
  Silhouette s;
  s.outline = Mirrored({{1.0f, 0.0f}, {0.62f, 0.11f}, {-0.8f, 0.12f}, {-1.0f, 0.08f}});
  s.fills = {s.outline, Box(0.3f, 0.55f, 0.17f), Box(-0.55f, -0.3f, 0.2f)};
  s.details = {{{0.9f, 0.0f}, {-0.9f, 0.0f}}};
  s.engines = {{-1.0f, 0.04f}, {-1.0f, -0.04f}};
  return s;
}

Silhouette Transport()
{
  Silhouette s;
  s.outline = Mirrored({{1.0f, 0.0f}, {1.0f, 0.3f}, {0.85f, 0.42f}, {-0.85f, 0.42f}, {-1.0f, 0.3f}});
  s.outline.erase(s.outline.begin());
  s.fills = {s.outline};
  s.details = {{{0.6f, 0.4f}, {0.6f, -0.4f}}, {{0.0f, 0.4f}, {0.0f, -0.4f}}, {{-0.6f, 0.4f}, {-0.6f, -0.4f}}};
  s.engines = {{-1.0f, 0.2f}, {-1.0f, -0.2f}};
  return s;
}

Silhouette Fighter(HullShape shape)
{
  Silhouette s;
  switch (shape) {
    case HullShape::XWing:
      s.fills = {{{1.0f, 0.0f}, {-0.8f, 0.12f}, {-0.8f, -0.12f}}, Box(-0.65f, -0.2f, 0.85f)};
      break;
    case HullShape::YWing:
      s.fills = {{{1.0f, 0.0f}, {0.6f, 0.2f}, {0.3f, 0.2f}, {0.3f, -0.2f}, {0.6f, -0.2f}},
                 Box(-1.0f, 0.4f, 0.06f),
                 Box(-1.0f, 0.1f, 0.1f, 0.55f),
                 Box(-1.0f, 0.1f, 0.1f, -0.55f),
                 Box(-0.2f, 0.0f, 0.55f)};
      break;
    case HullShape::AWing:
      s.fills = {Mirrored({{1.0f, 0.0f}, {-0.75f, 0.7f}, {-1.0f, 0.6f}})};
      break;
    case HullShape::BWing:
      s.fills = {Box(-0.6f, 1.0f, 0.12f), Box(-0.4f, -0.1f, 1.0f), Box(-0.5f, 0.0f, 0.15f, 0.9f)};
      break;
    case HullShape::TieInterceptor:
      s.fills = {Ellipse({0.0f, 0.0f}, 0.35f, 0.35f, 8),
                 Box(-0.08f, 0.08f, 0.7f),
                 // the bent wings, each as two convex halves
                 {{0.7f, 0.95f}, {0.0f, 0.72f}, {0.0f, 0.82f}},
                 {{0.0f, 0.72f}, {-0.7f, 0.95f}, {0.0f, 0.82f}},
                 {{0.7f, -0.95f}, {0.0f, -0.82f}, {0.0f, -0.72f}},
                 {{0.0f, -0.82f}, {-0.7f, -0.95f}, {0.0f, -0.72f}}};
      break;
    case HullShape::TieBomber:
      s.fills = {Ellipse({0.15f, 0.25f}, 0.55f, 0.2f, 10), Ellipse({0.15f, -0.25f}, 0.55f, 0.2f, 10),
                 Box(-0.6f, 0.6f, 0.06f, 0.82f), Box(-0.6f, 0.6f, 0.06f, -0.82f), Box(-0.05f, 0.05f, 0.8f)};
      break;
    case HullShape::TieDefender:
      s.fills = {Ellipse({0.0f, 0.0f}, 0.4f, 0.4f, 8), Box(-0.08f, 0.08f, 0.9f), Box(-0.6f, 0.6f, 0.07f, 0.9f),
                 Box(-0.6f, 0.6f, 0.07f, -0.9f), Box(-0.9f, -0.1f, 0.07f)};
      break;
    default: // TIE Fighter
      s.fills = {Ellipse({0.0f, 0.0f}, 0.38f, 0.38f, 8), Box(-0.07f, 0.07f, 0.75f), Box(-0.7f, 0.7f, 0.07f, 0.82f),
                 Box(-0.7f, 0.7f, 0.07f, -0.82f)};
      break;
  }
  return s;
}

Silhouette DeathStar()
{
  Silhouette s;
  s.outline = Ellipse({0.0f, 0.0f}, 1.0f, 1.0f, 48);
  s.fills = {s.outline};
  s.details = {Closed(Ellipse({0.35f, 0.4f}, 0.22f, 0.22f, 20)), {{-1.0f, 0.0f}, {1.0f, 0.0f}}};
  return s;
}

} // namespace

ClassLook LookFor(int32_t typeIndex, battle::ViewUnitKind kind)
{
  using H = HullShape;
  using T = TacticalSymbol;
  static constexpr std::array<ClassLook, 38> kLooks{{
      {H::MonCalamari, T::Heavy},       // 0 Mon Calamari Cruiser
      {H::LongHull, T::Line},           // 1 Bulk Cruiser
      {H::LongHull, T::Line},           // 2 Assault Frigate
      {H::Nebulon, T::Escort},          // 3 Nebulon-B
      {H::Carrier, T::Carrier},         // 4 Alliance Escort Carrier
      {H::Corvette, T::Picket},         // 5 Corellian Corvette
      {H::Transport, T::Transport},     // 6 Medium Transport
      {H::Transport, T::Transport},     // 7 Bulk Transport
      {H::Corvette, T::Picket},         // 8 Corellian Gunship
      {H::LongHull, T::Line},           // 9 Alliance Dreadnaught
      {H::Interdictor, T::Interdictor}, // 10 CC-7700
      {H::LongHull, T::Line},           // 11 CC-9600
      {H::Wedge, T::Heavy},             // 12 Bulwark
      {H::MonCalamari, T::Heavy},       // 13 Liberator
      {H::MonCalamari, T::Heavy},       // 14 Dauntless
      {H::Wedge, T::Line},              // 15 Strike Cruiser
      {H::Picket, T::Picket},           // 16 Lancer
      {H::Interdictor, T::Interdictor}, // 17 Interdictor
      {H::Picket, T::Picket},           // 18 Carrack
      {H::Wedge, T::Line},              // 19 Victory
      {H::Wedge, T::Heavy},             // 20 Imperial Star Destroyer
      {H::SlimWedge, T::Heavy},         // 21 Super Star Destroyer
      {H::Transport, T::Transport},     // 22 Assault Transport
      {H::Transport, T::Transport},     // 23 Galleon
      {H::Wedge, T::Line},              // 24 Victory II
      {H::Wedge, T::Heavy},             // 25 Imperial II
      {H::Transport, T::Transport},     // 26 Star Galleon
      {H::Carrier, T::Carrier},         // 27 Imperial Escort Carrier
      {H::LongHull, T::Line},           // 28 Imperial Dreadnaught
      {H::XWing, T::Fighter},           // 29
      {H::YWing, T::Fighter},           // 30
      {H::AWing, T::Fighter},           // 31
      {H::BWing, T::Fighter},           // 32
      {H::TieFighter, T::Fighter},      // 33
      {H::TieInterceptor, T::Fighter},  // 34
      {H::TieBomber, T::Fighter},       // 35
      {H::TieDefender, T::Fighter},     // 36
      {H::DeathStar, T::DeathStar},     // 37
  }};
  if (typeIndex >= 0 && typeIndex < static_cast<int32_t>(kLooks.size())) {
    return kLooks[typeIndex];
  }
  switch (kind) {
    case battle::ViewUnitKind::Squadron:
      return {H::XWing, T::Fighter};
    case battle::ViewUnitKind::DeathStar:
      return {H::DeathStar, T::DeathStar};
    case battle::ViewUnitKind::CapitalShip:
      break;
  }
  return {H::LongHull, T::Line};
}

const Silhouette& SilhouetteFor(HullShape shape)
{
  static const std::array<Silhouette, 19> kShapes{Wedge(0.6f, 0.14f),
                                                  Wedge(0.2f, 0.06f),
                                                  Interdictor(),
                                                  MonCalamari(),
                                                  LongHull(),
                                                  Corvette(),
                                                  Nebulon(),
                                                  Carrier(),
                                                  Picket(),
                                                  Transport(),
                                                  Fighter(HullShape::XWing),
                                                  Fighter(HullShape::YWing),
                                                  Fighter(HullShape::AWing),
                                                  Fighter(HullShape::BWing),
                                                  Fighter(HullShape::TieFighter),
                                                  Fighter(HullShape::TieInterceptor),
                                                  Fighter(HullShape::TieBomber),
                                                  Fighter(HullShape::TieDefender),
                                                  DeathStar()};
  return kShapes[static_cast<size_t>(shape)];
}

} // namespace reb::battle_ui
