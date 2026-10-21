#include "space_battle_test_support.h"

#include "space_battle/ship_profile.h"
#include "space_battle/tuning.h"

using namespace sbtest;

namespace
{
constexpr float kDegree = reb::battle::tuning::kDegrees;

void ArchetypesComeFromTheStats()
{
  SnapshotBuilder builder;
  const UnitIndex wedge = builder.Add(Wedge(Side::Empire));
  const UnitIndex broadside = builder.Add(Broadside(Side::Alliance));
  const UnitIndex picket = builder.Add(Picket(Side::Empire));
  const UnitIndex carrier = builder.Add(CarrierShip(Side::Empire));
  const UnitIndex interdictor = builder.Add(InterdictorShip(Side::Empire));
  const UnitIndex transport = builder.Add(TransportShip(Side::Alliance));
  const UnitIndex tie = builder.Add(TieLike(Side::Empire, carrier));
  const UnitIndex xwing = builder.Add(XwingLike(Side::Alliance, kNoUnit));
  const UnitIndex bomber = builder.Add(BomberLike(Side::Alliance, kNoUnit));
  const auto profiles = BuildProfiles(builder.Snapshot());
  SB_CHECK(profiles[wedge].archetype == Archetype::BowLine);
  SB_CHECK(profiles[wedge].facing == FacingStyle::Bow);
  SB_CHECK(profiles[broadside].archetype == Archetype::BroadsideLine);
  SB_CHECK(profiles[broadside].facing == FacingStyle::Broadside);
  SB_CHECK(profiles[picket].archetype == Archetype::Picket);
  SB_CHECK(profiles[picket].facing == FacingStyle::Omni);
  SB_CHECK(profiles[carrier].archetype == Archetype::Carrier);
  SB_CHECK(profiles[interdictor].archetype == Archetype::Interdictor);
  SB_CHECK(profiles[transport].archetype == Archetype::Transport);
  SB_CHECK(profiles[tie].archetype == Archetype::Interceptor);
  SB_CHECK(profiles[xwing].archetype == Archetype::Multirole);
  SB_CHECK(profiles[bomber].archetype == Archetype::Bomber);

  SB_CHECK(profiles[wedge].staticTags & TagBit(TargetTag::Heavy));
  SB_CHECK(profiles[wedge].staticTags & TagBit(TargetTag::Tractor));
  SB_CHECK(!(profiles[wedge].staticTags & TagBit(TargetTag::Carrier))); // hangars, but nothing aboard
  SB_CHECK(profiles[carrier].staticTags & TagBit(TargetTag::Carrier));
  SB_CHECK(profiles[picket].staticTags & TagBit(TargetTag::Picket));
  SB_CHECK(profiles[picket].staticTags & TagBit(TargetTag::Light));
  SB_CHECK(profiles[interdictor].staticTags & TagBit(TargetTag::Interdictor));
  SB_CHECK(profiles[transport].staticTags & TagBit(TargetTag::Transport));
  SB_CHECK(profiles[bomber].staticTags & TagBit(TargetTag::Bomber));
  SB_CHECK(!(profiles[xwing].staticTags & TagBit(TargetTag::Bomber)));
  SB_CHECK(profiles[tie].staticTags & TagBit(TargetTag::Fighter));
  SB_CHECK(!profiles[tie].CanJumpAlone());
  SB_CHECK(profiles[xwing].CanJumpAlone());
}

void BestArcDealsTheLegacyCreditAndOtherArcsAdd()
{
  SnapshotBuilder builder;
  const UnitIndex wedge = builder.Add(Wedge(Side::Empire));
  const UnitIndex picket = builder.Add(Picket(Side::Empire));
  const auto profiles = BuildProfiles(builder.Snapshot());
  const float scale = reb::battle::tuning::kLegacyStepsPerSecond;
  // Fore arc: 120 turbolaser + 80 ion, all at factor 1: the legacy credit is the full weapon recharge rate.
  SB_CHECK_NEAR(profiles[wedge].arcVsCapital[0], 20.0f * scale, 1e-3f);
  SB_CHECK_NEAR(profiles[wedge].bestArcVsCapital, 20.0f * scale, 1e-3f);
  // Each further arc adds its own points at the same rate: 100 points per broadside = half the fore arc.
  SB_CHECK_NEAR(profiles[wedge].arcVsCapital[2], 10.0f * scale, 1e-3f);
  SB_CHECK_NEAR(profiles[wedge].arcVsCapital[3], 10.0f * scale, 1e-3f);
  // Against fighters only turbolasers count, at half, with the calibrated point-defence output.
  SB_CHECK_NEAR(profiles[wedge].arcVsFighter[0], 120.0f * 0.5f * (20.0f / 200.0f) * scale * reb::battle::tuning::kPointDefenceDamageScale, 1e-3f);
  // A laser picket retains its anti-capital rate; point defence uses the same calibrated output in every arc.
  for (int arc = 0; arc < kArcCount; ++arc) {
    SB_CHECK_NEAR(profiles[picket].arcVsCapital[arc], 10.0f / 6.0f * scale, 1e-3f);
    SB_CHECK_NEAR(profiles[picket].arcVsFighter[arc], 10.0f * scale * reb::battle::tuning::kPointDefenceDamageScale, 1e-3f);
  }
  const float point = kMetersPerRangePoint;
  SB_CHECK(profiles[wedge].batteryCount == 8);
  SB_CHECK_NEAR(profiles[wedge].maxRange, 50.0f * point, 1e-2f);
  // Optimal: turbolasers alone carry less than 80% of the firepower, so the wedge holds at its ion range.
  SB_CHECK_NEAR(profiles[wedge].optimalRange, 36.0f * point * 0.9f, 1e-2f);
  SB_CHECK_NEAR(profiles[picket].closeRange, 18.0f * point * 0.9f, 1e-2f);
}

void ArcGeometryOverlapsAtTheCorners()
{
  SB_CHECK(InArc(Arc::Fore, 0.0f));
  SB_CHECK(InArc(Arc::Fore, 45.0f * kDegree) && InArc(Arc::Port, 45.0f * kDegree));
  SB_CHECK(InArc(Arc::Fore, -45.0f * kDegree) && InArc(Arc::Starboard, -45.0f * kDegree));
  SB_CHECK(!InArc(Arc::Fore, 55.0f * kDegree) && InArc(Arc::Port, 55.0f * kDegree));
  SB_CHECK(InArc(Arc::Aft, 180.0f * kDegree) && InArc(Arc::Aft, -170.0f * kDegree));
  SB_CHECK(!InArc(Arc::Aft, 120.0f * kDegree) && InArc(Arc::Port, 120.0f * kDegree));
  SB_CHECK(InArc(Arc::Starboard, -90.0f * kDegree) && !InArc(Arc::Port, -90.0f * kDegree));
}

// Sizes follow the class-length table by type index (presentation key), else a hull-based fallback.
void RadiusFollowsClassLengths()
{
  SnapshotBuilder builder;
  CapitalShipEntry super = Wedge(Side::Empire, "Super");
  super.identity.typeIndex = 21;
  CapitalShipEntry destroyer = Wedge(Side::Empire, "Destroyer");
  destroyer.identity.typeIndex = 20;
  const UnitIndex superIndex = builder.Add(super);
  const UnitIndex destroyerIndex = builder.Add(destroyer);
  const UnitIndex wedge = builder.Add(Wedge(Side::Empire));
  const UnitIndex picket = builder.Add(Picket(Side::Empire));
  const UnitIndex station = builder.AddDeathStar();
  const auto profiles = BuildProfiles(builder.Snapshot());
  SB_CHECK(profiles[destroyerIndex].radius == 801.0f);
  SB_CHECK(profiles[superIndex].radius > 2.9f * profiles[destroyerIndex].radius);
  SB_CHECK(profiles[picket].radius < profiles[wedge].radius);
  SB_CHECK(profiles[wedge].radius > 500.0f && profiles[wedge].radius < 1100.0f);
  SB_CHECK(profiles[station].radius == reb::battle::tuning::kDeathStarRadius);
}

// Auto facing: the bow ship points its bow, the broadside ship turns side-on, the omni picket keeps its heading.
void AutoFacingFollowsTheGrid()
{
  SnapshotBuilder builder;
  const UnitIndex wedge = builder.Add(Wedge(Side::Empire));
  const UnitIndex broadside = builder.Add(Broadside(Side::Empire));
  const UnitIndex picket = builder.Add(Picket(Side::Empire));
  const UnitIndex enemy = builder.Add(Broadside(Side::Alliance, "Enemy"));
  Battle battle(builder.Snapshot(), Uncommanded());
  BattleState& state = Mutable(battle);
  Place(battle, enemy, {0.0f, 0.0f}, 0.0f);
  const auto relativeBearing = [&](UnitIndex ship, float heading) {
    return std::fabs(AngleDelta(heading, AngleOf(state.units[enemy].position - state.units[ship].position)));
  };
  for (UnitIndex ship : {wedge, broadside, picket}) {
    Place(battle, ship, {-1500.0f + 1500.0f * static_cast<float>(ship), -3000.0f}, 0.0f);
    state.Ship(ship)->primaryTarget = enemy;
  }
  float firepower = 0.0f;
  const float wedgeHeading = sim::ChooseHeading(state, wedge, Facing::Auto, state.units[wedge].position, &firepower);
  SB_CHECK(firepower > 0.0f);
  const float firmly = reb::battle::tuning::kArcHalfWidth - reb::battle::tuning::kFacingArcMargin + 1e-3f;
  SB_CHECK(relativeBearing(wedge, wedgeHeading) <= firmly);
  const float broadsideHeading
      = sim::ChooseHeading(state, broadside, Facing::Auto, state.units[broadside].position, &firepower);
  SB_CHECK(std::fabs(relativeBearing(broadside, broadsideHeading) - 90.0f * kDegree) <= firmly);
  const float picketHeading = sim::ChooseHeading(state, picket, Facing::Auto, state.units[picket].position, &firepower);
  SB_CHECK(std::fabs(AngleDelta(0.0f, picketHeading)) < 1e-3f);
  // Held bow-on, the broadside ship points at its target instead.
  const float bowOn = sim::ChooseHeading(state, broadside, Facing::BowOn, state.units[broadside].position, &firepower);
  SB_CHECK(relativeBearing(broadside, bowOn) <= firmly);
}

} // namespace

int main()
{
  ArchetypesComeFromTheStats();
  BestArcDealsTheLegacyCreditAndOtherArcsAdd();
  ArcGeometryOverlapsAtTheCorners();
  RadiusFollowsClassLengths();
  AutoFacingFollowsTheGrid();
  std::puts("space battle profiles passed");
  return 0;
}
