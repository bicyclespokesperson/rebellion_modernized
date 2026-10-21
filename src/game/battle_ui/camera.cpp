#include "battle_ui/camera.h"

#include <array>

namespace reb::battle_ui
{
namespace
{

Vec2 Rotated(Vec2 v, float angle)
{
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  return {v.x * c - v.y * s, v.x * s + v.y * c};
}

} // namespace

void Camera::SetViewport(const Rect& viewport)
{
  m_viewport = viewport;
}

void Camera::SetUiScale(float uiScale)
{
  const float ratio = uiScale / m_uiScale;
  m_uiScale = uiScale;
  SetZoom(m_zoom * ratio);
}

void Camera::SetZoom(float pixelsPerMeter)
{
  m_zoom = std::clamp(pixelsPerMeter, kMinZoom * m_uiScale, kMaxZoom * m_uiScale);
}

void Camera::ZoomAt(Point anchor, float factor)
{
  const Vec2 before = ScreenToWorld(anchor);
  SetZoom(m_zoom * factor);
  const Vec2 after = ScreenToWorld(anchor);
  m_center.x += before.x - after.x;
  m_center.y += before.y - after.y;
}

void Camera::PanPixels(Point delta)
{
  const Point centerOnScreen = m_viewport.Center();
  const Vec2 from = ScreenToWorld(centerOnScreen);
  const Vec2 to = ScreenToWorld(centerOnScreen - delta);
  m_center.x += to.x - from.x;
  m_center.y += to.y - from.y;
}

void Camera::FrameBox(const WorldBox& box, float margin)
{
  m_center = {(box.min.x + box.max.x) * 0.5f, (box.min.y + box.max.y) * 0.5f};
  const std::array<Vec2, 4> corners{
      {{box.min.x, box.min.y}, {box.max.x, box.min.y}, {box.min.x, box.max.y}, {box.max.x, box.max.y}}};
  float halfWidth = 1.0f;
  float halfHeight = 1.0f;
  const float pitchScale = VerticalScale();
  for (const Vec2& corner : corners) {
    const Vec2 rotated = Rotated({corner.x - m_center.x, corner.y - m_center.y}, -Rotation());
    halfWidth = std::max(halfWidth, std::abs(rotated.x));
    halfHeight = std::max(halfHeight, std::abs(rotated.y) * pitchScale);
  }
  const float usable = 1.0f - 2.0f * margin;
  SetZoom(std::min(m_viewport.w * 0.5f * usable / halfWidth, m_viewport.h * 0.5f * usable / halfHeight));
}

void Camera::FrameBoxInto(const WorldBox& box, const Rect& area, float margin)
{
  const Rect viewport = m_viewport;
  m_viewport = area;
  FrameBox(box, margin);
  m_viewport = viewport;
  PanPixels(area.Center() - viewport.Center());
}

void Camera::Rotate(int steps)
{
  m_rotationStep = ((m_rotationStep + steps) % kRotationSteps + kRotationSteps) % kRotationSteps;
}

float Camera::Rotation() const
{
  return static_cast<float>(m_rotationStep) * kTwoPi / kRotationSteps;
}

ZoomLook Camera::Look() const
{
  return m_zoom / m_uiScale >= kDetailedZoom ? ZoomLook::Detailed : ZoomLook::Tactical;
}

float Camera::VerticalScale() const
{
  return m_view == CameraView::Angled ? std::sin(kAngledPitch) : 1.0f;
}

Point Camera::WorldToScreen(Vec2 world) const
{
  const Vec2 v = Rotated({world.x - m_center.x, world.y - m_center.y}, -Rotation());
  const Point center = m_viewport.Center();
  return {center.x + v.x * m_zoom, center.y - v.y * m_zoom * VerticalScale()};
}

Point Camera::WorldToScreen(Vec2 world, float height) const
{
  const Point onPlane = WorldToScreen(world);
  if (m_view != CameraView::Angled) {
    return onPlane;
  }
  return {onPlane.x, onPlane.y - height * m_zoom * std::cos(kAngledPitch)};
}

Vec2 Camera::ScreenToWorld(Point screen) const
{
  const Point center = m_viewport.Center();
  const Vec2 v{(screen.x - center.x) / m_zoom, -(screen.y - center.y) / (m_zoom * VerticalScale())};
  const Vec2 world = Rotated(v, Rotation());
  return {world.x + m_center.x, world.y + m_center.y};
}

float Camera::ScreenHeading(float worldHeading) const
{
  return worldHeading - Rotation();
}

float Camera::ProjectedAngle(float worldHeading) const
{
  const float relative = ScreenHeading(worldHeading);
  return std::atan2(std::sin(relative) * VerticalScale(), std::cos(relative));
}

WorldBox Camera::VisibleBox() const
{
  const std::array<Point, 4> corners{{{m_viewport.x, m_viewport.y},
                                      {m_viewport.Right(), m_viewport.y},
                                      {m_viewport.x, m_viewport.Bottom()},
                                      {m_viewport.Right(), m_viewport.Bottom()}}};
  WorldBox box{ScreenToWorld(corners[0]), ScreenToWorld(corners[0])};
  for (const Point& corner : corners) {
    const Vec2 world = ScreenToWorld(corner);
    box.min = {std::min(box.min.x, world.x), std::min(box.min.y, world.y)};
    box.max = {std::max(box.max.x, world.x), std::max(box.max.y, world.y)};
  }
  return box;
}

} // namespace reb::battle_ui
