#include "battle_ui/draw_list.h"

namespace reb::battle_ui
{

void DrawList::Clear()
{
  m_commands.clear();
  m_clips.clear();
}

void DrawList::FillRect(const Rect& rect, Color color, Blend blend)
{
  Add(DrawRect{rect, color, 0.0f, blend});
}

void DrawList::StrokeRect(const Rect& rect, Color color, float width)
{
  Add(DrawRect{rect, color, width, Blend::Alpha});
}

void DrawList::Line(Point a, Point b, Color color, const Stroke& stroke, Blend blend)
{
  Add(DrawPolyline{{a, b}, color, stroke, false, blend});
}

void DrawList::Polyline(std::vector<Point> points, Color color, const Stroke& stroke, bool closed, Blend blend)
{
  Add(DrawPolyline{std::move(points), color, stroke, closed, blend});
}

void DrawList::Circle(Point center, float radius, Color color, const Stroke& stroke, Blend blend)
{
  Add(DrawArc{center, radius, 0.0f, kTwoPi, color, stroke, blend});
}

void DrawList::Arc(Point center, float radius, float start, float sweep, Color color, const Stroke& stroke, Blend blend)
{
  Add(DrawArc{center, radius, start, sweep, color, stroke, blend});
}

void DrawList::FillCircle(Point center, float radius, Color color, Blend blend)
{
  Add(DrawSector{center, 0.0f, radius, 0.0f, kTwoPi, color, blend});
}

void DrawList::Ring(Point center, float innerRadius, float outerRadius, Color color, Blend blend)
{
  Add(DrawSector{center, innerRadius, outerRadius, 0.0f, kTwoPi, color, blend});
}

void DrawList::Sector(Point center, float innerRadius, float outerRadius, float start, float sweep, Color color,
                      Blend blend)
{
  Add(DrawSector{center, innerRadius, outerRadius, start, sweep, color, blend});
}

void DrawList::FillTriangle(Point a, Point b, Point c, Color color, Blend blend)
{
  Add(DrawPolygon{{a, b, c}, color, blend});
}

void DrawList::FillConvex(std::vector<Point> points, Color color, Blend blend)
{
  Add(DrawPolygon{std::move(points), color, blend});
}

void DrawList::Triangles(std::vector<Vertex> vertices, Blend blend)
{
  Add(DrawTriangles{std::move(vertices), blend});
}

void DrawList::Text(std::string text, Point position, float size, Color color, Align align, VAlign valign,
                    FontWeight weight, float maxWidth)
{
  Add(DrawText{std::move(text), position, size, color, align, valign, weight, maxWidth});
}

void DrawList::PushClip(const Rect& rect)
{
  const Rect clip = m_clips.empty() ? rect : Intersection(m_clips.back(), rect);
  m_clips.push_back(clip);
  Add(DrawClip{clip});
}

void DrawList::PopClip()
{
  if (!m_clips.empty()) {
    m_clips.pop_back();
  }
  Add(DrawClip{m_clips.empty() ? std::nullopt : std::optional<Rect>(m_clips.back())});
}

} // namespace reb::battle_ui
