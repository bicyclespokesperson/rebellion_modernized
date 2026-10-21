// The angled view and the presentation added with it: projection with heights, framing into the battle area, the
// view toggle, foreshortened plane shapes, depth-sorted art, picking raised fighters, orders through the projection,
// crowd labelling and label overlap, the bolt palette, escape pods, visible damage and planet types.
#include <algorithm>
#include <string_view>

#include "battle_ui/command_hud.h"
#include "battle_ui/plane_shapes.h"
#include "battle_ui/world_painter.h"
#include "battle_ui_screen_driver.h"

using namespace reb::battle_ui;
using namespace reb::battle_ui::test;
using reb::battle::Side;

namespace
{

class AllSprites : public SpriteCatalog
{
  public:
  bool Has(int32_t typeIndex, SpriteView) const override
  {
    return typeIndex >= 0;
  }
  float LengthMeters(int32_t) const override
  {
    return 800.0f;
  }
};

template <class T, class Predicate>
size_t CountIf(const DrawList& list, Predicate predicate)
{
  return static_cast<size_t>(std::ranges::count_if(list.Commands(), [&](const DrawCommand& command) {
    const T* drawn = std::get_if<T>(&command);
    return drawn && predicate(*drawn);
  }));
}

bool HasText(const DrawList& list, std::string_view text)
{
  return CountIf<DrawText>(list,
                           [text](const DrawText& t) {
                             return t.text == text;
                           })
         > 0;
}

Camera AngledCamera()
{
  Camera camera;
  camera.SetViewport({0.0f, 0.0f, 1280.0f, 720.0f});
  camera.SetView(CameraView::Angled);
  camera.SetZoom(0.1f);
  return camera;
}

void ProjectionWithHeights()
{
  const Camera angled = AngledCamera();
  const Point ground = angled.WorldToScreen({500.0f, 200.0f});
  const Point raised = angled.WorldToScreen({500.0f, 200.0f}, 100.0f);
  REQUIRE(Near(raised.x, ground.x) && Near(ground.y - raised.y, 100.0f * 0.1f * std::cos(Camera::kAngledPitch), 1e-3f));
  // The plane under a raised point is found by casting onto the plane, not through the height.
  const Vec2 back = angled.ScreenToWorld(ground);
  REQUIRE(Near(back.x, 500.0f, 0.05f) && Near(back.y, 200.0f, 0.05f));
  REQUIRE(Near(angled.VerticalScale(), std::sin(Camera::kAngledPitch)));

  Camera top = angled;
  top.SetView(CameraView::TopDown);
  REQUIRE(Near(top.WorldToScreen({500.0f, 200.0f}, 100.0f), top.WorldToScreen({500.0f, 200.0f})));
  for (const float heading : {0.3f, 1.2f, -2.0f}) {
    REQUIRE(Near(top.ProjectedAngle(heading), top.ScreenHeading(heading), 1e-5f));
  }
  // Foreshortened toward the horizontal, but straight up stays straight up.
  REQUIRE(Near(angled.ProjectedAngle(kPi * 0.5f), kPi * 0.5f, 1e-5f));
  REQUIRE(angled.ProjectedAngle(kPi * 0.25f) < kPi * 0.25f && angled.ProjectedAngle(kPi * 0.25f) > 0.0f);
}

void FramingIntoAnArea()
{
  Camera camera = AngledCamera();
  const Rect area{300.0f, 50.0f, 600.0f, 400.0f};
  const WorldBox box{{-4000.0f, -2000.0f}, {6000.0f, 3000.0f}};
  camera.FrameBoxInto(box, area);
  REQUIRE(Near(camera.WorldToScreen({1000.0f, 500.0f}), area.Center(), 0.5f));
  for (const Vec2 corner : {box.min, box.max, Vec2{box.min.x, box.max.y}, Vec2{box.max.x, box.min.y}}) {
    REQUIRE(area.Inset(-1.0f).Contains(camera.WorldToScreen(corner)));
  }
}

void PlaneShapesForeshorten()
{
  const Camera angled = AngledCamera();
  DrawList list;
  PlaneCircle(list, angled, {0.0f, 0.0f}, 1000.0f, Rgb(0xffffff), Stroke{});
  const auto* ellipse = std::get_if<DrawPolyline>(&list.Commands().back());
  REQUIRE(ellipse && ellipse->closed);
  float left = 1e9f, right = -1e9f, top = 1e9f, bottom = -1e9f;
  for (const Point& p : ellipse->points) {
    left = std::min(left, p.x);
    right = std::max(right, p.x);
    top = std::min(top, p.y);
    bottom = std::max(bottom, p.y);
  }
  REQUIRE(Near(right - left, 200.0f, 0.5f));
  REQUIRE(Near(bottom - top, 200.0f * std::sin(Camera::kAngledPitch), 0.5f));

  Camera top2 = angled;
  top2.SetView(CameraView::TopDown);
  DrawList topList;
  PlaneSector(topList, top2, {0.0f, 0.0f}, 0.0f, 1000.0f, 0.2f, 1.0f, Rgb(0xffffff));
  REQUIRE(std::holds_alternative<DrawSector>(topList.Commands().back())); // exact circles top-down
  DrawList angledList;
  PlaneSector(angledList, angled, {0.0f, 0.0f}, 500.0f, 1000.0f, 0.0f, kTwoPi, Rgb(0xffffff));
  REQUIRE(std::holds_alternative<DrawTriangles>(angledList.Commands().back())); // a projected ring
}

void ToggleKeepsTheView()
{
  ScreenDriver d;
  const Point areaCenter = d.screen.Layout().battleArea.Center();
  const Vec2 before = d.screen.GetCamera().ScreenToWorld(areaCenter);
  REQUIRE(!d.screen.AngledView());
  d.Press(Key::V);
  REQUIRE(d.screen.AngledView());
  const Vec2 after = d.screen.GetCamera().ScreenToWorld(areaCenter);
  REQUIRE(Near(before.x, after.x, 1.0f) && Near(before.y, after.y, 1.0f));
  d.Press(Key::Q); // rotation works in the angled view as well
  REQUIRE(d.screen.GetCamera().RotationStep() == 1);
  d.Press(Key::V);
  REQUIRE(!d.screen.AngledView() && d.screen.GetCamera().RotationStep() == 1);
}

void AngledArtIsDepthSorted()
{
  RecordingSource source;
  // Two ships at the same x: the one further up the plane (+y) is farther from the camera.
  source.View().units[0].position = {0.0f, -2000.0f};
  source.View().units[1].position = {0.0f, 2000.0f};
  FixedTextMeasurer text;
  AllSprites sprites;
  BattleScreen screen(source, text, sprites);
  screen.HandleInput(ScreenChanged{ScreenInfo{1280, 720, 1.0f, 1.0f}});
  screen.SetOverlay(false);
  screen.ToggleView();
  screen.GetCamera().SetZoom(0.12f);
  screen.CenterAt({0.0f, 0.0f});
  screen.Update(0.0);
  DrawList frame;
  screen.Build(frame);
  std::vector<float> order;
  for (const DrawCommand& command : frame.Commands()) {
    if (const auto* sprite = std::get_if<DrawSprite>(&command); sprite && sprite->typeIndex != 29) {
      REQUIRE(sprite->view == SpriteView::Angled);
      order.push_back(sprite->center.y);
    }
  }
  REQUIRE(order.size() >= 2 && std::ranges::is_sorted(order)); // farther (higher on screen) first
  // Fighters are drawn above the plane: the squadron's craft sprites sit higher than their plane positions.
  const Point plane = screen.GetCamera().WorldToScreen(source.View().craft[0].position);
  REQUIRE(CountIf<DrawSprite>(frame,
                              [&](const DrawSprite& s) {
                                return s.typeIndex == 29 && s.tint.a == 255 && s.center.y < plane.y - 5.0f;
                              })
          >= 1);
}

void PickingAndOrdersThroughTheProjection()
{
  ScreenDriver d;
  d.Press(Key::V);
  d.Frame();
  // A squadron is picked where it is drawn, above its spot on the plane.
  const Camera& camera = d.screen.GetCamera();
  const Point raised = camera.WorldToScreen(d.source.View().units[3].position, Camera::kFlightHeight);
  d.Click(raised);
  REQUIRE((d.Selected() == std::vector<reb::battle::UnitIndex>{3}));
  d.Click(d.At(0));
  const Point plane = camera.WorldToScreen({-1000.0f, -500.0f});
  d.Click(plane, MouseButton::Right);
  const auto* move = std::get_if<reb::battle::OrderMove>(&d.source.commands.back().order);
  REQUIRE(move && Near(move->position.x, -1000.0f, 40.0f) && Near(move->position.y, -500.0f, 40.0f));
  d.Drag(plane, plane + Point{0.0f, -60.0f}, MouseButton::Right); // up the screen is still +y on the plane
  move = std::get_if<reb::battle::OrderMove>(&d.source.commands.back().order);
  REQUIRE(move && move->facing && Near(*move->facing, kPi * 0.5f, 0.01f));
}

void CrowdsLabelKeyUnitsOnly()
{
  RecordingSource source;
  auto& units = source.View().units;
  for (int i = 0; i < 30; ++i) {
    units.push_back(Unit(10 + i, Side::Alliance, reb::battle::ViewUnitKind::CapitalShip, 5,
                         {-3000.0f + 200.0f * static_cast<float>(i), 2500.0f}, 90.0f, 0));
  }
  FixedTextMeasurer text;
  NoSprites sprites;
  BattleScreen screen(source, text, sprites);
  screen.HandleInput(ScreenChanged{ScreenInfo{1280, 720, 1.0f, 1.0f}});
  screen.GetCamera().SetZoom(Camera::kDetailedZoom * 1.5f);
  screen.CenterAt({0.0f, 2000.0f});
  std::vector<reb::battle::UnitIndex> all{0, 1};
  for (int i = 0; i < 30; ++i) {
    all.push_back(10 + i);
  }
  screen.GetSelection().Set(all);
  screen.Update(0.0);
  DrawList frame;
  screen.Build(frame);
  // Name labels under ships (the info box lists the selection too, left-aligned).
  const auto labelled = [&](std::string_view name) {
    return CountIf<DrawText>(frame,
                             [&](const DrawText& t) {
                               return t.text == name && t.align == Align::Center;
                             })
           > 0;
  };
  REQUIRE(labelled("Unit 0"));   // the group's leader
  REQUIRE(!labelled("Unit 1"));  // selected, but in a large selection
  REQUIRE(!labelled("Unit 25")); // crowded: not every ship on screen
}

// Two selected ships whose names would overlap: the first keeps its label; hovering the other gives it the space.
void OverlappingLabelsGiveWay()
{
  ScreenDriver d;
  auto& units = d.source.View().units;
  for (const int index : {60, 61}) {
    units.push_back(Unit(index, Side::Alliance, reb::battle::ViewUnitKind::CapitalShip, 5,
                         {20000.0f + 300.0f * static_cast<float>(index - 60), 20000.0f}, 50.0f, 0));
  }
  d.screen.GetCamera().SetZoom(Camera::kDetailedZoom * 1.5f);
  d.screen.CenterAt({20150.0f, 20000.0f});
  d.screen.GetSelection().Set({60, 61});
  const auto labelled = [&](const DrawList& frame, std::string_view name) {
    return CountIf<DrawText>(frame,
                             [&](const DrawText& t) {
                               return t.text == name && t.align == Align::Center;
                             })
           > 0;
  };
  DrawList& before = d.Frame();
  REQUIRE(labelled(before, "Unit 60") && !labelled(before, "Unit 61"));
  d.Move(d.At(61));
  DrawList& hovered = d.Frame();
  REQUIRE(labelled(hovered, "Unit 61") && !labelled(hovered, "Unit 60"));
}

void BoltsFollowThePalette()
{
  ScreenDriver d;
  reb::battle::ViewShot shot;
  shot.side = Side::Alliance;
  shot.from = {0.0f, 0.0f};
  shot.to = {6000.0f, 0.0f};
  shot.progress = 0.5f;
  d.source.View().shots = {shot};
  const Theme accessible = HudTheme(true);
  REQUIRE(accessible.allianceBolt != DefaultTheme().allianceBolt && accessible.ionBolt != DefaultTheme().ionBolt);
  d.screen.SetTheme(accessible);
  d.Frame();
  REQUIRE(CountIf<DrawPolyline>(d.frame,
                                [&](const DrawPolyline& l) {
                                  return l.blend == Blend::Additive
                                         && l.color.WithAlpha(255) == accessible.allianceBolt;
                                })
          == 1);
}

void PodsAndDamage()
{
  ScreenDriver d;
  d.screen.SetOverlay(false);
  d.screen.GetCamera().SetZoom(0.2f);
  d.screen.CenterAt({0.0f, 0.0f});
  d.Frame();
  const size_t calm = d.frame.Commands().size();
  auto& ship = d.source.View().units[0];
  ship.criticals.speed = 2;
  ship.criticals.weaponRecharge = 1;
  ship.hullFraction = 0.2f;
  d.Frame();
  REQUIRE(d.frame.Commands().size() > calm + 5); // sputter, smoke, scorch and fires

  reb::battle::ViewEffect blast;
  blast.kind = reb::battle::ViewEffectKind::Explosion;
  blast.position = d.source.View().units[2].position;
  blast.size = 900.0f;
  blast.unit = 2;
  blast.age = 1.0f;
  d.source.View().effects = {blast};
  d.Frame();
  const size_t withPods = d.frame.Count<DrawSector>();
  d.source.View().effects[0].unit = 3; // a squadron's explosion: no pods
  d.source.View().effects[0].size = 60.0f;
  d.Frame();
  REQUIRE(withPods > d.frame.Count<DrawSector>() + 8);
}

void PlanetTypes()
{
  reb::battle::ViewModel view;
  view.planetCenter = {0.0f, 0.0f};
  view.planetRadius = 6000.0f;
  FixedTextMeasurer text;
  NoSprites sprites;
  Selection selection;
  const std::vector<reb::battle::ViewUnitDetail> details;
  const std::vector<std::string> names;
  Camera camera = AngledCamera();
  const auto planet = [&](uint32_t picture, bool overlay) {
    DrawList frame;
    PaintWorld(frame, WorldScene{view, camera, sprites, DefaultTheme(), 0.0, selection, reb::battle::kNoUnit,
                                 overlay, Side::Alliance, details, names, picture, &text});
    return frame;
  };
  const auto body = [](const DrawList& frame, Color color) -> const DrawTriangles* {
    for (const DrawCommand& command : frame.Commands()) {
      if (const auto* triangles = std::get_if<DrawTriangles>(&command);
          triangles && !triangles->vertices.empty()
          && std::ranges::all_of(triangles->vertices, [color](const Vertex& v) { return v.color == color; })) {
        return triangles;
      }
    }
    return nullptr;
  };
  for (const CameraView angle : {CameraView::TopDown, CameraView::Angled}) {
    camera.SetView(angle);
    for (const float zoom : {Camera::kMinZoom, Camera::kDetailedZoom * 1.1f, Camera::kMaxZoom}) {
      camera.SetZoom(zoom);
      for (const bool overlay : {false, true}) {
        const DrawList terran = planet(1, overlay);
        const DrawList ringed = planet(21, overlay);
        const DrawList volcanic = planet(24, overlay);
        const DrawList asteroid = planet(16, overlay);
        const DrawTriangles* sphere = body(terran, Rgb(0x2f5f9c));
        REQUIRE(sphere && sphere->vertices.size() >= 48 * 3 && sphere->vertices.size() <= 720 * 3);
        REQUIRE(body(volcanic, Rgb(0xa8401e)));
        REQUIRE(body(asteroid, Rgb(0x5a5a5a)));
        REQUIRE(ringed.Count<DrawTriangles>() > terran.Count<DrawTriangles>());
        float left = 1e9f, right = -1e9f, top = 1e9f, bottom = -1e9f;
        for (const Vertex& vertex : sphere->vertices) {
          left = std::min(left, vertex.position.x);
          right = std::max(right, vertex.position.x);
          top = std::min(top, vertex.position.y);
          bottom = std::max(bottom, vertex.position.y);
        }
        REQUIRE(Near(right - left, bottom - top, camera.PixelsFor(view.planetRadius) * 0.002f));
        const bool tactical = overlay || camera.Look() == ZoomLook::Tactical;
        REQUIRE((CountIf<DrawPolygon>(terran, [](const DrawPolygon& polygon) {
                   return polygon.color == Rgb(0x020610, 115);
                 }) > 0) == tactical);
      }
    }
  }
  camera.SetZoom(Camera::kMinZoom);
  camera.SetCenter({1000000.0f, 1000000.0f});
  REQUIRE(!body(planet(1, false), Rgb(0x2f5f9c)));
  view.planetRadius = 0.0f;
  camera.SetCenter({0.0f, 0.0f});
  REQUIRE(!body(planet(1, false), Rgb(0x2f5f9c)));
}

void GravityWellsIdentifyProjectorsWithoutOverlappingLabels()
{
  reb::battle::ViewModel view;
  std::vector<std::string> names;
  for (int i = 0; i < 6; ++i) {
    names.push_back("Projector " + std::to_string(i + 1));
    view.wells.push_back({i, {0.0f, 0.0f}, 1500.0f, i % 2 == 0 ? Side::Alliance : Side::Empire, i != 5});
  }
  FixedTextMeasurer text;
  NoSprites sprites;
  Selection selection;
  const std::vector<reb::battle::ViewUnitDetail> details;
  Camera camera = AngledCamera();
  const auto wells = [&] {
    DrawList frame;
    PaintWorld(frame, WorldScene{view, camera, sprites, DefaultTheme(), 0.0, selection, reb::battle::kNoUnit,
                                 false, Side::Alliance, details, names, 0, &text});
    return frame;
  };
  for (const CameraView angle : {CameraView::TopDown, CameraView::Angled}) {
    camera.SetView(angle);
    const DrawList frame = wells();
    std::vector<Rect> labels;
    for (int i = 0; i < 6; ++i) {
      REQUIRE(HasText(frame, std::string(i % 2 == 0 ? "YOUR" : "ENEMY") + " GRAVITY WELL · " + names[i]));
    }
    REQUIRE(HasText(frame, "Enemy ships inside cannot jump to hyperspace"));
    REQUIRE(HasText(frame, "Your ships inside cannot jump to hyperspace"));
    REQUIRE(HasText(frame, "Projector down · this well is inactive"));
    for (const DrawCommand& command : frame.Commands()) {
      const auto* label = std::get_if<DrawText>(&command);
      if (!label) {
        continue;
      }
      const float width = label->maxWidth > 0.0f
                              ? std::min(label->maxWidth, text.Width(label->text, label->size, label->weight))
                              : text.Width(label->text, label->size, label->weight);
      const float x = label->position.x - (label->align == Align::Center ? width * 0.5f : 0.0f);
      const Rect bounds{x, label->position.y, width, text.LineHeight(label->size)};
      REQUIRE(camera.Viewport().Contains({bounds.x, bounds.y}));
      REQUIRE(camera.Viewport().Contains({bounds.Right(), bounds.Bottom()}));
      for (const Rect& previous : labels) {
        REQUIRE(Intersection(previous, bounds).Empty());
      }
      REQUIRE(CountIf<DrawRect>(frame, [&](const DrawRect& backing) {
                return backing.lineWidth == 0.0f && backing.color.a > 100
                       && backing.rect.Contains(bounds.Center());
              }) > 0);
      labels.push_back(bounds);
    }
    REQUIRE(labels.size() == 12);
    if (angle == CameraView::TopDown) {
      REQUIRE(CountIf<DrawArc>(frame, [](const DrawArc& ripple) {
                return ripple.radius > 50.0f && ripple.radius < 140.0f && ripple.color.a > 0
                       && ripple.stroke.dash == 0.0f;
              }) >= 5);
      REQUIRE(CountIf<DrawArc>(frame, [](const DrawArc& ring) {
                return Near(ring.radius, 150.0f) && ring.stroke.dash == 0.0f;
              }) >= 5);
    }
  }
  camera.SetView(CameraView::TopDown);
  camera.SetCenter({0.0f, -3000.0f});
  const DrawList partial = wells();
  for (int i = 0; i < 6; ++i) {
    REQUIRE(HasText(partial, std::string(i % 2 == 0 ? "YOUR" : "ENEMY") + " GRAVITY WELL · " + names[i]));
  }
  camera.SetCenter({0.0f, -10000.0f});
  const DrawList offscreen = wells();
  REQUIRE(offscreen.Count<DrawText>() == 0);
}

} // namespace

int main()
{
  ProjectionWithHeights();
  FramingIntoAnArea();
  PlaneShapesForeshorten();
  ToggleKeepsTheView();
  AngledArtIsDepthSorted();
  PickingAndOrdersThroughTheProjection();
  CrowdsLabelKeyUnitsOnly();
  OverlappingLabelsGiveWay();
  BoltsFollowThePalette();
  PodsAndDamage();
  PlanetTypes();
  GravityWellsIdentifyProjectorsWithoutOverlappingLabels();
  std::puts("battle_ui angled view tests passed");
}
