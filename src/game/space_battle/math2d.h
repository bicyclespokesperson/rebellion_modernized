#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

#include "space_battle/commands.h"

// Plane geometry for the simulator. Angles are radians, 0 = +x, counter-clockwise.
namespace reb::battle
{

inline constexpr float kPi = std::numbers::pi_v<float>;
inline constexpr float kTwoPi = 2.0f * kPi;

inline Vec2 operator+(Vec2 a, Vec2 b)
{
  return {a.x + b.x, a.y + b.y};
}
inline Vec2 operator-(Vec2 a, Vec2 b)
{
  return {a.x - b.x, a.y - b.y};
}
inline Vec2 operator-(Vec2 a)
{
  return {-a.x, -a.y};
}
inline Vec2 operator*(Vec2 a, float s)
{
  return {a.x * s, a.y * s};
}
inline Vec2 operator*(float s, Vec2 a)
{
  return {a.x * s, a.y * s};
}
inline Vec2& operator+=(Vec2& a, Vec2 b)
{
  a.x += b.x;
  a.y += b.y;
  return a;
}
inline bool operator==(Vec2 a, Vec2 b)
{
  return a.x == b.x && a.y == b.y;
}

inline float Dot(Vec2 a, Vec2 b)
{
  return a.x * b.x + a.y * b.y;
}
inline float Cross(Vec2 a, Vec2 b)
{
  return a.x * b.y - a.y * b.x;
}
inline float LengthSq(Vec2 a)
{
  return Dot(a, a);
}
inline float Length(Vec2 a)
{
  return std::sqrt(LengthSq(a));
}
inline float Distance(Vec2 a, Vec2 b)
{
  return Length(a - b);
}
inline float DistanceSq(Vec2 a, Vec2 b)
{
  return LengthSq(a - b);
}
inline Vec2 Normalized(Vec2 a, Vec2 fallback = {1.0f, 0.0f})
{
  const float length = Length(a);
  return length > 1e-6f ? a * (1.0f / length) : fallback;
}
inline Vec2 FromAngle(float angle)
{
  return {std::cos(angle), std::sin(angle)};
}
inline float AngleOf(Vec2 a)
{
  return std::atan2(a.y, a.x);
}
inline Vec2 Rotated(Vec2 a, float angle)
{
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  return {a.x * c - a.y * s, a.x * s + a.y * c};
}
// Wraps into (-pi, pi].
inline float WrapAngle(float angle)
{
  angle = std::fmod(angle, kTwoPi);
  if (angle <= -kPi) {
    angle += kTwoPi;
  } else if (angle > kPi) {
    angle -= kTwoPi;
  }
  return angle;
}
// Signed shortest rotation from `from` to `to`.
inline float AngleDelta(float from, float to)
{
  return WrapAngle(to - from);
}
inline Vec2 Lerp(Vec2 a, Vec2 b, float t)
{
  return a + (b - a) * t;
}
inline float LerpAngle(float a, float b, float t)
{
  return WrapAngle(a + AngleDelta(a, b) * t);
}
inline Vec2 Clamped(Vec2 a, float halfExtent)
{
  return {std::clamp(a.x, -halfExtent, halfExtent), std::clamp(a.y, -halfExtent, halfExtent)};
}

} // namespace reb::battle
