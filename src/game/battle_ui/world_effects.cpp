// Bolts, effects and visible damage (§10.3): bolts in the theme's colour code with muzzle flashes, explosions with
// debris and escape pods, shield flashes, hyperspace jumps, ion crackle, and critical damage that shows on the hull.
// Light is additive: glows are radial gradients to transparent, so overlapping fire brightens instead of covering.
#include <algorithm>

#include "battle_ui/world_painter_internal.h"
#include "space_battle/math2d.h"
#include "space_battle/tuning.h"

namespace reb::battle_ui::painter
{
namespace
{

using battle::ViewEffectKind;
using battle::ViewShotKind;

const Color kWhite = Rgb(0xffffff);
const Color kPod = Rgb(0xe8ecf2);
const Color kVent = Rgb(0xcfe4ff);
const Color kFire = Rgb(0xff9a3a);
const Color kEmber = Rgb(0xff6a1a);
const Color kSmoke = Rgb(0x0a0a0c);
const Color kDebris = Rgb(0x8a8e96);
const Color kHyperspace = Rgb(0xdcecff);

constexpr float kPodDelay = 0.25f;   // seconds after the blast before the pods clear it
constexpr float kMuzzleFlash = 0.1f; // of a bolt's flight, the share during which its muzzle flash shows

uint8_t Alpha(float value)
{
  return static_cast<uint8_t>(std::clamp(value, 0.0f, 255.0f));
}

float EffectLifetime(const battle::ViewEffect& effect)
{
  switch (effect.kind) {
    case ViewEffectKind::Explosion:
      return std::clamp(0.7f + effect.size / 450.0f, 0.7f, 3.2f);
    case ViewEffectKind::ShieldFlash:
      return 0.4f;
    case ViewEffectKind::HullHit:
      return 0.55f;
    case ViewEffectKind::CriticalHit:
      return 0.9f;
    case ViewEffectKind::Disruption:
      return 0.7f;
    case ViewEffectKind::Jump:
      return 1.0f;
    case ViewEffectKind::Superlaser:
      return 1.6f;
  }
  return 1.0f;
}

float HeightOf(const Painter& p, battle::UnitIndex unit)
{
  const ViewUnit* found = unit == battle::kNoUnit ? nullptr : FindUnit(p.scene.view, unit);
  return found && found->kind == ViewUnitKind::Squadron ? p.CraftHeight() : 0.0f;
}

// A streak of light from `tail` to `head`, `width` wide at the head and tapering to a point at the tail, shaded from
// transparent at the tail to `color` at the head.
void Streak(DrawList& out, Point tail, Point head, float width, Color color)
{
  const float length = Distance(tail, head);
  if (length < 0.5f) {
    return;
  }
  const Point dir = (head - tail) * (1.0f / length);
  const Point side{-dir.y * width * 0.5f, dir.x * width * 0.5f};
  const Color clear = color.WithAlpha(0);
  const Point nose = head + dir * (width * 0.6f);
  out.Triangles({{tail, clear},
                 {head + side, color},
                 {head - side, color},
                 {head + side, color},
                 {nose, color.WithAlpha(color.a / 3)},
                 {head - side, color}},
                Blend::Additive);
}

// Sparks thrown from `at`: short streaks that fly out and fade.
void Sparks(const Painter& p, Point at, uint32_t seed, int count, float reach, float t, Color color)
{
  const float fade = 1.0f - t;
  const float travel = 1.0f - (1.0f - t) * (1.0f - t);
  for (int i = 0; i < count; ++i) {
    const float angle = Unit01(seed + static_cast<uint32_t>(i) * 13U) * kTwoPi;
    const float speed = 0.45f + 0.55f * Unit01(seed + static_cast<uint32_t>(i) * 29U);
    const Point dir = ScreenDirection(angle);
    const Point head = at + dir * (reach * speed * travel);
    const Point tail = at + dir * (reach * speed * std::max(0.0f, travel - 0.35f));
    Streak(p.out, tail, head, 1.4f * p.ui, color.WithAlpha(Alpha(230.0f * fade)));
  }
}

// ---- bolts -----------------------------------------------------------------------------------------------------

struct BoltLook {
  float lengthMeters; // the streak's length in flight
  float minPx;        // on screen at least, and at most
  float maxPx;
  float core; // UI pixels
  float glow;
};

BoltLook LookFor(const battle::ViewShot& shot)
{
  const float planetary = shot.battery >= 0 ? 1.6f : 1.0f; // a planet's guns throw bigger bolts than a ship's
  switch (shot.kind) {
    case ViewShotKind::Laser:
      return {45.0f, 7.0f, 18.0f, 1.0f, 4.0f};
    case ViewShotKind::Ion:
      return {120.0f * planetary, 6.0f, 22.0f * planetary, 2.2f * planetary, 9.0f * planetary};
    case ViewShotKind::Turbolaser:
    case ViewShotKind::Torpedo:
      break;
  }
  return {170.0f * planetary, 12.0f, 44.0f * planetary, 1.7f * planetary, 6.5f * planetary};
}

void PaintMuzzleFlash(const Painter& p, Point at, Color color, float flash, float size)
{
  if (flash <= 0.0f) {
    return;
  }
  p.out.Glow(at, size * (0.7f + 0.6f * flash), color.WithAlpha(Alpha(190.0f * flash)));
  p.out.Glow(at, size * 0.35f, Mix(color, kWhite, 0.7f).WithAlpha(Alpha(240.0f * flash)));
}

void PaintTorpedo(const Painter& p, Point from, Point head, Point dir, Color color, double seconds)
{
  const Color hot = Mix(color, Rgb(0xffb070), 0.45f);
  const Point tail = head - dir * std::min(Distance(from, head), 34.0f * p.ui);
  Streak(p.out, tail, head, 2.0f * p.ui, hot.WithAlpha(90));
  p.out.Glow(head, 9.0f * p.ui, hot.WithAlpha(150));
  p.out.Glow(head, 3.5f * p.ui, Mix(hot, kWhite, 0.75f));
  // A four-point flare, turning as it flies.
  const float spin = static_cast<float>(seconds * 3.0);
  for (int i = 0; i < 2; ++i) {
    const Point arm = ScreenDirection(spin + static_cast<float>(i) * kPi * 0.5f) * (8.0f * p.ui);
    p.out.Line(head - arm, head + arm, Mix(hot, kWhite, 0.5f).WithAlpha(150), Stroke{1.0f * p.ui}, Blend::Additive);
  }
}

void PaintIonBolt(const Painter& p, Point from, Point head, Point dir, Color color, const BoltLook& look)
{
  const float trail = std::min(Distance(from, head), std::clamp(p.Px(look.lengthMeters), look.minPx * p.ui,
                                                                look.maxPx * p.ui));
  Streak(p.out, head - dir * trail, head, look.glow * 0.9f * p.ui, color.WithAlpha(110));
  p.out.Glow(head, look.glow * 1.3f * p.ui, color.WithAlpha(200));
  p.out.Glow(head, look.core * 1.6f * p.ui, Mix(color, kWhite, 0.8f));
}

void PaintBolt(const Painter& p, Point from, Point head, Point dir, Color color, const BoltLook& look)
{
  const float travelled = Distance(from, head);
  const float length = std::min(travelled, std::clamp(p.Px(look.lengthMeters), look.minPx * p.ui, look.maxPx * p.ui));
  const Point tail = head - dir * length;
  Streak(p.out, tail, head, look.glow * p.ui, color.WithAlpha(120));
  Streak(p.out, head - dir * (length * 0.85f), head, look.core * 1.8f * p.ui, color.WithAlpha(235));
  p.out.Line(head - dir * (length * 0.6f), head, Mix(color, kWhite, 0.7f), Stroke{look.core * p.ui}, Blend::Additive);
  p.out.Glow(head, look.glow * 0.9f * p.ui, color.WithAlpha(110));
}

// ---- impacts ---------------------------------------------------------------------------------------------------

// A destroyed capital ship ejects escape pods (a nod to the characters aboard being set adrift, §5.6): small capsules
// that leave the fireball in every direction with a faint thruster glow.
void PaintEscapePods(const Painter& p, const battle::ViewEffect& effect)
{
  if (effect.age < kPodDelay) {
    return;
  }
  const int pods = std::clamp(static_cast<int>(effect.size / 140.0f), 4, 10);
  const uint32_t seed = static_cast<uint32_t>(effect.unit + 1) * 40503U;
  const float flight = effect.age - kPodDelay;
  const float fade = std::clamp(1.0f - flight / 2.5f, 0.0f, 1.0f);
  for (int i = 0; i < pods; ++i) {
    const float angle = Unit01(seed + static_cast<uint32_t>(i)) * kTwoPi;
    const float speed = 160.0f + Unit01(seed + 17U * static_cast<uint32_t>(i)) * 220.0f; // m/s, cosmetic
    const float reach = effect.size * 0.35f + speed * flight;
    const Vec2 at{effect.position.x + std::cos(angle) * reach, effect.position.y + std::sin(angle) * reach};
    const Point screen = p.Screen(at);
    const Point back = p.Screen({at.x - std::cos(angle) * 60.0f, at.y - std::sin(angle) * 60.0f});
    Streak(p.out, back, screen, 2.0f * p.ui, kFire.WithAlpha(Alpha(150.0f * fade)));
    p.out.Glow(screen, 5.0f * p.ui, kFire.WithAlpha(Alpha(80.0f * fade)));
    p.out.FillCircle(screen, 1.8f * p.ui, kPod.WithAlpha(Alpha(240.0f * fade)));
  }
}

// Hull fragments tumbling away from the blast, glowing hot at first, then dark against the stars.
void PaintDebris(const Painter& p, Point center, float size, uint32_t seed, float t, int count)
{
  const float travel = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
  for (int i = 0; i < count; ++i) {
    const uint32_t salt = seed + static_cast<uint32_t>(i) * 977U;
    const float angle = Unit01(salt) * kTwoPi;
    const float speed = 0.35f + 0.95f * Unit01(salt + 1);
    const Point dir = ScreenDirection(angle);
    const Point at = center + dir * (size * speed * travel * 1.25f);
    const float chunk = std::max(1.2f * p.ui, size * (0.025f + 0.035f * Unit01(salt + 2)));
    const float spin = Unit01(salt + 3) * kTwoPi + t * (Unit01(salt + 4) - 0.5f) * 14.0f;
    const Color hot = Mix(Rgb(0xfff0c0), kEmber, std::min(1.0f, t * 2.5f));
    const Color body = Mix(hot, kDebris, std::min(1.0f, t * 1.6f)).WithAlpha(Alpha(255.0f * (1.0f - t * t)));
    const auto corner = [&](float a, float r) {
      return at + ScreenDirection(spin + a) * (chunk * r);
    };
    p.out.FillConvex({corner(0.0f, 1.0f), corner(2.1f, 0.7f), corner(3.6f, 0.9f), corner(5.0f, 0.55f)}, body);
    if (t < 0.6f) {
      const Point trail = at - dir * (size * 0.12f * (1.0f - t));
      Streak(p.out, trail, at, chunk * 1.2f, kEmber.WithAlpha(Alpha(200.0f * (1.0f - t / 0.6f))));
    }
  }
}

void PaintExplosion(const Painter& p, const battle::ViewEffect& effect, float t)
{
  const Point center = p.Screen(effect.position, HeightOf(p, effect.unit));
  const float size = std::max(p.Px(effect.size), 7.0f * p.ui);
  const float grow = 1.0f - (1.0f - t) * (1.0f - t);
  const float fade = 1.0f - t;
  const uint32_t seed = static_cast<uint32_t>(effect.unit + 1) * 2654435761U ^ static_cast<uint32_t>(effect.size);
  const ViewUnit* unit = effect.unit == battle::kNoUnit ? nullptr : FindUnit(p.scene.view, effect.unit);
  const bool capital = unit ? unit->kind == ViewUnitKind::CapitalShip : effect.size >= 250.0f;
  if (t < 1.0f) {
    // Smoke first, under the fire, so the fireball burns in front of its own pall.
    for (int i = 0; i < 4 && t > 0.15f; ++i) {
      const uint32_t salt = seed + 50U + static_cast<uint32_t>(i);
      const Point at = center + ScreenDirection(Unit01(salt) * kTwoPi) * (size * 0.35f * grow * Unit01(salt + 9));
      p.out.Glow(at, size * (0.45f + 0.45f * grow), kSmoke.WithAlpha(Alpha(150.0f * fade)), Blend::Alpha);
    }
    // The fireball: overlapping hot blobs, white-yellow cooling to orange and red.
    const Color heat = t < 0.3f ? Mix(Rgb(0xfff4d0), Rgb(0xffb040), t / 0.3f) : Mix(Rgb(0xffb040), Rgb(0xc8381a),
                                                                                    (t - 0.3f) / 0.7f);
    for (int i = 0; i < 6; ++i) {
      const uint32_t salt = seed + static_cast<uint32_t>(i) * 31U;
      const Point at = center + ScreenDirection(Unit01(salt) * kTwoPi) * (size * 0.3f * grow * Unit01(salt + 1));
      const float radius = size * (0.3f + 0.3f * Unit01(salt + 2)) * (0.35f + 0.65f * grow);
      p.out.Glow(at, radius, heat.WithAlpha(Alpha(230.0f * fade)));
    }
    p.out.Glow(center, size * 0.35f * (1.0f - 0.5f * t), Mix(heat, kWhite, 0.6f).WithAlpha(Alpha(255.0f * fade * fade)));
    // The flash and the shock wave.
    if (t < 0.25f) {
      const float flash = 1.0f - t / 0.25f;
      p.out.Glow(center, size * (0.9f + 1.5f * t), kWhite.WithAlpha(Alpha(230.0f * flash * flash)));
    }
    const float shock = size * (0.25f + 1.35f * grow);
    const Color front = Mix(kFire, kWhite, 0.4f).WithAlpha(Alpha(110.0f * fade * fade));
    p.out.GradientSector(center, shock * 0.8f, shock, 0.0f, kTwoPi, kFire.WithAlpha(0), front, Blend::Additive);
    p.out.GradientSector(center, shock, shock * 1.06f, 0.0f, kTwoPi, front, kFire.WithAlpha(0), Blend::Additive);
    PaintDebris(p, center, size, seed, t, capital ? 18 : 8);
    Sparks(p, center, seed + 7U, capital ? 14 : 8, size * 1.4f, t, Rgb(0xffe0a0));
    // Secondary blasts tear through a capital ship's hull after the first.
    if (capital) {
      for (int i = 0; i < 3; ++i) {
        const float start = 0.1f + 0.12f * static_cast<float>(i);
        const float local = (t - start) / 0.35f;
        if (local <= 0.0f || local >= 1.0f) {
          continue;
        }
        const uint32_t salt = seed + 300U + static_cast<uint32_t>(i);
        const Point at = center + ScreenDirection(Unit01(salt) * kTwoPi) * (size * (0.3f + 0.4f * Unit01(salt + 1)));
        p.out.Glow(at, size * 0.35f * (0.5f + local), Rgb(0xffd080).WithAlpha(Alpha(220.0f * (1.0f - local))));
      }
    }
  }
  if (capital) {
    PaintEscapePods(p, effect);
  }
}

void PaintShieldFlash(const Painter& p, const battle::ViewEffect& effect, float t)
{
  const ViewUnit* unit = FindUnit(p.scene.view, effect.unit);
  if (!unit) {
    return;
  }
  const float fade = 1.0f - t;
  const Point center = p.Screen(unit->position, HeightOf(p, effect.unit));
  const Point toImpact = p.Screen(effect.position) - center;
  const float angle = std::atan2(-toImpact.y, toImpact.x);
  const float radius = std::max(DrawnRadiusPx(p, *unit) * 0.92f, 10.0f * p.ui);
  const Color color = p.scene.theme.shield;
  // The bubble lights where it is struck: a band brightening toward the rim, a bright edge and a ripple spreading.
  const float spread = 0.55f + 0.5f * t;
  constexpr int kSlices = 9; // the band fades toward its ends as well as inward
  for (int i = 0; i < kSlices; ++i) {
    const float weight = std::sin(kPi * (static_cast<float>(i) + 0.5f) / kSlices);
    const float from = angle - spread + 2.0f * spread * static_cast<float>(i) / kSlices;
    p.out.GradientSector(center, radius * 0.72f, radius, from, 2.0f * spread / kSlices, color.WithAlpha(0),
                         color.WithAlpha(Alpha(130.0f * fade * weight)), Blend::Additive);
  }
  p.out.Arc(center, radius, angle - spread * 0.8f, 1.6f * spread, Mix(color, kWhite, 0.45f).WithAlpha(Alpha(220.0f * fade)),
            Stroke{1.5f * p.ui, 0, 0, 0, 2.5f * p.ui}, Blend::Additive);
  p.out.Arc(center, radius * (1.0f + 0.1f * t), angle - spread * 1.2f, 2.4f * spread,
            color.WithAlpha(Alpha(70.0f * fade)), Stroke{1.0f * p.ui, 0, 0, 0, 2.0f * p.ui}, Blend::Additive);
  const Point rim = center + ScreenDirection(angle) * radius;
  p.out.Glow(rim, (7.0f - 3.0f * t) * p.ui, Mix(color, kWhite, 0.6f).WithAlpha(Alpha(230.0f * fade)));
}

void PaintHullHit(const Painter& p, const battle::ViewEffect& effect, Point at, float t, float scale)
{
  const float fade = 1.0f - t;
  const uint32_t seed = static_cast<uint32_t>(effect.startTick) * 2246822519U ^ static_cast<uint32_t>(effect.unit);
  if (t > 0.2f) {
    p.out.Glow(at + Point{0.0f, -4.0f * t * p.ui}, (4.0f + 7.0f * t) * scale * p.ui,
               kSmoke.WithAlpha(Alpha(120.0f * fade)), Blend::Alpha);
  }
  p.out.Glow(at, (5.0f + 5.0f * t) * scale * p.ui, kFire.WithAlpha(Alpha(210.0f * fade)));
  p.out.Glow(at, 2.5f * scale * p.ui, Rgb(0xfff0c0).WithAlpha(Alpha(255.0f * fade * fade)));
  Sparks(p, at, seed, static_cast<int>(5.0f * scale), 12.0f * scale * p.ui, t, Rgb(0xffd890));
}

void PaintCriticalHit(const Painter& p, const battle::ViewEffect& effect, Point at, float t)
{
  const float fade = 1.0f - t;
  PaintHullHit(p, effect, at, t, 1.6f);
  p.out.GradientSector(at, (6.0f + 16.0f * t) * p.ui, (9.0f + 20.0f * t) * p.ui, 0.0f, kTwoPi, kFire.WithAlpha(0),
                       Rgb(0xffe6a0).WithAlpha(Alpha(150.0f * fade)), Blend::Additive);
  // Systems shorting out: a few blue-white arcs around the wound.
  const uint32_t seed = static_cast<uint32_t>(p.scene.seconds * 18.0) + static_cast<uint32_t>(effect.unit) * 7U;
  for (int i = 0; i < 3 && t < 0.7f; ++i) {
    std::vector<Point> bolt{at};
    Point tip = at;
    const float heading = Unit01(seed + static_cast<uint32_t>(i)) * kTwoPi;
    for (int k = 0; k < 4; ++k) {
      tip = tip + ScreenDirection(heading + (Unit01(seed + 11U * static_cast<uint32_t>(i + k)) - 0.5f) * 1.6f)
                      * (4.0f * p.ui);
      bolt.push_back(tip);
    }
    p.out.Polyline(std::move(bolt), Rgb(0xb8dcff).WithAlpha(Alpha(220.0f * fade)), Stroke{1.0f * p.ui}, false,
                   Blend::Additive);
  }
}

// An ion hit leaves the ship crackling: jagged arcs jumping around its hull.
void PaintDisruption(const Painter& p, const battle::ViewEffect& effect, Point at, float t)
{
  const float fade = 1.0f - t;
  const float radius = std::max(p.Px(effect.size), 8.0f * p.ui);
  const Color color = p.scene.theme.ion;
  p.out.GradientSector(at, radius * 0.6f, radius * (1.0f + 0.25f * t), 0.0f, kTwoPi, color.WithAlpha(0),
                       color.WithAlpha(Alpha(90.0f * fade)), Blend::Additive);
  const uint32_t frame = static_cast<uint32_t>(p.scene.seconds * 20.0);
  for (int arc = 0; arc < 4; ++arc) {
    const uint32_t seed = Hash(frame * 31U + static_cast<uint32_t>(arc) + static_cast<uint32_t>(effect.unit) * 101U);
    const float start = Unit01(seed) * kTwoPi;
    std::vector<Point> bolt;
    for (int k = 0; k <= 6; ++k) {
      const float a = start + static_cast<float>(k) * 0.16f;
      const float r = radius * (0.8f + 0.35f * Unit01(seed + static_cast<uint32_t>(k) + 1U));
      bolt.push_back(at + ScreenDirection(a) * r);
    }
    p.out.Polyline(bolt, color.WithAlpha(Alpha(90.0f * fade)), Stroke{1.0f * p.ui, 0, 0, 0, 3.0f * p.ui}, false,
                   Blend::Additive);
    p.out.Polyline(std::move(bolt), Mix(color, kWhite, 0.6f).WithAlpha(Alpha(230.0f * fade)), Stroke{1.0f * p.ui},
                   false, Blend::Additive);
  }
}

// The jump to hyperspace: the ship stretches into a streak of light along its heading and is gone in a flash.
void PaintJump(const Painter& p, const battle::ViewEffect& effect, Point at, float t)
{
  const float size = std::max(p.Px(effect.size), 8.0f * p.ui);
  const float fade = 1.0f - t;
  const ViewUnit* unit = effect.unit == battle::kNoUnit ? nullptr : FindUnit(p.scene.view, effect.unit);
  if (unit) {
    const Point dir = ScreenDirection(p.camera.ProjectedAngle(unit->heading));
    const float stretch = size * (1.0f + 14.0f * std::min(1.0f, t * 3.0f));
    const float collapse = t < 0.35f ? 0.0f : (t - 0.35f) / 0.65f;
    const Point head = at + dir * stretch;
    const Point tail = at + dir * (stretch * collapse);
    Streak(p.out, tail, head, size * 0.5f * fade, kHyperspace.WithAlpha(Alpha(160.0f * fade)));
    Streak(p.out, tail, head, size * 0.15f * fade, kWhite.WithAlpha(Alpha(255.0f * fade)));
  }
  if (t < 0.3f) {
    const float flash = 1.0f - t / 0.3f;
    p.out.Glow(at, size * (0.8f + t), kHyperspace.WithAlpha(Alpha(220.0f * flash)));
  }
  p.out.GradientSector(at, size * (0.4f + 1.4f * t), size * (0.5f + 1.8f * t), 0.0f, kTwoPi, kHyperspace.WithAlpha(0),
                       kHyperspace.WithAlpha(Alpha(110.0f * fade)), Blend::Additive);
}

// The superlaser: the beam from the station, then the target torn apart in green fire.
void PaintSuperlaser(const Painter& p, const battle::ViewEffect& effect, Point at, float t)
{
  const Color green = Rgb(0x60ff70);
  const float fade = 1.0f - t;
  const float size = std::max(p.Px(effect.size), 12.0f * p.ui);
  if (t < 0.4f) {
    const auto station = std::ranges::find_if(p.scene.view.units, [](const ViewUnit& unit) {
      return unit.kind == ViewUnitKind::DeathStar;
    });
    if (station != p.scene.view.units.end()) {
      const float beam = 1.0f - t / 0.4f;
      const Point from = p.Screen(station->position);
      p.out.Line(from, at, green.WithAlpha(Alpha(120.0f * beam)), Stroke{6.0f * p.ui, 0, 0, 0, 8.0f * p.ui},
                 Blend::Additive);
      p.out.Line(from, at, Mix(green, kWhite, 0.7f).WithAlpha(Alpha(255.0f * beam)), Stroke{2.0f * p.ui},
                 Blend::Additive);
    }
  }
  p.out.Glow(at, size * (0.6f + 1.2f * t), green.WithAlpha(Alpha(210.0f * fade)));
  p.out.Glow(at, size * 0.4f, Mix(green, kWhite, 0.7f).WithAlpha(Alpha(255.0f * fade * fade)));
  p.out.GradientSector(at, size * (0.5f + 1.8f * t), size * (0.6f + 2.2f * t), 0.0f, kTwoPi, green.WithAlpha(0),
                       green.WithAlpha(Alpha(140.0f * fade)), Blend::Additive);
  Sparks(p, at, static_cast<uint32_t>(effect.unit) * 77U + 5U, 16, size * 2.2f, t, Mix(green, kWhite, 0.4f));
}

} // namespace

// The tactical look draws a squadron as one symbol and not its craft, so its bolts start and end at the symbol.
battle::Vec2 BoltAnchor(const Painter& p, battle::UnitIndex unit, battle::Vec2 drawn)
{
  const ViewUnit* found = p.detailed || unit == battle::kNoUnit ? nullptr : FindUnit(p.scene.view, unit);
  return found && found->kind == ViewUnitKind::Squadron ? found->position : drawn;
}

void PaintShots(const Painter& p)
{
  for (const battle::ViewShot& shot : p.scene.view.shots) {
    const battle::Vec2 start = BoltAnchor(p, shot.shooter, shot.from);
    battle::Vec2 end = BoltAnchor(p, shot.target, shot.to);
    if (!shot.hit && end != shot.to) {
      const float aside = std::max(battle::tuning::kCraftMissOffset, p.camera.MetersFor(6.0f * p.ui));
      end = end + battle::Rotated(battle::Normalized(end - start), 0.5f * battle::kPi) * aside;
    }
    const Point from = p.Screen(start, HeightOf(p, shot.shooter));
    const Point to = p.Screen(end, HeightOf(p, shot.target));
    const float length = Distance(from, to);
    if (length < 0.5f) {
      continue;
    }
    const Point dir = (to - from) * (1.0f / length);
    const Point head = from + (to - from) * shot.progress;
    const Color color = BoltColor(p.scene.theme, shot.side, shot.kind);
    const BoltLook look = LookFor(shot);
    const float flash = 1.0f - shot.progress / kMuzzleFlash;
    switch (shot.kind) {
      case ViewShotKind::Torpedo:
        PaintTorpedo(p, from, head, dir, color, p.scene.seconds);
        break;
      case ViewShotKind::Ion:
        PaintMuzzleFlash(p, from, color, flash, look.glow * 1.4f * p.ui);
        PaintIonBolt(p, from, head, dir, color, look);
        break;
      case ViewShotKind::Laser:
      case ViewShotKind::Turbolaser:
        PaintMuzzleFlash(p, from, color, flash, look.glow * 1.2f * p.ui);
        PaintBolt(p, from, head, dir, color, look);
        break;
    }
  }
}

void PaintEffects(const Painter& p)
{
  for (const battle::ViewEffect& effect : p.scene.view.effects) {
    const float t = std::clamp(effect.age / EffectLifetime(effect), 0.0f, 1.0f);
    const Point at = p.Screen(effect.position, HeightOf(p, effect.unit));
    switch (effect.kind) {
      case ViewEffectKind::Explosion:
        PaintExplosion(p, effect, t);
        break;
      case ViewEffectKind::ShieldFlash:
        PaintShieldFlash(p, effect, t);
        break;
      case ViewEffectKind::HullHit:
        PaintHullHit(p, effect, at, t, 1.0f);
        break;
      case ViewEffectKind::CriticalHit:
        PaintCriticalHit(p, effect, at, t);
        break;
      case ViewEffectKind::Disruption:
        PaintDisruption(p, effect, at, t);
        break;
      case ViewEffectKind::Jump:
        PaintJump(p, effect, at, t);
        break;
      case ViewEffectKind::Superlaser:
        PaintSuperlaser(p, effect, at, t);
        break;
    }
  }
}

// Critical systems the strategic game tracks (§6.5), shown where they are: engine hits make the stern sputter and
// smoke, shield-generator hits and heavy hull damage vent gas, weapon hits leave a scorched, burning section, and a
// badly holed hull burns in a few places.
void PaintDamage(const Painter& p, const ViewUnit& unit, float lengthMeters)
{
  const float half = lengthMeters * 0.5f;
  if (p.Px(half) < 7.0f * p.ui) {
    return;
  }
  const battle::CriticalHits& hits = unit.criticals;
  const uint32_t seed = Hash(static_cast<uint32_t>(unit.unit) * 7919U + 13U);
  const double seconds = p.scene.seconds;
  const auto hull = [&](Point local) {
    return p.Screen(HullToWorld(unit.position, unit.heading, half, local));
  };
  const auto flicker = [&](uint32_t salt, double rate) {
    return Unit01(seed + salt + static_cast<uint32_t>(seconds * rate));
  };
  const float spark = std::max(1.2f * p.ui, p.Px(half) * 0.035f);

  if (hits.speed > 0) {
    for (int engine = 0; engine < 2; ++engine) {
      const Point stern = hull({-0.97f, engine == 0 ? 0.22f : -0.22f});
      if (flicker(static_cast<uint32_t>(engine) * 31U, 13.0) > 0.22f * static_cast<float>(hits.speed)) {
        p.out.Glow(stern, spark * 2.6f, kFire.WithAlpha(190));
      }
      const float drift = static_cast<float>(std::fmod(seconds * 0.8 + engine * 0.5, 1.0));
      const Point smoke = hull({-1.0f - drift * 0.35f, (engine == 0 ? 0.22f : -0.22f) + drift * 0.08f});
      p.out.Glow(smoke, spark * (2.0f + 4.0f * drift), kSmoke.WithAlpha(Alpha(150.0f * (1.0f - drift))), Blend::Alpha);
    }
  }
  if (hits.shieldRecharge > 0 || unit.hullFraction < 0.4f) {
    const float side = Unit01(seed + 5) < 0.5f ? 1.0f : -1.0f;
    const float x = -0.3f + 0.6f * Unit01(seed + 6);
    for (int puff = 0; puff < 3; ++puff) {
      const float drift = static_cast<float>(std::fmod(seconds * 0.9 + puff / 3.0, 1.0));
      const Point at = hull({x - drift * 0.1f, side * (0.3f + drift * 0.45f)});
      p.out.Glow(at, spark * (1.4f + 4.0f * drift), kVent.WithAlpha(Alpha(130.0f * (1.0f - drift))));
    }
  }
  if (hits.weaponRecharge > 0) {
    const Point center = hull({0.15f + 0.3f * Unit01(seed + 8), (Unit01(seed + 9) - 0.5f) * 0.3f});
    const float size = p.Px(half) * 0.13f;
    p.out.Glow(center, size * 1.3f, kSmoke.WithAlpha(190), Blend::Alpha);
    if (flicker(41, 9.0) > 0.35f) {
      p.out.Glow(center, spark * 2.2f, kFire.WithAlpha(200));
    }
  }
  const int fires = unit.hullFraction < 0.25f ? 3 : unit.hullFraction < 0.5f ? 1 : 0;
  for (int fire = 0; fire < fires; ++fire) {
    const uint32_t salt = 100U + static_cast<uint32_t>(fire) * 7U;
    const Point at = hull({-0.6f + 1.2f * Unit01(seed + salt), (Unit01(seed + salt + 1) - 0.5f) * 0.4f});
    const float glow = 0.6f + 0.4f * flicker(salt + 2, 11.0);
    p.out.Glow(at, spark * 3.2f * glow, kFire.WithAlpha(Alpha(150.0f * glow)));
    p.out.Glow(at, spark * 1.2f * glow, Rgb(0xffe0a0).WithAlpha(230));
  }
}

} // namespace reb::battle_ui::painter
