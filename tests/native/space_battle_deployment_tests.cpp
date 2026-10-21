#include "space_battle_test_support.h"

#include <array>
#include <bit>

#include "space_battle/tuning.h"

using namespace sbtest;
namespace tuning = reb::battle::tuning;

namespace
{
constexpr float kGeometryTolerance = 0.1f;

CapitalShipEntry HeavyShip(Side side)
{
  CapitalShipEntry ship = Wedge(side);
  ship.identity.typeIndex = 20;
  return ship;
}

void CheckHull(const BattleState& state, UnitIndex index)
{
  const UnitState& unit = state.units[index];
  const SideState& side = state.sides[SideIndex(unit.side)];
  const float radius = state.Profile(index).radius;
  SB_CHECK(Distance(unit.position, side.deploymentCenter) + radius <= side.deploymentRadius + kGeometryTolerance);
  SB_CHECK(std::fabs(unit.position.x) + radius <= kBattlePlaneHalfExtent + kGeometryTolerance);
  SB_CHECK(std::fabs(unit.position.y) + radius <= kBattlePlaneHalfExtent + kGeometryTolerance);
  SB_CHECK(Distance(unit.position, state.planetCenter) + kGeometryTolerance >= tuning::kPlanetRadius + radius);
}

void CheckLayout(const Battle& battle)
{
  const BattleState& state = battle.State();
  ViewModel view;
  battle.BuildViewModel(0.0f, view);
  for (Side side : {Side::Alliance, Side::Empire}) {
    const SideState& deployment = state.sides[SideIndex(side)];
    SB_CHECK(view.deployZones[SideIndex(side)].center == deployment.deploymentCenter);
    SB_CHECK(view.deployZones[SideIndex(side)].radius == deployment.deploymentRadius);
    SB_CHECK(DeploymentCenter(state, side) == deployment.deploymentCenter);
    const Vec2 forward = side == state.attacker ? -state.arrivalDirection : state.arrivalDirection;
    const float front = Dot(deployment.deploymentCenter, forward) + deployment.deploymentRadius;
    const float originalFront = -(side == state.attacker ? tuning::kAttackerDeployDistance
                                                       : tuning::kDefenderDeployDistance)
                                + tuning::kDeployZoneRadius;
    SB_CHECK(front <= originalFront + kGeometryTolerance);
  }
  for (const ShipState& ship : state.ships) {
    CheckHull(state, ship.unit);
    const Vec2 forward = state.units[ship.unit].side == state.attacker ? -state.arrivalDirection : state.arrivalDirection;
    SB_CHECK_NEAR(AngleDelta(state.units[ship.unit].heading, AngleOf(forward)), 0.0f, 1e-6f);
  }
}

void LargeFleetsFitAtEveryBearing()
{
  for (const bool mixed : {false, true}) {
    for (Side defender : {Side::Alliance, Side::Empire}) {
      for (float bearing : {0.0f, 0.37f, 0.25f * kPi, 0.5f * kPi, 1.17f * kPi}) {
        SnapshotBuilder builder;
        builder.Params().heldFirstBy = defender;
        for (Side side : {Side::Alliance, Side::Empire}) {
          for (int i = 0; i < (mixed ? 30 : 40); ++i) {
            if (mixed && i >= 20) {
              builder.Add(i < 25 ? CarrierShip(side) : TransportShip(side));
            } else {
              builder.Add(HeavyShip(side));
            }
          }
        }
        BattleOptions options = Uncommanded();
        options.attackerBearing = bearing;
        Battle battle(builder.Snapshot(), options);
        CheckLayout(battle);
        if (mixed) {
          for (Side side : {Side::Alliance, Side::Empire}) {
            const Vec2 forward = side == battle.State().attacker ? -battle.State().arrivalDirection
                                                                : battle.State().arrivalDirection;
            float lastFront = 1e9f;
            float firstBack = -1e9f;
            for (const ShipState& ship : battle.State().ships) {
              if (battle.State().units[ship.unit].side != side) {
                continue;
              }
              const float depth = Dot(battle.State().units[ship.unit].position, forward);
              const Archetype archetype = battle.State().Profile(ship.unit).archetype;
              if (archetype == Archetype::Carrier || archetype == Archetype::Transport) {
                firstBack = std::max(firstBack, depth);
              } else {
                lastFront = std::min(lastFront, depth);
              }
            }
            SB_CHECK(lastFront > firstBack);
          }
        }
      }
    }
  }
}

void CheckBits(float actual, float expected)
{
  SB_CHECK(std::bit_cast<uint32_t>(actual) == std::bit_cast<uint32_t>(expected));
}

void SmallFleetsKeepTheLegacyLayout()
{
  for (float bearing : {0.0f, 0.37f, 0.5f * kPi}) {
    SnapshotBuilder builder;
    for (Side side : {Side::Alliance, Side::Empire}) {
      builder.Add(Wedge(side));
      builder.Add(Wedge(side));
      builder.Add(Broadside(side));
      builder.Add(Picket(side));
      builder.Add(CarrierShip(side));
      builder.Add(TransportShip(side));
    }
    BattleOptions options = Uncommanded();
    options.attackerBearing = bearing;
    Battle battle(builder.Snapshot(), options);
    const BattleState& state = battle.State();
    CheckLayout(battle);
    for (Side side : {Side::Alliance, Side::Empire}) {
      const Vec2 forward = side == state.attacker ? -state.arrivalDirection : state.arrivalDirection;
      const Vec2 left = Rotated(forward, 0.5f * kPi);
      const Vec2 center = state.arrivalDirection * (side == state.attacker ? tuning::kAttackerDeployDistance
                                                                       : -tuning::kDefenderDeployDistance);
      const SideState& deployment = state.sides[SideIndex(side)];
      SB_CHECK(deployment.deploymentCenter == center);
      SB_CHECK(deployment.deploymentRadius == tuning::kDeployZoneRadius);
      for (bool back : {false, true}) {
        std::vector<const GroupState*> row;
        std::vector<float> spacings;
        float width = 0.0f;
        for (const GroupState& group : state.groups) {
          if (group.side != side || group.flight || group.members.empty()) {
            continue;
          }
          const Archetype archetype = state.Profile(group.members.front()).archetype;
          if ((archetype == Archetype::Carrier || archetype == Archetype::Transport) != back) {
            continue;
          }
          float radius = 0.0f;
          for (UnitIndex member : group.members) {
            radius = std::max(radius, state.Profile(member).radius);
          }
          const float spacing = tuning::kFormationSpacing + 2.0f * radius;
          row.push_back(&group);
          spacings.push_back(spacing);
          width += spacing * static_cast<float>(group.members.size());
        }
        float cursor = -0.5f * width;
        const Vec2 rowCenter = back ? center - forward * tuning::kDeploymentRowSpacing : center;
        for (size_t i = 0; i < row.size(); ++i) {
          for (UnitIndex member : row[i]->members) {
            const Vec2 expected = rowCenter + left * (cursor + 0.5f * spacings[i]);
            CheckBits(state.units[member].position.x, expected.x);
            CheckBits(state.units[member].position.y, expected.y);
            CheckBits(state.units[member].heading, AngleOf(forward));
            cursor += spacings[i];
          }
        }
      }
    }
  }
}

void DeployPreservesRigidFormation()
{
  for (Side side : {Side::Alliance, Side::Empire}) {
    for (const std::optional<float> facing : {std::optional<float>{}, std::optional<float>{0.5f * kPi}}) {
      SnapshotBuilder builder;
      std::vector<UnitIndex> units;
      for (int i = 0; i < 10; ++i) {
        units.push_back(builder.Add(HeavyShip(side)));
      }
      builder.Add(Wedge(Opponent(side)));
      Battle battle(builder.Snapshot(), Uncommanded());
      std::vector<Vec2> original;
      for (UnitIndex unit : units) {
        original.push_back(battle.State().units[unit].position);
      }
      const Vec2 center = DeploymentCenter(battle.State(), side);
      for (Vec2 target : {center + Vec2{0.0f, 50000.0f}, battle.State().planetCenter,
                          center + Vec2{50000.0f, -50000.0f}, center}) {
        std::vector<Vec2> before;
        for (UnitIndex unit : units) {
          before.push_back(battle.State().units[unit].position);
        }
        const float turn = facing ? AngleDelta(battle.State().units[units.front()].heading, *facing) : 0.0f;
        SB_CHECK(battle.Submit(MakeCommand(side, OrderDeploy{target, facing}, units)));
        for (size_t i = 0; i < units.size(); ++i) {
          CheckHull(battle.State(), units[i]);
          if (facing) {
            SB_CHECK_NEAR(AngleDelta(battle.State().units[units[i]].heading, *facing), 0.0f, 1e-5f);
          }
          const Vec2 expectedOffset = Rotated(before[i] - before.front(), turn);
          const Vec2 actualOffset = battle.State().units[units[i]].position - battle.State().units[units.front()].position;
          SB_CHECK_NEAR(actualOffset.x, expectedOffset.x, 0.05f);
          SB_CHECK_NEAR(actualOffset.y, expectedOffset.y, 0.05f);
          for (size_t j = 0; j < i; ++j) {
            SB_CHECK_NEAR(Distance(battle.State().units[units[i]].position, battle.State().units[units[j]].position),
                          Distance(original[i], original[j]), 0.05f);
          }
        }
      }
    }
  }
}

void SaveRestoresDeploymentZones()
{
  SnapshotBuilder builder;
  for (Side side : {Side::Alliance, Side::Empire}) {
    for (int i = 0; i < 40; ++i) {
      builder.Add(HeavyShip(side));
    }
  }
  const BattleOptions options = Uncommanded();
  Battle battle(builder.Snapshot(), options);
  SB_CHECK(std::any_of(battle.State().sides.begin(), battle.State().sides.end(),
                       [](const SideState& side) { return side.deploymentRadius > tuning::kDeployZoneRadius; }));
  battle.RunHeadless(10);
  const std::unique_ptr<Battle> restored = Battle::RestoreState(builder.Snapshot(), options, battle.SaveState());
  SB_CHECK(restored != nullptr);
  for (Side side : {Side::Alliance, Side::Empire}) {
    const SideState& before = battle.State().sides[SideIndex(side)];
    const SideState& after = restored->State().sides[SideIndex(side)];
    SB_CHECK(before.deploymentCenter == after.deploymentCenter);
    SB_CHECK(before.deploymentRadius == after.deploymentRadius);
  }
  SB_CHECK(battle.StateHash() == restored->StateHash());
}
void OversizedFleetsStillStart()
{
  SnapshotBuilder builder;
  for (int i = 0; i < 400; ++i) {
    builder.Add(HeavyShip(Side::Alliance));
  }
  builder.Add(HeavyShip(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  for (const UnitState& unit : battle.State().units) {
    SB_CHECK(std::isfinite(unit.position.x) && std::isfinite(unit.position.y));
  }
  for (const SideState& side : battle.State().sides) {
    SB_CHECK(std::isfinite(side.deploymentRadius) && side.deploymentRadius >= tuning::kDeployZoneRadius);
  }
}
} // namespace

int main()
{
  LargeFleetsFitAtEveryBearing();
  SmallFleetsKeepTheLegacyLayout();
  DeployPreservesRigidFormation();
  SaveRestoresDeploymentZones();
  OversizedFleetsStillStart();
  std::puts("space battle deployment passed");
  return 0;
}
