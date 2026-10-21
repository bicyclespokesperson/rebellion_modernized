#include "battle_ui/hud_icons.h"

#include <cmath>
#include <vector>

namespace reb::battle_ui
{
namespace
{

// Icon space: [-1, 1] on both axes, y down, mapped onto the box.
struct Pen {
  DrawList& draw;
  Point center;
  float half;
  Color color;
  Stroke stroke;

  Point P(float x, float y) const
  {
    return {center.x + x * half, center.y + y * half};
  }
  void Line(float x0, float y0, float x1, float y1) const
  {
    draw.Line(P(x0, y0), P(x1, y1), color, stroke);
  }
  void Path(std::initializer_list<Point> points, bool closed = false) const
  {
    std::vector<Point> mapped;
    for (Point p : points) {
      mapped.push_back(P(p.x, p.y));
    }
    draw.Polyline(std::move(mapped), color, stroke, closed);
  }
  void Fill(std::initializer_list<Point> points) const
  {
    std::vector<Point> mapped;
    for (Point p : points) {
      mapped.push_back(P(p.x, p.y));
    }
    draw.FillConvex(std::move(mapped), color);
  }
  void Circle(float x, float y, float r) const
  {
    draw.Circle(P(x, y), r * half, color, stroke);
  }
  void Dot(float x, float y, float r) const
  {
    draw.FillCircle(P(x, y), r * half, color);
  }
  void Arc(float x, float y, float r, float start, float sweep) const
  {
    draw.Arc(P(x, y), r * half, start, sweep, color, stroke);
  }
  // An arrowhead at (x, y) pointing along (dx, dy).
  void Head(float x, float y, float dx, float dy, float size = 0.38f) const
  {
    const float length = std::hypot(dx, dy);
    dx /= length;
    dy /= length;
    Fill({{x, y},
          {x - dx * size - dy * size * 0.6f, y - dy * size + dx * size * 0.6f},
          {x - dx * size + dy * size * 0.6f, y - dy * size - dx * size * 0.6f}});
  }
};

void DrawWithdraw(const Pen& p)
{
  p.Line(-0.15f, 0.0f, 0.6f, 0.0f);
  p.Head(0.9f, 0.0f, 1.0f, 0.0f, 0.42f);
  p.Line(-0.9f, -0.45f, -0.25f, -0.45f);
  p.Line(-0.9f, 0.45f, -0.25f, 0.45f);
  p.Line(-0.7f, 0.0f, -0.4f, 0.0f);
}

void DrawShield(const Pen& p)
{
  p.Path({{0.0f, -0.85f}, {0.7f, -0.55f}, {0.6f, 0.25f}, {0.0f, 0.85f}, {-0.6f, 0.25f}, {-0.7f, -0.55f}}, true);
}

void DrawCrosshair(const Pen& p, float ring)
{
  p.Circle(0.0f, 0.0f, ring);
  p.Line(0.0f, -0.95f, 0.0f, -ring * 0.45f);
  p.Line(0.0f, 0.95f, 0.0f, ring * 0.45f);
  p.Line(-0.95f, 0.0f, -ring * 0.45f, 0.0f);
  p.Line(0.95f, 0.0f, ring * 0.45f, 0.0f);
}

} // namespace

Icon FieldIcon(DoctrineField field)
{
  switch (field) {
    case DoctrineField::Role:
      return Icon::Role;
    case DoctrineField::Range:
      return Icon::Range;
    case DoctrineField::Approach:
      return Icon::Approach;
    case DoctrineField::Facing:
      return Icon::Facing;
    case DoctrineField::Priorities:
      return Icon::Targets;
    case DoctrineField::Allocation:
      return Icon::Allocation;
    case DoctrineField::Subsystem:
      return Icon::Subsystem;
    case DoctrineField::Power:
      return Icon::Power;
    case DoctrineField::Withdraw:
      return Icon::Withdraw;
    case DoctrineField::Formation:
      return Icon::Formation;
    case DoctrineField::Mission:
      return Icon::Mission;
    case DoctrineField::Dogfight:
      return Icon::Dogfight;
    case DoctrineField::Rearm:
      return Icon::Rearm;
    case DoctrineField::Launch:
      return Icon::Launch;
  }
  return Icon::Role;
}

Icon SignalIcon(battle::SignalKind signal)
{
  switch (signal) {
    case battle::SignalKind::ConcentrateFire:
      return Icon::ConcentrateFire;
    case battle::SignalKind::HoldTheLine:
      return Icon::HoldTheLine;
    case battle::SignalKind::Scramble:
      return Icon::Scramble;
    case battle::SignalKind::CoordinatedJump:
      return Icon::CoordinatedJump;
  }
  return Icon::ConcentrateFire;
}

void DrawIcon(DrawList& draw, Icon icon, const Rect& box, Color color, float scale)
{
  const Pen p{draw, box.Center(), std::min(box.w, box.h) * 0.5f, color, Stroke{1.4f * scale}};
  switch (icon) {
    case Icon::Role:
      p.Path({{-0.7f, 0.05f}, {0.0f, -0.6f}, {0.7f, 0.05f}});
      p.Path({{-0.7f, 0.65f}, {0.0f, 0.0f}, {0.7f, 0.65f}});
      break;
    case Icon::Range:
      p.Dot(-0.7f, 0.7f, 0.17f);
      p.Arc(-0.7f, 0.7f, 0.75f, 0.0f, kPi * 0.5f);
      p.Arc(-0.7f, 0.7f, 1.35f, 0.0f, kPi * 0.5f);
      break;
    case Icon::Approach:
      p.Path({{-0.6f, 0.85f},
              {-0.6f, -0.1f},
              {-0.4f, -0.55f},
              {0.0f, -0.72f},
              {0.4f, -0.55f},
              {0.6f, -0.15f},
              {0.6f, 0.2f}});
      p.Head(0.6f, 0.65f, 0.0f, 1.0f);
      break;
    case Icon::Facing:
      p.Path({{0.85f, 0.0f}, {-0.65f, -0.55f}, {-0.4f, 0.0f}, {-0.65f, 0.55f}}, true);
      break;
    case Icon::Targets:
      p.Dot(-0.7f, -0.55f, 0.13f);
      p.Line(-0.4f, -0.55f, 0.85f, -0.55f);
      p.Dot(-0.7f, 0.0f, 0.13f);
      p.Line(-0.4f, 0.0f, 0.55f, 0.0f);
      p.Dot(-0.7f, 0.55f, 0.13f);
      p.Line(-0.4f, 0.55f, 0.25f, 0.55f);
      break;
    case Icon::Allocation:
      p.Line(-0.9f, 0.0f, -0.15f, 0.0f);
      p.Line(-0.15f, 0.0f, 0.6f, -0.6f);
      p.Line(-0.15f, 0.0f, 0.65f, 0.0f);
      p.Line(-0.15f, 0.0f, 0.6f, 0.6f);
      p.Dot(0.7f, -0.68f, 0.15f);
      p.Dot(0.8f, 0.0f, 0.15f);
      p.Dot(0.7f, 0.68f, 0.15f);
      break;
    case Icon::Subsystem:
      p.Path({{-0.45f, -0.45f}, {0.45f, -0.45f}, {0.45f, 0.45f}, {-0.45f, 0.45f}}, true);
      p.Dot(0.0f, 0.0f, 0.16f);
      for (const float t : {-0.2f, 0.2f}) {
        p.Line(t, -0.45f, t, -0.85f);
        p.Line(t, 0.45f, t, 0.85f);
        p.Line(-0.45f, t, -0.85f, t);
        p.Line(0.45f, t, 0.85f, t);
      }
      break;
    case Icon::Power:
      p.Fill({{0.25f, -0.95f}, {-0.55f, 0.15f}, {0.05f, 0.15f}});
      p.Fill({{-0.05f, -0.15f}, {0.55f, -0.15f}, {-0.25f, 0.95f}});
      break;
    case Icon::Withdraw:
    case Icon::Hyperdrive:
      DrawWithdraw(p);
      break;
    case Icon::Formation:
      for (const Point at : {Point{0.45f, 0.0f}, Point{-0.35f, -0.55f}, Point{-0.35f, 0.55f}}) {
        p.Fill({{at.x + 0.35f, at.y}, {at.x - 0.3f, at.y - 0.28f}, {at.x - 0.3f, at.y + 0.28f}});
      }
      break;
    case Icon::Mission:
      p.Line(-0.75f, -0.75f, 0.75f, 0.75f);
      p.Line(-0.75f, 0.75f, 0.75f, -0.75f);
      p.Dot(0.0f, 0.0f, 0.28f);
      break;
    case Icon::Dogfight:
      p.Arc(0.0f, 0.0f, 0.65f, kPi * 0.15f, kPi * 0.7f);
      p.Head(-0.62f, -0.2f, -0.35f, 1.0f, 0.34f);
      p.Arc(0.0f, 0.0f, 0.65f, kPi * 1.15f, kPi * 0.7f);
      p.Head(0.62f, 0.2f, 0.35f, -1.0f, 0.34f);
      break;
    case Icon::Rearm:
      p.Path({{-0.75f, -0.2f}, {0.35f, -0.2f}, {0.75f, 0.1f}, {0.35f, 0.4f}, {-0.75f, 0.4f}}, true);
      p.Line(-0.75f, -0.2f, -0.95f, -0.45f);
      p.Line(-0.75f, 0.4f, -0.95f, 0.65f);
      p.Line(0.45f, -0.95f, 0.45f, -0.45f);
      p.Line(0.2f, -0.7f, 0.7f, -0.7f);
      break;
    case Icon::Launch:
      p.Line(-0.75f, 0.75f, 0.75f, 0.75f);
      p.Line(0.0f, 0.5f, 0.0f, -0.45f);
      p.Head(0.0f, -0.9f, 0.0f, -1.0f, 0.45f);
      break;
    case Icon::Attack:
      DrawCrosshair(p, 0.55f);
      p.Dot(0.0f, 0.0f, 0.14f);
      break;
    case Icon::Move:
      p.Line(-0.65f, 0.65f, 0.4f, -0.4f);
      p.Head(0.75f, -0.75f, 1.0f, -1.0f, 0.5f);
      break;
    case Icon::Hold:
      p.Fill({{-0.55f, -0.55f}, {0.55f, -0.55f}, {0.55f, 0.55f}, {-0.55f, 0.55f}});
      break;
    case Icon::Escort:
      DrawShield(p);
      p.Dot(0.0f, -0.05f, 0.2f);
      break;
    case Icon::Tractor:
      p.Dot(-0.7f, 0.0f, 0.2f);
      p.Line(-0.55f, -0.1f, 0.8f, -0.65f);
      p.Line(-0.55f, 0.1f, 0.8f, 0.65f);
      p.Arc(-0.7f, 0.0f, 1.6f, -kPi * 0.14f, kPi * 0.28f);
      p.Line(0.1f, -0.15f, 0.1f, 0.15f);
      break;
    case Icon::Recall:
      p.Path({{0.6f, 0.75f}, {0.6f, -0.2f}, {0.35f, -0.6f}, {-0.1f, -0.65f}, {-0.4f, -0.4f}, {-0.45f, 0.25f}});
      p.Head(-0.45f, 0.75f, 0.0f, 1.0f, 0.45f);
      break;
    case Icon::Strike:
      p.Line(-0.3f, 0.8f, 0.9f, 0.8f);
      p.Line(-0.85f, -0.75f, 0.1f, 0.3f);
      p.Head(0.4f, 0.6f, 1.0f, 1.0f, 0.48f);
      break;
    case Icon::Recover:
      p.Path({{-0.75f, 0.3f}, {-0.75f, 0.8f}, {0.75f, 0.8f}, {0.75f, 0.3f}});
      p.Line(0.0f, -0.85f, 0.0f, 0.05f);
      p.Head(0.0f, 0.5f, 0.0f, 1.0f, 0.45f);
      break;
    case Icon::DeathStar:
      p.Circle(0.0f, 0.0f, 0.8f);
      p.Line(-0.8f, 0.05f, 0.8f, 0.05f);
      p.Dot(0.3f, -0.35f, 0.22f);
      break;
    case Icon::PlanetaryBattery:
      // A turret on the ground, its barrel raised toward orbit.
      p.Line(-0.9f, 0.7f, 0.9f, 0.7f);
      p.Fill({{-0.55f, 0.7f}, {-0.5f, 0.38f}, {-0.35f, 0.17f}, {0.0f, 0.07f}, {0.35f, 0.17f}, {0.5f, 0.38f},
              {0.55f, 0.7f}});
      p.Line(0.05f, 0.2f, 0.7f, -0.62f);
      p.Dot(0.78f, -0.72f, 0.13f);
      break;
    case Icon::ConcentrateFire:
      p.Circle(0.0f, 0.0f, 0.28f);
      p.Head(0.0f, -0.42f, 0.0f, 1.0f, 0.4f);
      p.Head(0.0f, 0.42f, 0.0f, -1.0f, 0.4f);
      p.Head(-0.42f, 0.0f, 1.0f, 0.0f, 0.4f);
      p.Head(0.42f, 0.0f, -1.0f, 0.0f, 0.4f);
      p.Line(0.0f, -0.95f, 0.0f, -0.75f);
      p.Line(0.0f, 0.95f, 0.0f, 0.75f);
      p.Line(-0.95f, 0.0f, -0.75f, 0.0f);
      p.Line(0.95f, 0.0f, 0.75f, 0.0f);
      break;
    case Icon::HoldTheLine:
      p.Line(-0.6f, 0.95f, -0.6f, -0.9f);
      p.Fill({{-0.6f, -0.85f}, {0.75f, -0.48f}, {-0.6f, -0.1f}});
      p.Line(-0.95f, 0.95f, 0.4f, 0.95f);
      break;
    case Icon::Scramble:
      for (const float x : {-0.6f, 0.0f, 0.6f}) {
        p.Line(x, 0.85f, x, -0.3f);
        p.Head(x, -0.75f, 0.0f, -1.0f, 0.4f);
      }
      break;
    case Icon::CoordinatedJump:
      p.Path({{-0.75f, -0.6f}, {-0.15f, 0.0f}, {-0.75f, 0.6f}});
      p.Path({{0.0f, -0.6f}, {0.6f, 0.0f}, {0.0f, 0.6f}});
      break;
    case Icon::Engines:
      p.Path({{0.0f, -0.45f}, {0.75f, -0.45f}, {0.75f, 0.45f}, {0.0f, 0.45f}}, true);
      p.Fill({{-0.95f, 0.0f}, {-0.1f, -0.35f}, {-0.1f, 0.35f}});
      break;
    case Icon::ShieldGenerator:
      p.Arc(0.0f, 0.45f, 0.8f, 0.0f, kPi);
      p.Line(-0.95f, 0.45f, 0.95f, 0.45f);
      p.Dot(0.0f, 0.2f, 0.2f);
      break;
    case Icon::WeaponGenerator:
      p.Dot(-0.3f, 0.3f, 0.4f);
      p.Line(-0.1f, 0.05f, 0.85f, -0.55f);
      p.Line(0.05f, 0.3f, 0.95f, -0.2f);
      break;
    case Icon::ChevronDown:
      p.Path({{-0.6f, -0.25f}, {0.0f, 0.35f}, {0.6f, -0.25f}});
      break;
    case Icon::ChevronRight:
      p.Path({{-0.25f, -0.6f}, {0.35f, 0.0f}, {-0.25f, 0.6f}});
      break;
    case Icon::ChevronUp:
      p.Path({{-0.6f, 0.3f}, {0.0f, -0.3f}, {0.6f, 0.3f}});
      break;
    case Icon::Close:
      p.Line(-0.55f, -0.55f, 0.55f, 0.55f);
      p.Line(-0.55f, 0.55f, 0.55f, -0.55f);
      break;
    case Icon::Plus:
      p.Line(0.0f, -0.65f, 0.0f, 0.65f);
      p.Line(-0.65f, 0.0f, 0.65f, 0.0f);
      break;
    case Icon::Grip:
      p.Line(-0.6f, -0.45f, 0.6f, -0.45f);
      p.Line(-0.6f, 0.0f, 0.6f, 0.0f);
      p.Line(-0.6f, 0.45f, 0.6f, 0.45f);
      break;
    case Icon::Gear:
      p.Circle(0.0f, 0.0f, 0.42f);
      for (int i = 0; i < 8; ++i) {
        const float angle = static_cast<float>(i) * kPi * 0.25f;
        p.Line(std::cos(angle) * 0.55f, std::sin(angle) * 0.55f, std::cos(angle) * 0.9f, std::sin(angle) * 0.9f);
      }
      break;
    case Icon::Eye:
      p.Path({{-0.95f, 0.0f},
              {-0.5f, -0.45f},
              {0.0f, -0.6f},
              {0.5f, -0.45f},
              {0.95f, 0.0f},
              {0.5f, 0.45f},
              {0.0f, 0.6f},
              {-0.5f, 0.45f}},
             true);
      p.Dot(0.0f, 0.0f, 0.27f);
      break;
    case Icon::FastForward:
      p.Fill({{-0.85f, -0.6f}, {0.0f, 0.0f}, {-0.85f, 0.6f}});
      p.Fill({{0.0f, -0.6f}, {0.85f, 0.0f}, {0.0f, 0.6f}});
      break;
    case Icon::Override:
      p.Fill({{0.0f, -0.75f}, {0.75f, 0.0f}, {0.0f, 0.75f}, {-0.75f, 0.0f}});
      break;
    case Icon::Preset:
      p.Path({{-0.5f, -0.85f}, {0.5f, -0.85f}, {0.5f, 0.85f}, {0.0f, 0.4f}, {-0.5f, 0.85f}}, true);
      break;
  }
}

} // namespace reb::battle_ui
