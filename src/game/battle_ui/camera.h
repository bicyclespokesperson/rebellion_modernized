#pragma once
#include <cstdint>

#include "battle_ui/ui_types.h"
#include "space_battle/commands.h"

// World meters on the battle plane <-> device pixels. Top-down is the commanding view; the angled view (an
// orthographic look at a fixed pitch, ships drawn from frames picked by relative heading, tactical_combat.md §10.1)
// shares the same pan, zoom and rotation and only changes the projection.
namespace reb::battle_ui
{

using reb::battle::Vec2;

enum class CameraView : uint8_t { TopDown, Angled };

// Semantic zoom (§10.1): zoomed out the screen draws tactical symbols over a grid, zoomed in the ship art.
enum class ZoomLook : uint8_t { Tactical, Detailed };

struct WorldBox {
  Vec2 min;
  Vec2 max;
};

class Camera
{
  public:
  // Zoom limits and the semantic threshold, in UI pixels (device pixels / uiScale) per meter.
  static constexpr float kMinZoom = 0.004f;
  static constexpr float kMaxZoom = 0.6f;
  static constexpr float kDetailedZoom = 0.07f;
  static constexpr int kRotationSteps = 8; // 45 degrees each
  // The angled view looks down this far below the horizon; the sprite pack renders its angled frames the same way.
  static constexpr float kAngledPitch = 50.0f * kPi / 180.0f;
  // Fighters are drawn this far above the plane in the angled view (cosmetic: nothing attacks from above, §4).
  static constexpr float kFlightHeight = 220.0f;

  void SetViewport(const Rect& viewport);
  const Rect& Viewport() const
  {
    return m_viewport;
  }
  void SetUiScale(float uiScale);
  float UiScale() const
  {
    return m_uiScale;
  }

  Vec2 Center() const
  {
    return m_center;
  }
  void SetCenter(Vec2 center)
  {
    m_center = center;
  }
  float Zoom() const // device pixels per meter
  {
    return m_zoom;
  }
  void SetZoom(float pixelsPerMeter);
  // Multiplies the zoom while keeping the world point under `anchor` fixed on screen.
  void ZoomAt(Point anchor, float factor);
  // Moves the view so that the world follows the cursor by `delta` pixels.
  void PanPixels(Point delta);
  void FrameBox(const WorldBox& box, float margin = 0.08f);
  // Frames the box into part of the viewport (the battle view between HUD panels) rather than all of it.
  void FrameBoxInto(const WorldBox& box, const Rect& area, float margin = 0.08f);

  int RotationStep() const
  {
    return m_rotationStep;
  }
  void Rotate(int steps);
  float Rotation() const; // radians, counter-clockwise: the world turns clockwise on screen as it grows

  CameraView View() const
  {
    return m_view;
  }
  void SetView(CameraView view)
  {
    m_view = view;
  }
  ZoomLook Look() const;

  Point WorldToScreen(Vec2 world) const;
  // A point `height` meters above the plane (the angled view lifts it; the top-down view ignores height).
  Point WorldToScreen(Vec2 world, float height) const;
  // The point on the plane under a screen position: the inverse projection, a ray cast onto the plane.
  Vec2 ScreenToWorld(Point screen) const;
  // Screen pixels per meter vertically on the plane relative to horizontally: sin(pitch) angled, 1 top-down.
  float VerticalScale() const;
  float PixelsFor(float meters) const
  {
    return meters * m_zoom;
  }
  float MetersFor(float pixels) const
  {
    return pixels / m_zoom;
  }
  // A world heading (radians, 0 = +x, counter-clockwise) as a screen angle; in the angled view, the heading relative
  // to the camera that selects the sprite frame.
  float ScreenHeading(float worldHeading) const;
  // The screen angle a world direction points along once projected (equal to ScreenHeading top-down; foreshortened
  // toward the horizontal in the angled view).
  float ProjectedAngle(float worldHeading) const;
  // The axis-aligned world box that covers the whole viewport.
  WorldBox VisibleBox() const;

  private:
  Rect m_viewport{0.0f, 0.0f, 1280.0f, 720.0f};
  float m_uiScale = 1.0f;
  Vec2 m_center;
  float m_zoom = 0.03f;
  int m_rotationStep = 0;
  CameraView m_view = CameraView::TopDown;
};

} // namespace reb::battle_ui
