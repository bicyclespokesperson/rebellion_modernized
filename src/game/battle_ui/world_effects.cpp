// Bolts, effects and visible damage (§10.3): bolts in the theme's colour code, explosions with debris and escape pods,
// shield flashes, and critical damage that shows on the hull.
#include "battle_ui/world_painter_internal.h"
#include "space_battle/math2d.h"
#include "space_battle/tuning.h"

namespace reb::battle_ui::painter
{
namespace
{

using battle::ViewEffectKind;
using battle::ViewShotKind;

const Color kPod = Rgb(0xe8ecf2);
const Color kVent = Rgb(0xcfe4ff);
const Color kFire = Rgb(0xff9a3a);
const Color kSmoke = Rgb(0x0a0a0c);

constexpr float kPodDelay = 0.25f; // seconds after the blast before the pods clear it

float EffectLifetime(const battle::ViewEffect& effect)
{
  switch (effect.kind) {
    case ViewEffectKind::Explosion:
      return std::clamp(0.6f + effect.size / 500.0f, 0.6f, 3.0f);
    case ViewEffectKind::ShieldFlash:
      return 0.35f;
    case ViewEffectKind::HullHit:
      return 0.5f;
    case ViewEffectKind::CriticalHit:
      return 0.8f;
    case ViewEffectKind::Disruption:
      return 0.7f;
    case ViewEffectKind::Jump:
      return 1.0f;
    case ViewEffectKind::Superlaser:
      return 1.5f;
  }
  return 1.0f;
}

float HeightOf(const Painter& p, battle::UnitIndex unit)
{
  const ViewUnit* found = unit == battle::kNoUnit ? nullptr : FindUnit(p.scene.view, unit);
  return found && found->kind == ViewUnitKind::Squadron ? p.CraftHeight() : 0.0f;
}

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
    const Point back = p.Screen({at.x - std::cos(angle) * 40.0f, at.y - std::sin(angle) * 40.0f});
    p.out.Line(back, screen, kFire.WithAlpha(static_cast<uint8_t>(130 * fade)), Stroke{1.5f * p.ui}, Blend::Additive);
    p.out.FillCircle(screen, 4.0f * p.ui, kFire.WithAlpha(static_cast<uint8_t>(60 * fade)), Blend::Additive);
    p.out.FillCircle(screen, 2.0f * p.ui, kPod.WithAlpha(static_cast<uint8_t>(240 * fade)));
  }
}

void PaintExplosion(const Painter& p, const battle::ViewEffect& effect, float t)
{
  const Point center = p.Screen(effect.position, HeightOf(p, effect.unit));
  const float size = std::max(p.Px(effect.size), 6.0f * p.ui);
  const float grow = 1.0f - (1.0f - t) * (1.0f - t);
  const float fade = 1.0f - t;
  if (t < 1.0f) {
    p.out.FillCircle(center, size * (0.25f + 0.55f * grow), kExplosionOuter.WithAlpha(static_cast<uint8_t>(150 * fade)),
                     Blend::Additive);
    p.out.FillCircle(center, size * (0.15f + 0.25f * grow),
                     kExplosionCore.WithAlpha(static_cast<uint8_t>(230 * fade * fade)), Blend::Additive);
    p.out.Circle(center, size * (0.3f + 1.0f * grow), kExplosionOuter.WithAlpha(static_cast<uint8_t>(120 * fade)),
                 Stroke{2.0f * p.ui}, Blend::Additive);
    const uint32_t seed = static_cast<uint32_t>(effect.unit + 1) * 2654435761U ^ static_cast<uint32_t>(effect.size);
    for (int i = 0; i < 10; ++i) {
      const float angle = Unit01(seed + i) * kTwoPi;
      const float speed = 0.5f + Unit01(seed + 31 * i) * 0.9f;
      const Point at = center + ScreenDirection(angle) * (size * speed * grow);
      p.out.FillCircle(at, std::max(1.0f, 1.6f * p.ui), kExplosionCore.WithAlpha(static_cast<uint8_t>(220 * fade)),
                       Blend::Additive);
    }
  }
  const ViewUnit* unit = effect.unit == battle::kNoUnit ? nullptr : FindUnit(p.scene.view, effect.unit);
  const bool capital = unit ? unit->kind == ViewUnitKind::CapitalShip : effect.size >= 250.0f;
  if (capital) {
    PaintEscapePods(p, effect);
  }
}

void PaintShieldFlash(const Painter& p, const battle::ViewEffect& effect, float fade)
{
  const ViewUnit* unit = FindUnit(p.scene.view, effect.unit);
  if (!unit) {
    return;
  }
  const Point center = p.Screen(unit->position);
  const Point toImpact = p.Screen(effect.position) - center;
  const float angle = std::atan2(-toImpact.y, toImpact.x);
  const float radius = std::max(DrawnRadiusPx(p, *unit) * 0.9f, 10.0f * p.ui);
  const Color color = p.scene.theme.shield;
  p.out.Arc(center, radius, angle - 0.6f, 1.2f, color.WithAlpha(static_cast<uint8_t>(200 * fade)), Stroke{2.5f * p.ui},
            Blend::Additive);
  p.out.Arc(center, radius, angle - 0.9f, 1.8f, color.WithAlpha(static_cast<uint8_t>(60 * fade)), Stroke{6.0f * p.ui},
            Blend::Additive);
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
    if (shot.kind == ViewShotKind::Torpedo) {
      p.out.FillCircle(head, 4.0f * p.ui, color.WithAlpha(70), Blend::Additive);
      p.out.FillCircle(head, 1.8f * p.ui, Mix(color, Rgb(0xffffff), 0.6f), Blend::Additive);
      continue;
    }
    const bool heavy = shot.kind != ViewShotKind::Laser;
    const float planetary = shot.battery >= 0 ? 1.7f : 1.0f; // a planet's guns throw bigger bolts than a ship's
    const float tail = std::min((heavy ? 16.0f : 9.0f) * planetary * p.ui, length * shot.progress);
    const Point back = head - dir * tail;
    const float width = (heavy ? 2.2f : 1.3f) * planetary * p.ui;
    p.out.Line(back, head, color.WithAlpha(80), Stroke{width * 2.6f}, Blend::Additive);
    p.out.Line(back, head, Mix(color, Rgb(0xffffff), 0.45f), Stroke{width}, Blend::Additive);
  }
}

void PaintEffects(const Painter& p)
{
  for (const battle::ViewEffect& effect : p.scene.view.effects) {
    const float t = std::clamp(effect.age / EffectLifetime(effect), 0.0f, 1.0f);
    const float fade = 1.0f - t;
    const Point at = p.Screen(effect.position, HeightOf(p, effect.unit));
    switch (effect.kind) {
      case ViewEffectKind::Explosion:
        PaintExplosion(p, effect, t);
        break;
      case ViewEffectKind::ShieldFlash:
        PaintShieldFlash(p, effect, fade);
        break;
      case ViewEffectKind::HullHit:
        p.out.FillCircle(at, (2.0f + 4.0f * t) * p.ui, kExplosionOuter.WithAlpha(static_cast<uint8_t>(200 * fade)),
                         Blend::Additive);
        break;
      case ViewEffectKind::CriticalHit:
        p.out.FillCircle(at, (3.0f + 6.0f * t) * p.ui, kExplosionCore.WithAlpha(static_cast<uint8_t>(230 * fade)),
                         Blend::Additive);
        p.out.Circle(at, (4.0f + 14.0f * t) * p.ui, kExplosionCore.WithAlpha(static_cast<uint8_t>(160 * fade)),
                     Stroke{1.5f * p.ui}, Blend::Additive);
        break;
      case ViewEffectKind::Disruption:
        p.out.Circle(at, std::max(p.Px(effect.size), 8.0f * p.ui) * (1.0f + 0.3f * t),
                     p.scene.theme.ion.WithAlpha(static_cast<uint8_t>(200 * fade)),
                     Stroke{2.0f * p.ui, 3.0f * p.ui, 3.0f * p.ui, t * 20.0f}, Blend::Additive);
        break;
      case ViewEffectKind::Jump:
        {
          const float size = std::max(p.Px(effect.size), 8.0f * p.ui);
          p.out.FillCircle(at, size * (0.4f + 0.6f * t), Rgb(0xe8f4ff, static_cast<uint8_t>(160 * fade)),
                           Blend::Additive);
          p.out.Circle(at, size * (0.5f + 1.5f * t), Rgb(0xc8e0ff, static_cast<uint8_t>(140 * fade)),
                       Stroke{2.0f * p.ui}, Blend::Additive);
          break;
        }
      case ViewEffectKind::Superlaser:
        {
          const float size = std::max(p.Px(effect.size), 12.0f * p.ui);
          p.out.FillCircle(at, size * (0.5f + t), Rgb(0x80ff80, static_cast<uint8_t>(200 * fade)), Blend::Additive);
          break;
        }
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
        p.out.FillCircle(stern, spark * 1.6f, kFire.WithAlpha(160), Blend::Additive);
      }
      const float drift = static_cast<float>(std::fmod(seconds * 0.8 + engine * 0.5, 1.0));
      const Point smoke = hull({-1.0f - drift * 0.35f, (engine == 0 ? 0.22f : -0.22f) + drift * 0.08f});
      p.out.FillCircle(smoke, spark * (1.5f + 3.0f * drift), kSmoke.WithAlpha(static_cast<uint8_t>(110 * (1 - drift))));
    }
  }
  if (hits.shieldRecharge > 0 || unit.hullFraction < 0.4f) {
    const float side = Unit01(seed + 5) < 0.5f ? 1.0f : -1.0f;
    const float x = -0.3f + 0.6f * Unit01(seed + 6);
    for (int puff = 0; puff < 3; ++puff) {
      const float drift = static_cast<float>(std::fmod(seconds * 0.9 + puff / 3.0, 1.0));
      const Point at = hull({x - drift * 0.1f, side * (0.3f + drift * 0.45f)});
      p.out.FillCircle(at, spark * (1.0f + 3.5f * drift), kVent.WithAlpha(static_cast<uint8_t>(120 * (1 - drift))),
                       Blend::Additive);
    }
  }
  if (hits.weaponRecharge > 0) {
    const Point center = hull({0.15f + 0.3f * Unit01(seed + 8), (Unit01(seed + 9) - 0.5f) * 0.3f});
    const float size = p.Px(half) * 0.13f;
    p.out.FillCircle(center, size, kSmoke.WithAlpha(150));
    p.out.FillCircle(center, size * 0.55f, kSmoke.WithAlpha(120));
    if (flicker(41, 9.0) > 0.35f) {
      p.out.FillCircle(center, spark * 1.3f, kFire.WithAlpha(190), Blend::Additive);
    }
  }
  const int fires = unit.hullFraction < 0.25f ? 3 : unit.hullFraction < 0.5f ? 1 : 0;
  for (int fire = 0; fire < fires; ++fire) {
    const uint32_t salt = 100U + static_cast<uint32_t>(fire) * 7U;
    const Point at = hull({-0.6f + 1.2f * Unit01(seed + salt), (Unit01(seed + salt + 1) - 0.5f) * 0.4f});
    const float glow = 0.6f + 0.4f * flicker(salt + 2, 11.0);
    p.out.FillCircle(at, spark * 2.2f * glow, kFire.WithAlpha(static_cast<uint8_t>(90 * glow)), Blend::Additive);
    p.out.FillCircle(at, spark * glow, Rgb(0xffe0a0, 200), Blend::Additive);
  }
}

} // namespace reb::battle_ui::painter
