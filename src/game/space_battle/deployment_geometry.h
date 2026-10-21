#pragma once

#include <span>
#include <utility>

#include "space_battle/math2d.h"

namespace reb::battle
{

struct DeploymentHull {
  Vec2 offset;
  float radius = 0.0f;
  bool avoidPlanet = true;
};

struct DeploymentBounds {
  Vec2 center;
  float radius = 0.0f;
  Vec2 planet;
  float planetRadius = 0.0f;
  float halfExtent = 0.0f;
};

inline bool DeploymentFits(std::span<const DeploymentHull> hulls, Vec2 center, const DeploymentBounds& bounds)
{
  for (const DeploymentHull& hull : hulls) {
    const Vec2 at = center + hull.offset;
    if (Distance(at, bounds.center) + hull.radius > bounds.radius + 0.01f
        || std::abs(at.x) + hull.radius > bounds.halfExtent + 0.01f
        || std::abs(at.y) + hull.radius > bounds.halfExtent + 0.01f
        || (hull.avoidPlanet && Distance(at, bounds.planet) + 0.01f < bounds.planetRadius + hull.radius)) {
      return false;
    }
  }
  return true;
}

// A feasible anchor lets the drag stop at the first boundary, including the non-convex planet exclusion.
inline Vec2 ClampDeploymentDrag(std::span<const DeploymentHull> hulls, Vec2 anchor, Vec2 target,
                                const DeploymentBounds& bounds)
{
  const Vec2 delta = target - anchor;
  const double lengthSquared = LengthSq(delta);
  if (lengthSquared < 1e-12) {
    return anchor;
  }
  double fraction = 1.0;
  for (const DeploymentHull& hull : hulls) {
    const Vec2 at = anchor + hull.offset;
    const Vec2 radial = at - bounds.center;
    const double radius = std::max(0.0f, bounds.radius - hull.radius);
    const double projection = Dot(radial, delta);
    const double discriminant = projection * projection
                                - lengthSquared * (LengthSq(radial) - radius * radius);
    fraction = std::min(fraction, (-projection + std::sqrt(std::max(0.0, discriminant))) / lengthSquared);
    const float extent = bounds.halfExtent - hull.radius;
    for (const auto [position, movement] : {std::pair{at.x, delta.x}, std::pair{at.y, delta.y}}) {
      if (movement > 0.0f) {
        fraction = std::min(fraction, static_cast<double>((extent - position) / movement));
      } else if (movement < 0.0f) {
        fraction = std::min(fraction, static_cast<double>((-extent - position) / movement));
      }
    }
    if (hull.avoidPlanet) {
      const Vec2 fromPlanet = at - bounds.planet;
      const double toward = Dot(fromPlanet, delta);
      const double clearance = bounds.planetRadius + hull.radius;
      const double crossing = toward * toward
                              - lengthSquared * (LengthSq(fromPlanet) - clearance * clearance);
      if (toward < 0.0 && crossing > 0.0) {
        fraction = std::min(fraction, (-toward - std::sqrt(crossing)) / lengthSquared);
      }
    }
  }
  // Keep float rounding on the interior of an active boundary.
  fraction = std::clamp(fraction, 0.0, 1.0);
  if (fraction < 1.0) {
    fraction = std::max(0.0, fraction - 0.01 / std::sqrt(lengthSquared));
  }
  return anchor + delta * static_cast<float>(fraction);
}

inline bool FindDeploymentAnchor(std::span<const DeploymentHull> hulls, Vec2& center,
                                 const DeploymentBounds& bounds)
{
  for (int pass = 0; pass < 512; ++pass) {
    if (DeploymentFits(hulls, center, bounds)) {
      return true;
    }
    for (const DeploymentHull& hull : hulls) {
      Vec2 at = center + hull.offset;
      const float radius = std::max(0.0f, bounds.radius - hull.radius - 0.02f);
      if (Distance(at, bounds.center) > radius) {
        center += bounds.center + Normalized(at - bounds.center) * radius - at;
      }
      at = center + hull.offset;
      center += Clamped(at, bounds.halfExtent - hull.radius - 0.02f) - at;
      at = center + hull.offset;
      const float clearance = bounds.planetRadius + hull.radius + 0.02f;
      if (hull.avoidPlanet && Distance(at, bounds.planet) < clearance) {
        center += bounds.planet + Normalized(at - bounds.planet, Normalized(bounds.center - bounds.planet))
                                      * clearance - at;
      }
    }
  }
  return DeploymentFits(hulls, center, bounds);
}

} // namespace reb::battle
