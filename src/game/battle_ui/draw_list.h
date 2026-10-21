#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "battle_ui/ui_types.h"

// What the battle screen draws in one frame, in device pixels, in paint order. The game builds it without SDL; the
// port's executor (src/port/native/space_battle/draw_executor.cpp) renders it. Angles are screen radians: 0 = right,
// positive = counter-clockwise as seen on screen.
namespace reb::battle_ui
{

enum class Blend : uint8_t { Alpha, Additive };

enum class SpriteView : uint8_t { TopDown, Angled };
enum class SpriteLayer : uint8_t { Color, Emissive };

// A ship image from the sprite pack, by tactical ship-type index. The art faces +x (bow to the right) at heading 0.
struct DrawSprite {
  int32_t typeIndex = -1;
  SpriteView view = SpriteView::TopDown;
  SpriteLayer layer = SpriteLayer::Color;
  Point center;
  float heading = 0.0f; // top-down: rotation on screen; angled: the relative heading that picks the frame
  float length = 0.0f;  // on-screen length of the hull's long axis; the sprite source picks the frame size by it
  Color tint;
  Blend blend = Blend::Alpha;
};

struct DrawRect {
  Rect rect;
  Color color;
  float lineWidth = 0.0f; // 0 = filled
  Blend blend = Blend::Alpha;
};

struct Stroke {
  float width = 1.0f;
  float dash = 0.0f; // dash and gap lengths along the line; 0 = solid
  float gap = 0.0f;
  float dashOffset = 0.0f;
};

struct DrawPolyline {
  std::vector<Point> points;
  Color color;
  Stroke stroke;
  bool closed = false;
  Blend blend = Blend::Alpha;
};

// An outlined arc (a full circle when |sweep| >= 2 pi).
struct DrawArc {
  Point center;
  float radius = 0.0f;
  float start = 0.0f;
  float sweep = kTwoPi;
  Color color;
  Stroke stroke;
  Blend blend = Blend::Alpha;
};

// A filled annular sector: a pie slice when innerRadius is 0, a ring when the sweep is a full turn, a disc when both.
struct DrawSector {
  Point center;
  float innerRadius = 0.0f;
  float outerRadius = 0.0f;
  float start = 0.0f;
  float sweep = kTwoPi;
  Color color;
  Blend blend = Blend::Alpha;
};

// A filled convex polygon; its edges are anti-aliased.
struct DrawPolygon {
  std::vector<Point> points;
  Color color;
  Blend blend = Blend::Alpha;
};

struct Vertex {
  Point position;
  Color color;
};

// Raw triangles (every three vertices), with per-vertex colour and no anti-aliasing.
struct DrawTriangles {
  std::vector<Vertex> vertices;
  Blend blend = Blend::Alpha;
};

enum class Align : uint8_t { Start, Center, End };
enum class VAlign : uint8_t { Top, Middle, Baseline, Bottom };
enum class FontWeight : uint8_t { Regular, Bold };
enum class TextOverflow : uint8_t { Ellipsis, Clip };

struct DrawText {
  std::string text;   // UTF-8; bytes that are not valid UTF-8 are read as Windows-1252 (the strategic game's names)
  Point position;     // the anchor that align / valign refer to
  float size = 14.0f; // pixel size (em height)
  Color color;
  Align align = Align::Start;
  VAlign valign = VAlign::Top;
  FontWeight weight = FontWeight::Regular;
  float maxWidth = 0.0f; // 0 = unlimited
  TextOverflow overflow = TextOverflow::Ellipsis;
};

// Sets the clip rectangle for the commands that follow; nullopt clears it.
struct DrawClip {
  std::optional<Rect> rect;
};

using DrawCommand = std::variant<DrawSprite, DrawRect, DrawPolyline, DrawArc, DrawSector, DrawPolygon, DrawTriangles,
                                 DrawText, DrawClip>;

class DrawList
{
  public:
  void Clear();
  const std::vector<DrawCommand>& Commands() const
  {
    return m_commands;
  }
  void Add(DrawCommand command)
  {
    m_commands.push_back(std::move(command));
  }

  void Sprite(const DrawSprite& sprite)
  {
    Add(sprite);
  }
  void FillRect(const Rect& rect, Color color, Blend blend = Blend::Alpha);
  void StrokeRect(const Rect& rect, Color color, float width = 1.0f);
  void Line(Point a, Point b, Color color, const Stroke& stroke = {}, Blend blend = Blend::Alpha);
  void Polyline(std::vector<Point> points, Color color, const Stroke& stroke = {}, bool closed = false,
                Blend blend = Blend::Alpha);
  void Circle(Point center, float radius, Color color, const Stroke& stroke = {}, Blend blend = Blend::Alpha);
  void Arc(Point center, float radius, float start, float sweep, Color color, const Stroke& stroke = {},
           Blend blend = Blend::Alpha);
  void FillCircle(Point center, float radius, Color color, Blend blend = Blend::Alpha);
  void Ring(Point center, float innerRadius, float outerRadius, Color color, Blend blend = Blend::Alpha);
  void Sector(Point center, float innerRadius, float outerRadius, float start, float sweep, Color color,
              Blend blend = Blend::Alpha);
  void FillTriangle(Point a, Point b, Point c, Color color, Blend blend = Blend::Alpha);
  void FillConvex(std::vector<Point> points, Color color, Blend blend = Blend::Alpha);
  void Triangles(std::vector<Vertex> vertices, Blend blend = Blend::Alpha);
  void Text(DrawText text)
  {
    Add(std::move(text));
  }
  void Text(std::string text, Point position, float size, Color color, Align align = Align::Start,
            VAlign valign = VAlign::Top, FontWeight weight = FontWeight::Regular, float maxWidth = 0.0f);

  // Nested clipping: each push intersects with the enclosing clip.
  void PushClip(const Rect& rect);
  void PopClip();

  template <class T>
  size_t Count() const
  {
    size_t n = 0;
    for (const DrawCommand& command : m_commands) {
      n += std::holds_alternative<T>(command) ? 1 : 0;
    }
    return n;
  }

  private:
  std::vector<DrawCommand> m_commands;
  std::vector<Rect> m_clips;
};

} // namespace reb::battle_ui
