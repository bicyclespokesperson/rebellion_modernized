#include "port/native/space_battle/draw_executor.h"

#include <cmath>

namespace reb::native::space_battle
{
namespace
{

using battle_ui::Blend;
using battle_ui::Color;
using battle_ui::Point;

constexpr float kFringe = 1.0f;    // device pixels of anti-aliasing at shape edges
constexpr float kSegmentPx = 4.0f; // arc tessellation: chord length
constexpr int kMaxSegments = 1024;

SDL_FColor ToFColor(Color color)
{
  return {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
}

SDL_FColor Transparent(SDL_FColor color)
{
  return {color.r, color.g, color.b, 0.0f};
}

Point Normalized(Point p)
{
  const float length = battle_ui::Length(p);
  return length > 1e-6f ? p * (1.0f / length) : Point{};
}

// The miter offset for a vertex between edges with unit normals a and b (as Dear ImGui computes it).
Point Miter(Point a, Point b)
{
  Point m{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
  const float d2 = m.x * m.x + m.y * m.y;
  if (d2 > 1e-6f) {
    m = m * std::min(1.0f / d2, 100.0f);
  }
  return m;
}

int ArcSegments(float radius, float sweep)
{
  const int wanted = static_cast<int>(std::ceil(std::abs(sweep) * radius / kSegmentPx));
  return std::clamp(wanted, 6, kMaxSegments);
}

std::vector<Point> ArcPoints(Point center, float radius, float start, float sweep, bool full)
{
  const int segments = ArcSegments(radius, sweep);
  std::vector<Point> points;
  points.reserve(static_cast<size_t>(segments) + 1);
  const int count = full ? segments : segments + 1;
  for (int k = 0; k < count; ++k) {
    const float angle = start + sweep * static_cast<float>(k) / static_cast<float>(segments);
    points.push_back(center + battle_ui::ScreenDirection(angle) * radius);
  }
  return points;
}

// Splits a path into its dashes.
std::vector<std::vector<Point>> Dashes(const std::vector<Point>& points, bool closed, const battle_ui::Stroke& stroke)
{
  std::vector<std::vector<Point>> dashes;
  const float period = stroke.dash + stroke.gap;
  float phase = std::fmod(stroke.dashOffset, period);
  if (phase < 0.0f) {
    phase += period;
  }
  std::vector<Point> current;
  bool on = phase < stroke.dash;
  float untilToggle = on ? stroke.dash - phase : period - phase;
  if (on) {
    current.push_back(points.front());
  }
  const size_t count = closed ? points.size() + 1 : points.size();
  for (size_t i = 1; i < count; ++i) {
    Point from = points[i - 1];
    const Point to = points[i % points.size()];
    float remaining = battle_ui::Distance(from, to);
    while (remaining > 0.0f) {
      const float step = std::min(remaining, untilToggle);
      const Point dir = Normalized(to - from);
      from = from + dir * step;
      remaining -= step;
      untilToggle -= step;
      if (untilToggle <= 1e-4f) {
        if (on) {
          current.push_back(from);
          dashes.push_back(std::move(current));
          current.clear();
        } else {
          current = {from};
        }
        on = !on;
        untilToggle = on ? stroke.dash : stroke.gap;
      }
    }
    if (on) {
      current.push_back(to);
    }
  }
  if (on && current.size() >= 2) {
    dashes.push_back(std::move(current));
  }
  return dashes;
}

} // namespace

DrawExecutor::DrawExecutor(TextAtlas& text, SpriteSource& sprites)
    : m_text(text)
    , m_sprites(sprites)
{
}

void DrawExecutor::Execute(SDL_Renderer* renderer, const battle_ui::DrawList& list)
{
  m_renderer = renderer;
  m_white = m_text.WhiteTexel();
  m_texture = nullptr;
  m_blend = Blend::Alpha;
  m_vertices.clear();
  for (const battle_ui::DrawCommand& command : list.Commands()) {
    std::visit(
        [this](const auto& c) {
          Draw(c);
        },
        command);
  }
  Flush();
  SDL_SetRenderClipRect(renderer, nullptr);
}

void DrawExecutor::Batch(SDL_Texture* texture, Blend blend)
{
  if (texture != m_texture || blend != m_blend) {
    Flush();
    m_texture = texture;
    m_blend = blend;
  }
}

void DrawExecutor::Flush()
{
  if (m_vertices.empty()) {
    return;
  }
  SDL_Texture* texture = m_texture;
  if (!texture) {
    texture = m_text.Texture(m_renderer);
    const float scale = 1.0f / static_cast<float>(m_text.Size());
    for (SDL_Vertex& v : m_vertices) {
      v.tex_coord = {v.tex_coord.x * scale, v.tex_coord.y * scale};
    }
  }
  const SDL_BlendMode mode = m_blend == Blend::Additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND;
  if (texture) {
    SDL_SetTextureBlendMode(texture, mode);
  } else {
    SDL_SetRenderDrawBlendMode(m_renderer, mode);
  }
  SDL_RenderGeometry(m_renderer, texture, m_vertices.data(), static_cast<int>(m_vertices.size()), nullptr, 0);
  m_vertices.clear();
}

void DrawExecutor::Vertex(Point p, SDL_FColor color)
{
  m_vertices.push_back({{p.x, p.y}, color, m_white});
}

void DrawExecutor::Triangle(Point a, Point b, Point c, SDL_FColor ca, SDL_FColor cb, SDL_FColor cc)
{
  Vertex(a, ca);
  Vertex(b, cb);
  Vertex(c, cc);
}

// The texture coordinates span the white block rather than one texel: SDL's software renderer turns a uniform
// axis-aligned quad into a texture copy, or with a zero-sized source into a fill that ignores additive blending.
void DrawExecutor::FilledRect(float x0, float y0, float x1, float y1, SDL_FColor color)
{
  if (x1 <= x0 || y1 <= y0) {
    return;
  }
  const SDL_FPoint lo{m_white.x - 1.0f, m_white.y - 1.0f};
  const SDL_FPoint hi{m_white.x + 1.0f, m_white.y + 1.0f};
  const SDL_Vertex a{{x0, y0}, color, lo};
  const SDL_Vertex b{{x1, y0}, color, {hi.x, lo.y}};
  const SDL_Vertex c{{x1, y1}, color, hi};
  const SDL_Vertex d{{x0, y1}, color, {lo.x, hi.y}};
  m_vertices.insert(m_vertices.end(), {a, b, c, a, c, d});
}

void DrawExecutor::Draw(const battle_ui::DrawRect& rect)
{
  Batch(nullptr, rect.blend);
  const SDL_FColor color = ToFColor(rect.color);
  const float x0 = std::round(rect.rect.x);
  const float y0 = std::round(rect.rect.y);
  const float x1 = std::round(rect.rect.Right());
  const float y1 = std::round(rect.rect.Bottom());
  if (rect.lineWidth <= 0.0f) {
    FilledRect(x0, y0, x1, y1, color);
    return;
  }
  const float w = std::max(1.0f, std::round(rect.lineWidth));
  FilledRect(x0, y0, x1, y0 + w, color);
  FilledRect(x0, y1 - w, x1, y1, color);
  FilledRect(x0, y0 + w, x0 + w, y1 - w, color);
  FilledRect(x1 - w, y0 + w, x1, y1 - w, color);
}

void DrawExecutor::Polyline(const std::vector<Point>& input, bool closed, SDL_FColor color, float width)
{
  std::vector<Point> points;
  points.reserve(input.size());
  for (const Point& p : input) {
    if (points.empty() || battle_ui::Distance(points.back(), p) > 1e-3f) {
      points.push_back(p);
    }
  }
  if (closed && points.size() > 2 && battle_ui::Distance(points.front(), points.back()) <= 1e-3f) {
    points.pop_back();
  }
  const size_t n = points.size();
  if (n < 2) {
    return;
  }
  if (width < 1.0f) {
    color.a *= width;
    width = 1.0f;
  }
  const float inner = (width - kFringe) * 0.5f;
  const float outer = inner + kFringe;
  const size_t segments = closed ? n : n - 1;
  std::vector<Point> normals(n);
  for (size_t i = 0; i < segments; ++i) {
    const Point d = Normalized(points[(i + 1) % n] - points[i]);
    normals[i] = {d.y, -d.x};
  }
  if (!closed) {
    normals[n - 1] = normals[n - 2];
  }
  std::vector<Point> miters(n);
  for (size_t i = 0; i < n; ++i) {
    const bool endpoint = !closed && (i == 0 || i == n - 1);
    miters[i] = endpoint ? normals[i] : Miter(normals[(i + n - 1) % n], normals[i]);
  }
  const SDL_FColor clear = Transparent(color);
  for (size_t i = 0; i < segments; ++i) {
    const size_t j = (i + 1) % n;
    const Point oi = points[i] + miters[i] * outer;
    const Point ii = points[i] + miters[i] * inner;
    const Point ri = points[i] - miters[i] * inner;
    const Point si = points[i] - miters[i] * outer;
    const Point oj = points[j] + miters[j] * outer;
    const Point ij = points[j] + miters[j] * inner;
    const Point rj = points[j] - miters[j] * inner;
    const Point sj = points[j] - miters[j] * outer;
    Triangle(oi, oj, ij, clear, clear, color);
    Triangle(oi, ij, ii, clear, color, color);
    if (inner > 0.0f) {
      Triangle(ii, ij, rj, color, color, color);
      Triangle(ii, rj, ri, color, color, color);
    }
    Triangle(ri, rj, sj, color, color, clear);
    Triangle(ri, sj, si, color, clear, clear);
  }
}

void DrawExecutor::StrokePath(const std::vector<Point>& points, bool closed, Color color,
                              const battle_ui::Stroke& stroke, Blend blend)
{
  if (points.size() < 2 || color.a == 0) {
    return;
  }
  Batch(nullptr, blend);
  const SDL_FColor fcolor = ToFColor(color);
  if (stroke.dash > 0.0f && stroke.gap > 0.0f) {
    for (const std::vector<Point>& dash : Dashes(points, closed, stroke)) {
      Polyline(dash, false, fcolor, stroke.width);
    }
    return;
  }
  Polyline(points, closed, fcolor, stroke.width);
}

void DrawExecutor::Draw(const battle_ui::DrawPolyline& line)
{
  StrokePath(line.points, line.closed, line.color, line.stroke, line.blend);
}

void DrawExecutor::Draw(const battle_ui::DrawArc& arc)
{
  if (arc.radius <= 0.0f) {
    return;
  }
  const bool full = std::abs(arc.sweep) >= battle_ui::kTwoPi - 1e-4f;
  StrokePath(ArcPoints(arc.center, arc.radius, arc.start, full ? battle_ui::kTwoPi : arc.sweep, full), full, arc.color,
             arc.stroke, arc.blend);
}

void DrawExecutor::Draw(const battle_ui::DrawSector& sector)
{
  if (sector.outerRadius <= 0.0f || sector.color.a == 0) {
    return;
  }
  Batch(nullptr, sector.blend);
  const SDL_FColor color = ToFColor(sector.color);
  const SDL_FColor clear = Transparent(color);
  const bool full = std::abs(sector.sweep) >= battle_ui::kTwoPi - 1e-4f;
  const float sweep = full ? battle_ui::kTwoPi : sector.sweep;
  const int segments = ArcSegments(sector.outerRadius, sweep);
  const float half = kFringe * 0.5f;
  const float outerCore = std::max(0.0f, sector.outerRadius - half);
  const float outerEdge = sector.outerRadius + half;
  const bool pie = sector.innerRadius <= 0.0f;
  const float innerCore = pie ? 0.0f : std::min(outerCore, sector.innerRadius + half);
  const float innerEdge = pie ? 0.0f : std::max(0.0f, sector.innerRadius - half);
  const auto at = [&](float radius, int k) {
    const float angle = sector.start + sweep * static_cast<float>(k) / static_cast<float>(segments);
    return sector.center + battle_ui::ScreenDirection(angle) * radius;
  };
  for (int k = 0; k < segments; ++k) {
    const Point oc0 = at(outerCore, k);
    const Point oc1 = at(outerCore, k + 1);
    const Point oe0 = at(outerEdge, k);
    const Point oe1 = at(outerEdge, k + 1);
    if (pie) {
      Triangle(sector.center, oc0, oc1, color, color, color);
    } else {
      const Point ic0 = at(innerCore, k);
      const Point ic1 = at(innerCore, k + 1);
      const Point ie0 = at(innerEdge, k);
      const Point ie1 = at(innerEdge, k + 1);
      Triangle(ic0, oc0, oc1, color, color, color);
      Triangle(ic0, oc1, ic1, color, color, color);
      Triangle(ie0, ic0, ic1, clear, color, color);
      Triangle(ie0, ic1, ie1, clear, color, clear);
    }
    Triangle(oc0, oe0, oe1, color, clear, clear);
    Triangle(oc0, oe1, oc1, color, clear, color);
  }
}

void DrawExecutor::Draw(const battle_ui::DrawPolygon& polygon)
{
  const std::vector<Point>& points = polygon.points;
  const size_t n = points.size();
  if (n < 3 || polygon.color.a == 0) {
    return;
  }
  Batch(nullptr, polygon.blend);
  float area = 0.0f;
  for (size_t i = 0; i < n; ++i) {
    const Point a = points[i];
    const Point b = points[(i + 1) % n];
    area += a.x * b.y - b.x * a.y;
  }
  if (std::abs(area) < 1e-4f) {
    return;
  }
  const float sign = area > 0.0f ? 1.0f : -1.0f;
  std::vector<Point> normals(n);
  for (size_t i = 0; i < n; ++i) {
    const Point d = Normalized(points[(i + 1) % n] - points[i]);
    normals[i] = Point{d.y, -d.x} * sign;
  }
  const SDL_FColor color = ToFColor(polygon.color);
  const SDL_FColor clear = Transparent(color);
  std::vector<Point> inner(n);
  std::vector<Point> outer(n);
  for (size_t i = 0; i < n; ++i) {
    const Point miter = Miter(normals[(i + n - 1) % n], normals[i]);
    inner[i] = points[i] - miter * (kFringe * 0.5f);
    outer[i] = points[i] + miter * (kFringe * 0.5f);
  }
  for (size_t k = 1; k + 1 < n; ++k) {
    Triangle(inner[0], inner[k], inner[k + 1], color, color, color);
  }
  for (size_t i = 0; i < n; ++i) {
    const size_t j = (i + 1) % n;
    Triangle(inner[i], inner[j], outer[j], color, color, clear);
    Triangle(inner[i], outer[j], outer[i], color, clear, clear);
  }
}

void DrawExecutor::Draw(const battle_ui::DrawTriangles& triangles)
{
  Batch(nullptr, triangles.blend);
  for (const battle_ui::Vertex& v : triangles.vertices) {
    Vertex(v.position, ToFColor(v.color));
  }
}

void DrawExecutor::Draw(const battle_ui::DrawText& text)
{
  if (text.text.empty() || text.color.a == 0) {
    return;
  }
  Batch(nullptr, Blend::Alpha);
  m_text.Append(text, m_vertices);
}

void DrawExecutor::Draw(const battle_ui::DrawSprite& sprite)
{
  SpriteFrame frame;
  if (!m_sprites.Frame(m_renderer, sprite.typeIndex, sprite.view, sprite.layer, sprite.heading, sprite.length, frame)
      || !frame.texture || frame.length <= 0.0f) {
    return;
  }
  float textureWidth = 0.0f;
  float textureHeight = 0.0f;
  SDL_GetTextureSize(frame.texture, &textureWidth, &textureHeight);
  Batch(frame.texture, sprite.blend);
  const float scale = sprite.length / frame.length;
  const float w = frame.source.w * scale;
  const float h = frame.source.h * scale;
  const float angle = sprite.view == battle_ui::SpriteView::TopDown ? sprite.heading - frame.artHeading : 0.0f;
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  const auto place = [&](float lx, float ly) {
    return Point{sprite.center.x + lx * c + ly * s, sprite.center.y - lx * s + ly * c};
  };
  const float left = -frame.pivot.x * w;
  const float top = -frame.pivot.y * h;
  const Point a = place(left, top);
  const Point b = place(left + w, top);
  const Point cc = place(left + w, top + h);
  const Point d = place(left, top + h);
  const float u0 = frame.source.x / textureWidth;
  const float v0 = frame.source.y / textureHeight;
  const float u1 = (frame.source.x + frame.source.w) / textureWidth;
  const float v1 = (frame.source.y + frame.source.h) / textureHeight;
  const SDL_FColor tint = ToFColor(sprite.tint);
  m_vertices.insert(m_vertices.end(), {{{a.x, a.y}, tint, {u0, v0}},
                                       {{b.x, b.y}, tint, {u1, v0}},
                                       {{cc.x, cc.y}, tint, {u1, v1}},
                                       {{a.x, a.y}, tint, {u0, v0}},
                                       {{cc.x, cc.y}, tint, {u1, v1}},
                                       {{d.x, d.y}, tint, {u0, v1}}});
}

void DrawExecutor::Draw(const battle_ui::DrawClip& clip)
{
  Flush();
  if (!clip.rect) {
    SDL_SetRenderClipRect(m_renderer, nullptr);
    return;
  }
  const int left = static_cast<int>(std::floor(clip.rect->x));
  const int top = static_cast<int>(std::floor(clip.rect->y));
  const int right = static_cast<int>(std::ceil(clip.rect->Right()));
  const int bottom = static_cast<int>(std::ceil(clip.rect->Bottom()));
  const SDL_Rect rect{left, top, right - left, bottom - top};
  SDL_SetRenderClipRect(m_renderer, &rect);
}

} // namespace reb::native::space_battle
