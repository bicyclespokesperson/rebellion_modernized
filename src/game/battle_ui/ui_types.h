#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Screen-space basics for the battle UI. Everything here is in device pixels (the renderer's output pixels, not
// window points), y down. World positions use reb::battle::Vec2 (meters, y up).
namespace reb::battle_ui
{

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = 2.0f * kPi;

struct Point {
  float x = 0.0f;
  float y = 0.0f;

  friend Point operator+(Point a, Point b)
  {
    return {a.x + b.x, a.y + b.y};
  }
  friend Point operator-(Point a, Point b)
  {
    return {a.x - b.x, a.y - b.y};
  }
  friend Point operator*(Point a, float s)
  {
    return {a.x * s, a.y * s};
  }
  friend bool operator==(Point a, Point b) = default;
};

inline float Length(Point p)
{
  return std::sqrt(p.x * p.x + p.y * p.y);
}

inline float Distance(Point a, Point b)
{
  return Length(a - b);
}

// A direction on screen for an angle in screen radians: 0 = right, positive = counter-clockwise as seen (toward up).
inline Point ScreenDirection(float angle)
{
  return {std::cos(angle), -std::sin(angle)};
}

struct Rect {
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;

  float Right() const
  {
    return x + w;
  }
  float Bottom() const
  {
    return y + h;
  }
  Point Center() const
  {
    return {x + w * 0.5f, y + h * 0.5f};
  }
  bool Contains(Point p) const
  {
    return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h;
  }
  bool Empty() const
  {
    return w <= 0.0f || h <= 0.0f;
  }
  Rect Inset(float dx, float dy) const
  {
    return {x + dx, y + dy, w - 2.0f * dx, h - 2.0f * dy};
  }
  Rect Inset(float d) const
  {
    return Inset(d, d);
  }
  friend bool operator==(const Rect&, const Rect&) = default;
};

inline Rect RectFromCorners(Point a, Point b)
{
  return {std::min(a.x, b.x), std::min(a.y, b.y), std::abs(a.x - b.x), std::abs(a.y - b.y)};
}

inline Rect Intersection(const Rect& a, const Rect& b)
{
  const float left = std::max(a.x, b.x);
  const float top = std::max(a.y, b.y);
  const float right = std::min(a.Right(), b.Right());
  const float bottom = std::min(a.Bottom(), b.Bottom());
  return {left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top)};
}

struct Color {
  uint8_t r = 255;
  uint8_t g = 255;
  uint8_t b = 255;
  uint8_t a = 255;

  constexpr Color WithAlpha(uint8_t alpha) const
  {
    return {r, g, b, alpha};
  }
  constexpr Color Faded(float factor) const
  {
    return {r, g, b, static_cast<uint8_t>(std::clamp(a * factor, 0.0f, 255.0f))};
  }
  friend bool operator==(Color, Color) = default;
};

inline constexpr Color Rgb(uint32_t rgb, uint8_t alpha = 255)
{
  return {static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8), static_cast<uint8_t>(rgb), alpha};
}

inline Color Mix(Color a, Color b, float t)
{
  const auto lerp = [t](uint8_t x, uint8_t y) {
    return static_cast<uint8_t>(std::clamp(x + (y - x) * t, 0.0f, 255.0f));
  };
  return {lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), lerp(a.a, b.a)};
}

} // namespace reb::battle_ui
