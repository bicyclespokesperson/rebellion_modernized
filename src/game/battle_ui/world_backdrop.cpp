// Space behind the battle: nebulae, stars and the system's planet; and the plane's grid.
#include <array>
#include <tuple>

#include "battle_ui/world_painter_internal.h"

namespace reb::battle_ui::painter
{
namespace
{

// ---- nebulae and stars ----------------------------------------------------------------------------------------

// Soft procedural nebulae behind the stars in the detailed look: a few large faint blobs per sky tile, drifting with
// a slower parallax than the stars.
void PaintNebulae(const Painter& p)
{
  static constexpr std::array<uint32_t, 4> kTints{0x2a3c8c, 0x5a2a78, 0x1f6070, 0x3a2050};
  constexpr int kSegments = 40;
  const Rect& viewport = p.camera.Viewport();
  const float tile = 2600.0f * p.ui;
  const float parallax = 0.0015f * p.ui;
  const Vec2 center = p.camera.Center();
  const float startX = viewport.x - Modulo(center.x * parallax, tile) - tile;
  const float startY = viewport.y - Modulo(-center.y * parallax, tile) - tile;
  std::vector<Vertex> vertices;
  for (float ty = startY; ty < viewport.Bottom() + tile; ty += tile) {
    for (float tx = startX; tx < viewport.Right() + tile; tx += tile) {
      for (uint32_t i = 0; i < 3; ++i) {
        const uint32_t seed = 4099U + i * 131U;
        const Point at{tx + Unit01(seed) * tile, ty + Unit01(seed + 1) * tile};
        const float radius = (500.0f + Unit01(seed + 2) * 700.0f) * p.ui;
        if (at.x + radius < viewport.x || at.x - radius > viewport.Right() || at.y + radius < viewport.y
            || at.y - radius > viewport.Bottom()) {
          continue;
        }
        const Color core
            = Rgb(kTints[Hash(seed + 3) % kTints.size()], static_cast<uint8_t>(16 + Unit01(seed + 4) * 14));
        const Color edge = core.WithAlpha(0);
        const float squash = 0.55f + 0.3f * Unit01(seed + 5);
        const auto rim = [&](float angle) {
          return Point{at.x + std::cos(angle) * radius, at.y + std::sin(angle) * radius * squash};
        };
        for (int k = 0; k < kSegments; ++k) {
          const float a0 = kTwoPi * static_cast<float>(k) / kSegments;
          const float a1 = kTwoPi * static_cast<float>(k + 1) / kSegments;
          vertices.insert(vertices.end(), {{at, core}, {rim(a0), edge}, {rim(a1), edge}});
        }
      }
    }
  }
  p.out.Triangles(std::move(vertices));
}

void PaintStars(const Painter& p)
{
  const Rect& viewport = p.camera.Viewport();
  const float tile = 900.0f * p.ui;
  const float parallax = 0.004f * p.ui;
  const Vec2 center = p.camera.Center();
  const float startX = viewport.x - Modulo(center.x * parallax, tile);
  const float startY = viewport.y - Modulo(-center.y * parallax, tile);
  const int perTile = p.detailed ? 150 : 70;
  std::vector<Vertex> vertices;
  for (float ty = startY; ty < viewport.Bottom(); ty += tile) {
    for (float tx = startX; tx < viewport.Right(); tx += tile) {
      for (int i = 0; i < perTile; ++i) {
        const uint32_t seed = static_cast<uint32_t>(i) * 7919U;
        const Point at{tx + Unit01(seed) * tile, ty + Unit01(seed + 1) * tile};
        if (!viewport.Contains(at)) {
          continue;
        }
        const float brightness = Unit01(seed + 2);
        const auto alpha = static_cast<uint8_t>((p.detailed ? 70.0f : 40.0f) + brightness * brightness * 150.0f);
        const Color tint = brightness > 0.9f ? Rgb(0xcfe0ff, alpha) : Rgb(0xffffff, alpha);
        Quad(vertices, at, (brightness > 0.93f ? 1.1f : 0.6f) * p.ui, tint);
      }
    }
  }
  p.out.Triangles(std::move(vertices));
}

// ---- the planet ------------------------------------------------------------------------------------------------

// How a planet type is drawn: a base colour with surface patches, clouds, bands or a ring. Authored by eye after the
// 26 encyclopedia planet pictures (EDATA.166-191, assumed to be pictures 1-26); no pixels are taken from them.
struct PlanetLook {
  uint32_t base;
  uint32_t patch;
  uint32_t atmosphere; // 0: none
  uint8_t patches;
  uint8_t clouds;
  bool bands = false;
  bool ring = false;
  bool irregular = false; // an asteroid
};

constexpr PlanetLook kTerran{0x2f5f9c, 0x5f8f4a, 0x8cb4ff, 7, 6};
constexpr PlanetLook kTerranArid{0x3a6a98, 0xb09a70, 0x9cbcff, 7, 5};
constexpr PlanetLook kOcean{0x1d4f86, 0x2f7f7a, 0x7aaaf0, 4, 7};
constexpr PlanetLook kDeepOcean{0x123c50, 0x1f6040, 0x6a9ad0, 6, 3};
constexpr PlanetLook kJungle{0x2a3a30, 0x5a4a6a, 0x8aa080, 9, 2};
constexpr PlanetLook kSwamp{0x3a6a60, 0x5a7a40, 0x8ac0a0, 8, 2};
constexpr PlanetLook kMire{0x3a3a2a, 0x2a4a3a, 0x90a080, 9, 1};
constexpr PlanetLook kDesert{0xb09a4a, 0x8a7a3a, 0xe0d0a0, 8, 1};
constexpr PlanetLook kOchre{0xb08a2a, 0x8a5a1a, 0xe0c080, 7, 0};
constexpr PlanetLook kSpeckled{0xb0a050, 0x2a4a9a, 0xd0d0b0, 9, 0};
constexpr PlanetLook kBarren{0x6a6a6a, 0x3a3a3a, 0, 10, 0};
constexpr PlanetLook kCracked{0x7a808c, 0x3a4050, 0x9aa8c0, 9, 0};
constexpr PlanetLook kIce{0xc0dce0, 0x8ab0b8, 0xd8f0ff, 6, 4};
constexpr PlanetLook kIceCloud{0x9ac0d4, 0xd8e8f0, 0xc8e4ff, 4, 8};
constexpr PlanetLook kVolcanic{0xa8401e, 0x6a2010, 0xff9a6a, 8, 0};
constexpr PlanetLook kDark{0x1a2a40, 0x4a3a2a, 0x5a7aa0, 9, 1};
constexpr PlanetLook kGasGiant{0xd07a40, 0xf0b070, 0xffc090, 0, 0, true};
constexpr PlanetLook kRinged{0xe0a050, 0xf0c880, 0xffd8a0, 0, 0, true, true};
constexpr PlanetLook kAsteroid{0x5a5a5a, 0x3a3a3a, 0, 6, 0, false, false, true};

const PlanetLook& LookForPicture(uint32_t picture)
{
  static constexpr std::array<const PlanetLook*, 26> kPictures{
      &kTerran, &kJungle,    &kDeepOcean, &kTerran, &kTerranArid, &kTerran,   &kOcean,    &kBarren,  &kDesert,
      &kSwamp,  &kTerran,    &kTerran,    &kTerran, &kMire,       &kGasGiant, &kAsteroid, &kDark,    &kIce,
      &kTerran, &kDeepOcean, &kRinged,    &kOchre,  &kCracked,    &kVolcanic, &kIceCloud, &kSpeckled};
  return picture >= 1 && picture <= kPictures.size() ? *kPictures[picture - 1] : kTerran;
}

std::vector<Point> Ellipse(Point center, float rx, float ry, float rotation = 0.0f, int segments = 14)
{
  std::vector<Point> points;
  const float c = std::cos(rotation);
  const float s = std::sin(rotation);
  for (int k = 0; k < segments; ++k) {
    const float a = kTwoPi * static_cast<float>(k) / static_cast<float>(segments);
    const float x = std::cos(a) * rx;
    const float y = std::sin(a) * ry;
    points.push_back({center.x + x * c - y * s, center.y + x * s + y * c});
  }
  return points;
}

// A soft-edged patch: opaque-ish in the middle, fading to nothing at its rim.
void SoftBlob(std::vector<Vertex>& vertices, Point at, float rx, float ry, float rotation, Color color)
{
  constexpr int kSegments = 16;
  const Color edge = color.WithAlpha(0);
  const std::vector<Point> rim = Ellipse(at, rx, ry, rotation, kSegments);
  for (int k = 0; k < kSegments; ++k) {
    vertices.insert(vertices.end(), {{at, color}, {rim[k], edge}, {rim[(k + 1) % kSegments], edge}});
  }
}

// Surface patches in clusters (continents, crater fields, storm belts), shrunk toward the limb as a sphere
// foreshortens them, with soft edges so that they read as terrain rather than paint.
void PaintSurface(const Painter& p, Point center, float radius, uint32_t seed, int clusters, Color color, float size,
                  float stretch)
{
  constexpr int kPerCluster = 9;
  std::vector<Vertex> vertices;
  for (int cluster = 0; cluster < clusters; ++cluster) {
    const uint32_t c = seed + static_cast<uint32_t>(cluster) * 977U;
    const Point heart = ScreenDirection(Unit01(c) * kTwoPi) * (std::sqrt(Unit01(c + 1)) * 0.75f);
    for (int i = 0; i < kPerCluster; ++i) {
      const uint32_t s = c + static_cast<uint32_t>(i) * 31U + 5U;
      const Point uv = heart + Point{Unit01(s) - 0.5f, Unit01(s + 1) - 0.5f} * (size * 2.2f);
      const float reach = uv.x * uv.x + uv.y * uv.y;
      if (reach > 0.9f) {
        continue;
      }
      const float foreshortening = std::sqrt(std::max(0.1f, 1.0f - reach));
      const float blob = radius * size * (0.45f + 0.8f * Unit01(s + 2)) * foreshortening;
      SoftBlob(vertices, center + uv * radius, blob * stretch, blob, (Unit01(s + 3) - 0.5f) * 0.5f, color);
    }
  }
  p.out.Triangles(std::move(vertices));
}

void PaintBands(const Painter& p, Point center, float radius, const PlanetLook& look, uint32_t seed)
{
  constexpr int kBands = 11;
  for (int band = 0; band < kBands; ++band) {
    const float y0 = -1.0f + 2.0f * static_cast<float>(band) / kBands;
    const float y1 = -1.0f + 2.0f * static_cast<float>(band + 1) / kBands;
    const float x0 = std::sqrt(std::max(0.0f, 1.0f - y0 * y0));
    const float x1 = std::sqrt(std::max(0.0f, 1.0f - y1 * y1));
    const float shade = Unit01(seed + static_cast<uint32_t>(band));
    const Color color = Mix(Rgb(look.base), Rgb(look.patch), shade).WithAlpha(static_cast<uint8_t>(120 + shade * 120));
    p.out.FillConvex({center + Point{-x0, y0} * radius, center + Point{x0, y0} * radius,
                      center + Point{x1, y1} * radius, center + Point{-x1, y1} * radius},
                     color);
  }
}

// Half of a ring tilted across the planet: the far half before the planet, the near half after it.
void PaintRing(const Painter& p, Point center, float radius, const PlanetLook& look, bool nearHalf)
{
  constexpr int kSegments = 48;
  constexpr float kTilt = -0.25f;
  const float c = std::cos(kTilt);
  const float s = std::sin(kTilt);
  const auto at = [&](float scale, float angle) {
    const float x = std::cos(angle) * radius * scale;
    const float y = std::sin(angle) * radius * scale * 0.28f;
    return center + Point{x * c - y * s, x * s + y * c};
  };
  const float start = nearHalf ? 0.0f : kPi;
  std::vector<Vertex> vertices;
  for (const auto& [inner, outer, alpha] :
       std::array<std::tuple<float, float, uint8_t>, 2>{{{1.35f, 1.75f, 150}, {1.8f, 2.05f, 90}}}) {
    const Color color = Rgb(look.patch, alpha);
    for (int k = 0; k < kSegments; ++k) {
      const float a0 = start + kPi * static_cast<float>(k) / kSegments;
      const float a1 = start + kPi * static_cast<float>(k + 1) / kSegments;
      vertices.insert(vertices.end(), {{at(inner, a0), color},
                                       {at(outer, a0), color},
                                       {at(outer, a1), color},
                                       {at(inner, a0), color},
                                       {at(outer, a1), color},
                                       {at(inner, a1), color}});
    }
  }
  p.out.Triangles(std::move(vertices));
}

std::vector<Point> AsteroidOutline(Point center, float radius, uint32_t seed)
{
  constexpr int kPoints = 18;
  std::vector<Point> points;
  for (int k = 0; k < kPoints; ++k) {
    const float angle = kTwoPi * static_cast<float>(k) / kPoints;
    const float reach = 0.72f + 0.28f * Unit01(seed + static_cast<uint32_t>(k));
    points.push_back(center
                     + Point{std::cos(angle) * radius * reach * 1.15f, -std::sin(angle) * radius * reach * 0.85f});
  }
  return points;
}

// Light from the upper left: a fan from the lit point (clear) to the rim (dark) puts the night side and the limb in
// shadow over whatever the surface shows.
void PaintShading(const Painter& p, Point center, const std::vector<Point>& rim, float radius)
{
  const Point lit = center + Point{-0.38f, -0.38f} * radius;
  const Color day = Rgb(0x000000, 0);
  const Color night = Rgb(0x02040a, 235);
  std::vector<Vertex> vertices;
  vertices.reserve(rim.size() * 3);
  for (size_t k = 0; k < rim.size(); ++k) {
    vertices.insert(vertices.end(), {{lit, day}, {rim[k], night}, {rim[(k + 1) % rim.size()], night}});
  }
  p.out.Triangles(std::move(vertices));
}

// A sphere stays round on screen in the angled view too; the tactical look subdues its surface under the symbols.
void PaintPlanet(const Painter& p)
{
  const battle::ViewModel& view = p.scene.view;
  const float radius = p.Px(view.planetRadius);
  if (view.planetRadius <= 0.0f || radius < 2.0f) {
    return;
  }
  const Point center = p.Screen(view.planetCenter);
  const Rect& viewport = p.camera.Viewport();
  if (center.x + radius * 2.2f < viewport.x || center.x - radius * 2.2f > viewport.Right()
      || center.y + radius * 2.2f < viewport.y || center.y - radius * 2.2f > viewport.Bottom()) {
    return;
  }
  const PlanetLook& look = LookForPicture(p.scene.systemPicture);
  const uint32_t seed = 0x9e3779b9U * (p.scene.systemPicture + 1);
  if (look.ring) {
    PaintRing(p, center, radius, look, false);
  }
  const int segments = std::clamp(static_cast<int>(radius / 5.0f), 48, 720);
  std::vector<Point> rim
      = look.irregular ? AsteroidOutline(center, radius, seed) : Ellipse(center, radius, radius, 0.0f, segments);
  std::vector<Vertex> body;
  for (size_t k = 0; k < rim.size(); ++k) {
    body.insert(body.end(),
                {{center, Rgb(look.base)}, {rim[k], Rgb(look.base)}, {rim[(k + 1) % rim.size()], Rgb(look.base)}});
  }
  p.out.Triangles(std::move(body));
  if (look.bands) {
    PaintBands(p, center, radius, look, seed);
  }
  PaintSurface(p, center, look.irregular ? radius * 0.8f : radius, seed + 11, look.patches / 2 + 1,
               Rgb(look.patch, 190), look.irregular ? 0.1f : 0.12f, 1.3f);
  if (look.clouds > 0) {
    PaintSurface(p, center, radius, seed + 23, look.clouds / 2 + 1, Rgb(0xf0f4ff, 110), 0.08f, 2.6f);
  }
  PaintShading(p, center, rim, radius);
  if (look.atmosphere != 0) {
    p.out.Ring(center, radius, radius + 2.5f * p.ui, Rgb(look.atmosphere, 110), Blend::Additive);
    p.out.Ring(center, radius + 2.5f * p.ui, radius + 14.0f * p.ui, Rgb(look.atmosphere, 24), Blend::Additive);
  }
  if (look.ring) {
    PaintRing(p, center, radius, look, true);
  }
  if (!p.detailed) {
    p.out.FillConvex(rim, Rgb(0x020610, 115));
    p.out.Polyline(rim, kGridMajor.WithAlpha(140), Stroke{1.0f * p.ui}, true);
  }
}

float GridSpacing(const Camera& camera, float minPixels)
{
  for (const float spacing : {250.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f}) {
    if (camera.PixelsFor(spacing) >= minPixels) {
      return spacing;
    }
  }
  return 40000.0f;
}

} // namespace

void PaintBackdrop(const Painter& p)
{
  p.out.FillRect(p.camera.Viewport(), p.detailed ? kSpaceDeep : kSpaceTactical);
  if (p.detailed) {
    PaintNebulae(p);
  }
  PaintStars(p);
  PaintPlanet(p);
}

void PaintGrid(const Painter& p)
{
  const battle::ViewModel& view = p.scene.view;
  const WorldBox visible = p.camera.VisibleBox();
  const Vec2 lo{std::max(visible.min.x, view.boundsMin.x), std::max(visible.min.y, view.boundsMin.y)};
  const Vec2 hi{std::min(visible.max.x, view.boundsMax.x), std::min(visible.max.y, view.boundsMax.y)};
  if (lo.x >= hi.x || lo.y >= hi.y) {
    return;
  }
  // The angled view foreshortens the plane, so its grid needs the spacing for the shorter (vertical) direction.
  const float minor = GridSpacing(p.camera, 26.0f * p.ui / p.camera.VerticalScale());
  const float major = minor * 5.0f;
  const float fade = p.detailed ? 0.45f : 1.0f;
  const Stroke thin{std::max(1.0f, p.ui)};
  for (float x = std::ceil(lo.x / minor) * minor; x <= hi.x; x += minor) {
    const bool isMajor = std::fmod(std::abs(x), major) < 1.0f;
    p.out.Line(p.Screen({x, lo.y}), p.Screen({x, hi.y}), (isMajor ? kGridMajor : kGridMinor).Faded(fade), thin);
  }
  for (float y = std::ceil(lo.y / minor) * minor; y <= hi.y; y += minor) {
    const bool isMajor = std::fmod(std::abs(y), major) < 1.0f;
    p.out.Line(p.Screen({lo.x, y}), p.Screen({hi.x, y}), (isMajor ? kGridMajor : kGridMinor).Faded(fade), thin);
  }
  const std::vector<Point> border{p.Screen(view.boundsMin), p.Screen({view.boundsMax.x, view.boundsMin.y}),
                                  p.Screen(view.boundsMax), p.Screen({view.boundsMin.x, view.boundsMax.y})};
  p.out.Polyline(border, kBounds.Faded(fade), Stroke{1.5f * p.ui}, true);
}

} // namespace reb::battle_ui::painter
