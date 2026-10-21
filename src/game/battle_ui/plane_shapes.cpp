#include "battle_ui/plane_shapes.h"

#include <vector>

namespace reb::battle_ui
{
namespace
{

constexpr float kSegmentPixels = 5.0f;
constexpr int kMaxSegments = 720;

bool FullTurn(float sweep)
{
  return std::abs(sweep) >= kTwoPi - 1e-4f;
}

int Segments(const Camera& camera, float radius, float sweep)
{
  const int wanted = static_cast<int>(std::ceil(std::abs(sweep) * camera.PixelsFor(radius) / kSegmentPixels));
  return std::clamp(wanted, 12, kMaxSegments);
}

// Points along a world arc, projected; `count` points over the sweep (the end point repeated only when not a turn).
std::vector<Point> ArcPoints(const Camera& camera, Vec2 center, float radius, float start, float sweep, int segments)
{
  const bool full = FullTurn(sweep);
  std::vector<Point> points;
  points.reserve(static_cast<size_t>(segments) + 1);
  for (int k = 0; k <= segments - (full ? 1 : 0); ++k) {
    const float angle = start + sweep * static_cast<float>(k) / static_cast<float>(segments);
    points.push_back(camera.WorldToScreen({center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius}));
  }
  return points;
}

} // namespace

void PlaneArc(DrawList& out, const Camera& camera, Vec2 center, float radius, float start, float sweep, Color color,
              const Stroke& stroke, Blend blend)
{
  if (radius <= 0.0f) {
    return;
  }
  if (camera.View() == CameraView::TopDown) {
    out.Arc(camera.WorldToScreen(center), camera.PixelsFor(radius), camera.ScreenHeading(start), sweep, color, stroke,
            blend);
    return;
  }
  const float turn = FullTurn(sweep) ? kTwoPi : sweep;
  out.Polyline(ArcPoints(camera, center, radius, start, turn, Segments(camera, radius, turn)), color, stroke,
               FullTurn(sweep), blend);
}

void PlaneCircle(DrawList& out, const Camera& camera, Vec2 center, float radius, Color color, const Stroke& stroke,
                 Blend blend)
{
  PlaneArc(out, camera, center, radius, 0.0f, kTwoPi, color, stroke, blend);
}

void PlaneSector(DrawList& out, const Camera& camera, Vec2 center, float inner, float outer, float start, float sweep,
                 Color color, Blend blend)
{
  if (outer <= 0.0f || color.a == 0) {
    return;
  }
  if (camera.View() == CameraView::TopDown) {
    out.Sector(camera.WorldToScreen(center), camera.PixelsFor(std::max(0.0f, inner)), camera.PixelsFor(outer),
               camera.ScreenHeading(start), sweep, color, blend);
    return;
  }
  const bool full = FullTurn(sweep);
  const float turn = full ? kTwoPi : sweep;
  const int segments = Segments(camera, outer, turn);
  std::vector<Point> rim = ArcPoints(camera, center, outer, start, turn, segments);
  if (inner <= 0.0f && (full || std::abs(sweep) <= kPi)) {
    if (!full) {
      rim.insert(rim.begin(), camera.WorldToScreen(center));
    }
    out.FillConvex(std::move(rim), color, blend);
    return;
  }
  // A ring or a wide sector: a strip between the inner and outer arcs (an inner radius of 0 is the centre).
  const std::vector<Point> hole = ArcPoints(camera, center, std::max(0.0f, inner), start, turn, segments);
  std::vector<Vertex> vertices;
  vertices.reserve(rim.size() * 6);
  const size_t count = full ? rim.size() : rim.size() - 1;
  for (size_t i = 0; i < count; ++i) {
    const size_t j = (i + 1) % rim.size();
    vertices.insert(
        vertices.end(),
        {{hole[i], color}, {rim[i], color}, {rim[j], color}, {hole[i], color}, {rim[j], color}, {hole[j], color}});
  }
  out.Triangles(std::move(vertices), blend);
}

void PlaneDisc(DrawList& out, const Camera& camera, Vec2 center, float radius, Color color, Blend blend)
{
  PlaneSector(out, camera, center, 0.0f, radius, 0.0f, kTwoPi, color, blend);
}

} // namespace reb::battle_ui
