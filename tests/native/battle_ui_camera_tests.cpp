// Camera: world <-> screen round trips, zoom about the cursor, rotation steps, framing and the semantic zoom look.
#include "battle_ui/camera.h"
#include "battle_ui_test_support.h"

using namespace reb::battle_ui;
using reb::battle_ui::test::Near;

namespace
{

Camera MakeCamera(CameraView view, int rotation, float zoom)
{
  Camera camera;
  camera.SetViewport({0.0f, 34.0f, 1280.0f, 686.0f});
  camera.SetView(view);
  camera.Rotate(rotation);
  camera.SetZoom(zoom);
  camera.SetCenter({1500.0f, -700.0f});
  return camera;
}

void RoundTrips()
{
  for (const CameraView view : {CameraView::TopDown, CameraView::Angled}) {
    for (int rotation = 0; rotation < Camera::kRotationSteps; ++rotation) {
      for (const float zoom : {0.005f, 0.05f, 0.5f}) {
        const Camera camera = MakeCamera(view, rotation, zoom);
        for (const Vec2 world : {Vec2{0.0f, 0.0f}, Vec2{-12000.0f, 3000.0f}, Vec2{1500.0f, -700.0f}}) {
          const Vec2 back = camera.ScreenToWorld(camera.WorldToScreen(world));
          REQUIRE(Near(back.x, world.x, 0.05f) && Near(back.y, world.y, 0.05f));
        }
        for (const Point screen : {Point{0.0f, 34.0f}, Point{640.0f, 377.0f}, Point{1279.0f, 719.0f}}) {
          REQUIRE(Near(camera.WorldToScreen(camera.ScreenToWorld(screen)), screen, 0.01f));
        }
      }
    }
  }
}

void TopDownOrientation()
{
  const Camera camera = MakeCamera(CameraView::TopDown, 0, 0.1f);
  const Point center = camera.WorldToScreen(camera.Center());
  REQUIRE(Near(center, camera.Viewport().Center()));
  // +x is right and +y is up on screen; 1 km is 100 pixels at 0.1 px/m.
  REQUIRE(Near(camera.WorldToScreen({2500.0f, -700.0f}), center + Point{100.0f, 0.0f}));
  REQUIRE(Near(camera.WorldToScreen({1500.0f, 300.0f}), center + Point{0.0f, -100.0f}));
  REQUIRE(Near(camera.ScreenHeading(1.0f), 1.0f));
}

void RotationSteps()
{
  Camera camera = MakeCamera(CameraView::TopDown, 0, 0.1f);
  camera.Rotate(2); // the view turns a quarter: world +x now points down the screen
  const Point center = camera.Viewport().Center();
  REQUIRE(camera.RotationStep() == 2);
  REQUIRE(Near(camera.WorldToScreen({2500.0f, -700.0f}), center + Point{0.0f, 100.0f}, 0.01f));
  REQUIRE(Near(camera.ScreenHeading(0.0f), -kPi * 0.5f, 1e-5f));
  camera.Rotate(-3);
  REQUIRE(camera.RotationStep() == Camera::kRotationSteps - 1);
  camera.Rotate(9);
  REQUIRE(camera.RotationStep() == 0);
}

void ZoomKeepsTheAnchor()
{
  for (const int rotation : {0, 3}) {
    Camera camera = MakeCamera(CameraView::TopDown, rotation, 0.05f);
    const Point anchor{300.0f, 200.0f};
    const Vec2 before = camera.ScreenToWorld(anchor);
    camera.ZoomAt(anchor, 2.0f);
    REQUIRE(Near(camera.Zoom(), 0.1f, 1e-6f));
    const Vec2 after = camera.ScreenToWorld(anchor);
    REQUIRE(Near(before.x, after.x, 0.05f) && Near(before.y, after.y, 0.05f));
  }
}

void ZoomLimitsAndLook()
{
  Camera camera = MakeCamera(CameraView::TopDown, 0, 0.05f);
  camera.SetZoom(100.0f);
  REQUIRE(Near(camera.Zoom(), Camera::kMaxZoom, 1e-6f));
  camera.SetZoom(0.0f);
  REQUIRE(Near(camera.Zoom(), Camera::kMinZoom, 1e-6f));
  REQUIRE(camera.Look() == ZoomLook::Tactical);
  camera.SetZoom(Camera::kDetailedZoom);
  REQUIRE(camera.Look() == ZoomLook::Detailed);
  // On a denser display the same look needs proportionally more device pixels per meter.
  camera.SetUiScale(2.0f);
  REQUIRE(Near(camera.Zoom(), Camera::kDetailedZoom * 2.0f, 1e-6f));
  REQUIRE(camera.Look() == ZoomLook::Detailed);
}

void PanFollowsTheCursor()
{
  Camera camera = MakeCamera(CameraView::TopDown, 1, 0.05f);
  const Point grab{500.0f, 300.0f};
  const Vec2 world = camera.ScreenToWorld(grab);
  camera.PanPixels({40.0f, -25.0f});
  REQUIRE(Near(camera.WorldToScreen(world), grab + Point{40.0f, -25.0f}, 0.01f));
}

void FramingContainsTheBox()
{
  for (const int rotation : {0, 1, 2}) {
    Camera camera = MakeCamera(CameraView::TopDown, rotation, 0.05f);
    const WorldBox box{{-9000.0f, -2000.0f}, {7000.0f, 5000.0f}};
    camera.FrameBox(box);
    for (const Vec2 corner : {box.min, box.max, Vec2{box.min.x, box.max.y}, Vec2{box.max.x, box.min.y}}) {
      REQUIRE(camera.Viewport().Contains(camera.WorldToScreen(corner)));
    }
    const WorldBox visible = camera.VisibleBox();
    REQUIRE(visible.min.x <= box.min.x && visible.max.x >= box.max.x);
    REQUIRE(visible.min.y <= box.min.y && visible.max.y >= box.max.y);
  }
}

} // namespace

int main()
{
  RoundTrips();
  TopDownOrientation();
  RotationSteps();
  ZoomKeepsTheAnchor();
  ZoomLimitsAndLook();
  PanFollowsTheCursor();
  FramingContainsTheBox();
  std::puts("battle_ui camera tests passed");
}
