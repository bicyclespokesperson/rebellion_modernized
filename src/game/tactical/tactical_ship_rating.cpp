#include "core/reb_x87.h"
#include "tactical/tactical_ship.h"

// The strength rating of a ship against a target (0x5b7780) or a target class (0x5b7a80): its firepower per weapon
// kind, weighted by the profile factors, divided by its recharge time. Intermediate values the original keeps on the
// x87 stack are `double`; stored values are cast to `float`.

namespace
{

double scaleIfFighter(TacticalShipBase* ship, float value)
{
  return ship->GetUnitClass() == 1 ? ship->ScaleByHull(value) : static_cast<double>(value);
}

} // namespace

double TacticalShipBase::AttackRating(TacticalShipBase* target, int)
{
  float sum = 0.0f;
  const float recharge = static_cast<float>(GetWeaponRecharge());
  if (recharge == 0.0f)
    return 0.0f;
  const uint32_t arcIndex = SelectArc(target->GetUnitClass());
  if (arcIndex == 4)
    return sum;
  TacticalWeaponArc& arc = m_arcs[arcIndex];
  uint32_t total = arc.m_countA + arc.m_countB + arc.m_countC;
  if (GetUnitClass() == 1)
    total = reb_ftou(ScaleByHull(static_cast<float>(total)));
  const double quotient = static_cast<double>(total) / recharge;
  const float divisor = static_cast<float>(quotient);
  if (quotient <= 0.0f)
    return 0.0f;
  const float ownTurnRate = static_cast<float>(GetTurnRate());
  float factor = static_cast<float>(ownTurnRate / target->GetTurnRate());
  if (!(0.1f <= factor))
    factor = 0.1f;
  if (arc.m_countB > 0)
  {
    const double perWeapon =
      static_cast<double>(target->GetUnitClass() == 0 ? arc.m_profileA.m_f0c : arc.m_profileA.m_f08) *
      arc.m_profileA.m_f04;
    float value = static_cast<float>(static_cast<double>(arc.m_countB) * perWeapon);
    if (target->GetUnitClass() == 1)
      value = 0.0f;
    sum = static_cast<float>(scaleIfFighter(this, value));
  }
  if (arc.m_countA > 0)
  {
    const double perWeapon =
      static_cast<double>(target->GetUnitClass() == 0 ? arc.m_profileB.m_f0c : arc.m_profileB.m_f08) *
      arc.m_profileB.m_f04;
    float value = static_cast<float>(static_cast<double>(arc.m_countA) * perWeapon);
    if (target->GetUnitClass() == 1)
      value = static_cast<float>(value * static_cast<double>(factor));
    sum = static_cast<float>(scaleIfFighter(this, value) + sum);
  }
  if (arc.m_countC > 0)
  {
    const double perWeapon =
      arc.m_profileC.m_f04 *
      static_cast<double>(target->GetUnitClass() == 0 ? arc.m_profileC.m_f0c : arc.m_profileC.m_f08);
    float value = static_cast<float>(static_cast<double>(arc.m_countC) * perWeapon);
    if (target->GetUnitClass() == 1)
      value = static_cast<float>(value * static_cast<double>(factor));
    sum = static_cast<float>(scaleIfFighter(this, value) + sum);
  }
  if (TorpedoStrength() != 0.0f && GetUnitClass() == 1 && target->GetUnitClass() == 0 &&
      target->ShieldFraction() == 0.0f)
  {
    return (ScaleByHull(static_cast<float>(TorpedoStrength())) + sum) / divisor;
  }
  return sum / divisor;
}

double TacticalShipBase::AttackRatingVsClass(int targetClass, int)
{
  float sum = 0.0f;
  const float recharge = static_cast<float>(GetWeaponRecharge());
  if (recharge == 0.0f)
    return 0.0f;
  const uint32_t arcIndex = SelectArc(targetClass);
  if (arcIndex == 4)
    return sum;
  TacticalWeaponArc& arc = m_arcs[arcIndex];
  uint32_t total = arc.m_countA + arc.m_countB + arc.m_countC;
  if (GetUnitClass() == 1)
    total = reb_ftou(ScaleByHull(static_cast<float>(total)));
  const double quotient = static_cast<double>(total) / recharge;
  const float divisor = static_cast<float>(quotient);
  if (quotient <= 0.0f)
    return 0.0f;
  float factor = static_cast<float>(GetTurnRate() / 6.0f);
  if (!(0.1f <= factor))
    factor = 0.1f;
  if (arc.m_countB > 0)
  {
    const double perWeapon =
      arc.m_profileA.m_f04 * static_cast<double>(targetClass == 0 ? arc.m_profileA.m_f0c : arc.m_profileA.m_f08);
    float value = static_cast<float>(static_cast<double>(arc.m_countB) * perWeapon);
    if (targetClass == 1)
      value = 0.0f;
    sum = static_cast<float>(scaleIfFighter(this, value));
  }
  if (arc.m_countA > 0)
  {
    const double perWeapon =
      static_cast<double>(targetClass == 0 ? arc.m_profileB.m_f0c : arc.m_profileB.m_f08) * arc.m_profileB.m_f04;
    float value = static_cast<float>(static_cast<double>(arc.m_countA) * perWeapon);
    if (targetClass == 1)
      value = static_cast<float>(value * static_cast<double>(factor));
    sum = static_cast<float>(scaleIfFighter(this, value) + sum);
  }
  if (arc.m_countC > 0)
  {
    const double perWeapon =
      static_cast<double>(targetClass == 0 ? arc.m_profileC.m_f0c : arc.m_profileC.m_f08) * arc.m_profileC.m_f04;
    float value = static_cast<float>(static_cast<double>(arc.m_countC) * perWeapon);
    if (targetClass == 1)
      value = static_cast<float>(value * static_cast<double>(factor));
    sum = static_cast<float>(scaleIfFighter(this, value) + sum);
  }
  return sum / divisor;
}
