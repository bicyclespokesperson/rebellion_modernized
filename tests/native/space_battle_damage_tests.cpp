#include "space_battle_test_support.h"

#include "space_battle/tuning.h"

using namespace sbtest;
namespace tuning = reb::battle::tuning;

namespace
{
struct Duel {
  SnapshotBuilder builder;
  UnitIndex target = kNoUnit;
  UnitIndex other = kNoUnit;
  std::unique_ptr<Battle> battle;

  explicit Duel(CapitalShipEntry ship = Wedge(Side::Empire, "Target"))
  {
    target = builder.Add(std::move(ship));
    other = builder.Add(Broadside(Side::Alliance, "Other"));
    battle = std::make_unique<Battle>(builder.Snapshot(), Uncommanded());
    // Far apart: nothing fires unless the test applies damage itself.
    Place(*battle, target, {15000.0f, 0.0f}, 0.0f);
    Place(*battle, other, {-15000.0f, 0.0f}, 0.0f);
  }
  BattleState& State()
  {
    return Mutable(*battle);
  }
  ShipState& Ship()
  {
    return *State().Ship(target);
  }
};

void ShieldsAbsorbThenHull()
{
  Duel duel;
  sim::DamageShip(duel.State(), duel.target, 100.0f, ShotKind::Turbolaser, Subsystem::Hull);
  SB_CHECK_NEAR(duel.Ship().shield, 200.0f, 1e-3f);
  SB_CHECK_NEAR(duel.Ship().hull, 2600.0f, 1e-3f);
  sim::DamageShip(duel.State(), duel.target, 300.0f, ShotKind::Turbolaser, Subsystem::Hull);
  SB_CHECK(duel.Ship().shield == 0.0f);
  SB_CHECK_NEAR(duel.Ship().hull, 2500.0f, 1e-3f);
}

// Shield recharge is a damage threshold: fire that cannot outpace it never breaks through (§6.5).
void RechargeIsAThreshold()
{
  for (const float share : {0.9f, 1.3f}) {
    Duel duel;
    BattleState& state = duel.State();
    const float regen = ShieldRegen(state, duel.target);
    SB_CHECK(regen > 0.0f);
    for (int tick = 0; tick < 60 * kTicksPerSecond; ++tick) {
      sim::DamageShip(state, duel.target, share * regen * kTickSeconds, ShotKind::Turbolaser, Subsystem::Hull);
      sim::UpdateShipSystems(state);
    }
    if (share < 1.0f) {
      SB_CHECK(duel.Ship().shield > 0.0f);
      SB_CHECK(duel.Ship().hull == 2600.0f);
    } else {
      SB_CHECK(duel.Ship().hull < 2600.0f);
    }
  }
}

void OutputScalesWithHullAndCriticals()
{
  Duel duel;
  BattleState& state = duel.State();
  const float fullWeapons = WeaponOutput(state, duel.target);
  const float fullShields = ShieldRegen(state, duel.target);
  duel.Ship().hull = 1300.0f;
  SB_CHECK_NEAR(WeaponOutput(state, duel.target), 0.5f * fullWeapons, 1e-4f);
  SB_CHECK_NEAR(ShieldRegen(state, duel.target), 0.5f * fullShields, 1e-4f);
  duel.Ship().hull = 2600.0f;
  duel.Ship().criticals.weaponRecharge = 1;
  SB_CHECK_NEAR(WeaponOutput(state, duel.target), 0.75f * fullWeapons, 1e-4f);
  duel.Ship().criticals.weaponRecharge = 4;
  SB_CHECK(WeaponOutput(state, duel.target) == 0.0f);
  duel.Ship().criticals.shieldRecharge = 4;
  SB_CHECK(ShieldRegen(state, duel.target) == 0.0f);
  const float speed = MaxSpeed(state, duel.target);
  duel.Ship().criticals.speed = 2;
  SB_CHECK_NEAR(MaxSpeed(state, duel.target), 0.5f * speed, 1e-3f);
  duel.Ship().criticals.speed = 4;
  SB_CHECK(MaxSpeed(state, duel.target) == 0.0f);
}

std::array<int, 5> CountCriticals(Subsystem focus, float* hullLost)
{
  Duel duel;
  BattleState& state = duel.State();
  duel.Ship().shield = 0.0f;
  std::array<int, 5> counts{};
  for (int hit = 0; hit < 2000; ++hit) {
    ShipState& ship = duel.Ship();
    ship.hull = 2600.0f;
    ship.shield = 0.0f;
    ship.criticals = {};
    const CriticalHits before = ship.criticals;
    sim::DamageShip(state, duel.target, 20.0f, ShotKind::Turbolaser, focus);
    *hullLost = 2600.0f - duel.Ship().hull;
    const CriticalHits& after = duel.Ship().criticals;
    counts[0] += after.shieldRecharge - before.shieldRecharge;
    counts[1] += after.weaponRecharge - before.weaponRecharge;
    counts[2] += after.tractor - before.tractor;
    counts[3] += after.speed - before.speed;
    counts[4] += after.hyperdrivePrimary + after.hyperdriveBackup - before.hyperdrivePrimary - before.hyperdriveBackup;
  }
  return counts;
}

// Subsystem focus keeps 70% of the hull damage and triples the chance of hitting the chosen system (§6.5).
void SubsystemFocusTradesHullForCriticals()
{
  float plainLoss = 0.0f;
  float focusedLoss = 0.0f;
  const std::array<int, 5> plain = CountCriticals(Subsystem::Hull, &plainLoss);
  const std::array<int, 5> focused = CountCriticals(Subsystem::Engines, &focusedLoss);
  SB_CHECK_NEAR(plainLoss, 20.0f, 1e-3f);
  SB_CHECK_NEAR(focusedLoss, 20.0f * tuning::kSubsystemHullShare, 1e-3f);
  const int plainTotal = plain[0] + plain[1] + plain[2] + plain[3] + plain[4];
  const int focusedTotal = focused[0] + focused[1] + focused[2] + focused[3] + focused[4];
  std::printf("criticals: hull focus %d (engines %d), engine focus %d (engines %d)\n", plainTotal, plain[3], focusedTotal,
              focused[3]);
  SB_CHECK(plainTotal > 20);
  const float plainShare = static_cast<float>(plain[3]) / static_cast<float>(plainTotal);
  const float focusedShare = static_cast<float>(focused[3]) / static_cast<float>(focusedTotal);
  SB_CHECK(focusedShare > 2.0f * plainShare);
}

void HyperdriveTakesOneHitPerDrive()
{
  Duel duel;
  BattleState& state = duel.State();
  SB_CHECK(CanJump(state, duel.target));
  duel.Ship().criticals.hyperdrivePrimary = 1;
  SB_CHECK(CanJump(state, duel.target));
  duel.Ship().criticals.hyperdriveBackup = 1;
  JumpBlocker blocker = JumpBlocker::None;
  SB_CHECK(!CanJump(state, duel.target, &blocker));
  SB_CHECK(blocker == JumpBlocker::NoHyperdrive);
  // A picket has no backup drive: one hit strands it.
  Duel picket(Picket(Side::Empire, "Picket"));
  picket.Ship().criticals.hyperdrivePrimary = 1;
  SB_CHECK(!CanJump(picket.State(), picket.target));
}

// Ion hits shields only, then disrupts: engines or an arc, plus tractor locks and wells; charged jump progress survives.
void IonStripsShieldsThenDisrupts()
{
  Duel duel;
  BattleState& state = duel.State();
  sim::DamageShip(state, duel.target, 120.0f, ShotKind::Ion, Subsystem::Hull);
  SB_CHECK_NEAR(duel.Ship().shield, 180.0f, 1e-3f);
  SB_CHECK(duel.Ship().hull == 2600.0f);
  duel.Ship().shield = 0.0f;
  duel.Ship().withdrawing = true;
  duel.Ship().spoolTicks = 100;
  duel.Ship().spoolTotal = 400;
  duel.Ship().tractorTarget = duel.other;
  sim::DamageShip(state, duel.target, 500.0f, ShotKind::Ion, Subsystem::Hull);
  SB_CHECK(duel.Ship().hull == 2600.0f);
  SB_CHECK(duel.Ship().systemsOffTicks > 0);
  SB_CHECK(duel.Ship().tractorTarget == kNoUnit);
  SB_CHECK(duel.Ship().spoolTicks == 100);
  SB_CHECK(!TractorReady(state, duel.target));
  // Enough disruption events take the engines down, which also stops a jump.
  for (int i = 0; i < 20 && duel.Ship().engineOffTicks == 0; ++i) {
    sim::DamageShip(state, duel.target, 500.0f, ShotKind::Ion, Subsystem::Hull);
  }
  SB_CHECK(duel.Ship().engineOffTicks >= sim::Ticks(tuning::kIonEngineMinSeconds));
  SB_CHECK(MaxSpeed(state, duel.target) == 0.0f);
  JumpBlocker blocker = JumpBlocker::None;
  SB_CHECK(!CanJump(state, duel.target, &blocker) && blocker == JumpBlocker::Engines);
  // Disruption is temporary.
  for (int tick = 0; tick < sim::Ticks(tuning::kIonEngineMaxSeconds) + 1; ++tick) {
    sim::UpdateShipSystems(state);
  }
  SB_CHECK(duel.Ship().engineOffTicks == 0 && duel.Ship().systemsOffTicks == 0);
  SB_CHECK(MaxSpeed(state, duel.target) > 0.0f);
}

void SustainedIonAllowsAChargedJump()
{
  Duel duel;
  BattleState& state = duel.State();
  duel.Ship().shield = 0.0f;
  duel.Ship().withdrawing = true;
  duel.Ship().spoolTicks = 100;
  duel.Ship().spoolTotal = 400;
  duel.Ship().engineOffTicks = sim::Ticks(40.0f);
  const int32_t outage = duel.Ship().engineOffTicks;
  for (int tick = 0; tick <= outage && state.InSpace(duel.target); ++tick) {
    duel.Ship().shield = 0.0f;
    sim::DamageShip(state, duel.target, 500.0f, ShotKind::Ion, Subsystem::Hull);
    sim::UpdateShipSystems(state);
    sim::UpdateWithdrawal(state);
  }
  SB_CHECK(state.units[duel.target].fate == UnitFate::Withdrawn);
  SB_CHECK(duel.Ship().hull == 2600.0f);
}

void TorpedoesQuarterShieldsFullHull()
{
  Duel duel;
  sim::DamageShip(duel.State(), duel.target, 40.0f, ShotKind::Torpedo, Subsystem::Hull);
  SB_CHECK_NEAR(duel.Ship().shield, 290.0f, 1e-3f);
  SB_CHECK(duel.Ship().hull == 2600.0f);
  duel.Ship().shield = 0.0f;
  sim::DamageShip(duel.State(), duel.target, 40.0f, ShotKind::Torpedo, Subsystem::Hull);
  SB_CHECK_NEAR(duel.Ship().hull, 2560.0f, 1e-3f);
  SB_CHECK(duel.Ship().shield == 0.0f);
}

void DamageControlRepairsOnItsInterval()
{
  CapitalShipEntry ship = Broadside(Side::Empire, "Target");
  ship.damageControl = 100;
  ship.criticals.weaponRecharge = 2;
  Duel duel(ship);
  BattleState& state = duel.State();
  duel.Ship().damageControlTicks = sim::Ticks(tuning::kDamageControlSeconds);
  for (int tick = 0; tick < sim::Ticks(tuning::kDamageControlSeconds) - 1; ++tick) {
    sim::UpdateShipSystems(state);
  }
  SB_CHECK(duel.Ship().criticals.weaponRecharge == 2);
  sim::UpdateShipSystems(state);
  SB_CHECK(duel.Ship().criticals.weaponRecharge == 1);
}

void DestroyedCarrierTakesDockedSquadrons()
{
  SnapshotBuilder builder;
  const UnitIndex carrier = builder.Add(CarrierShip(Side::Empire));
  const UnitIndex docked = builder.Add(TieLike(Side::Empire, carrier));
  builder.Add(Broadside(Side::Alliance));
  Battle battle(builder.Snapshot(), Uncommanded());
  BattleState& state = Mutable(battle);
  SB_CHECK(state.Squadron(docked)->place == SquadronPlace::Docked);
  state.Ship(carrier)->shield = 0.0f;
  sim::DamageShip(state, carrier, 5000.0f, ShotKind::Turbolaser, Subsystem::Hull);
  SB_CHECK(state.units[carrier].fate == UnitFate::Destroyed);
  SB_CHECK(state.units[docked].fate == UnitFate::Destroyed);
  const BattleResultSet results = battle.Results();
  SB_CHECK(results.ships[0].hull == 0 && results.ships[0].fate == UnitFate::Destroyed);
  SB_CHECK(results.squadrons[0].size == 0);
}

} // namespace

int main()
{
  ShieldsAbsorbThenHull();
  RechargeIsAThreshold();
  OutputScalesWithHullAndCriticals();
  SubsystemFocusTradesHullForCriticals();
  HyperdriveTakesOneHitPerDrive();
  IonStripsShieldsThenDisrupts();
  SustainedIonAllowsAChargedJump();
  TorpedoesQuarterShieldsFullHull();
  DamageControlRepairsOnItsInterval();
  DestroyedCarrierTakesDockedSquadrons();
  std::puts("space battle damage passed");
  return 0;
}
