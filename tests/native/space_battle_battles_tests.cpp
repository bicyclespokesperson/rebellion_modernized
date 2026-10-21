#include "space_battle_test_support.h"

#include "space_battle/tuning.h"

using namespace sbtest;
namespace tuning = reb::battle::tuning;

namespace
{
BattleSnapshot LineBattle(int alliance, int empire, uint32_t seed)
{
  SnapshotBuilder builder(seed);
  for (int i = 0; i < alliance; ++i) {
    builder.Add(Broadside(Side::Alliance, "Broadside " + std::to_string(i + 1)));
  }
  for (int i = 0; i < empire; ++i) {
    builder.Add(Wedge(Side::Empire, "Wedge " + std::to_string(i + 1)));
  }
  return builder.Snapshot();
}

int Surviving(const BattleResultSet& results, const BattleSnapshot& snapshot, Side side)
{
  int count = 0;
  for (const ShipResult& ship : results.ships) {
    const auto& entry = std::get<CapitalShipEntry>(snapshot.units[ship.unit]);
    count += entry.identity.side == side && ship.fate != UnitFate::Destroyed ? 1 : 0;
  }
  return count;
}

void CapitalBattleRunsToAResult()
{
  const BattleSnapshot snapshot = LineBattle(3, 3, 11);
  Battle battle(snapshot, BattleOptions{});
  SB_CHECK(battle.RunHeadless(battle.Options().tickLimit + 1));
  const BattleResultSet results = battle.Results();
  std::printf("3 v 3 ended after %.0f s: alliance %d, empire %d ships left\n", battle.State().BattleSeconds(),
              Surviving(results, snapshot, Side::Alliance), Surviving(results, snapshot, Side::Empire));
  SB_CHECK(results.ships.size() == 6);
  SB_CHECK(results.outcome.alliancePresent != results.outcome.empirePresent
           || battle.State().endReason != EndReason::Elimination);
  for (const ShipResult& ship : results.ships) {
    SB_CHECK((ship.fate == UnitFate::Destroyed) == (ship.hull == 0));
    SB_CHECK(ship.criticals.weaponRecharge >= 0 && ship.criticals.weaponRecharge <= 4);
  }
  // Pacing (§6.1): a 3 v 3 line battle takes minutes, not seconds or the whole time limit.
  SB_CHECK(battle.State().BattleSeconds() > 90.0f && battle.State().BattleSeconds() < 600.0f);
  SB_CHECK(!battle.State().events.empty());
}

void StrongerFleetWins()
{
  const BattleSnapshot snapshot = LineBattle(1, 4, 5);
  Battle battle(snapshot, BattleOptions{});
  battle.RunHeadless(battle.Options().tickLimit + 1);
  const BattleResultSet results = battle.Results();
  SB_CHECK(battle.State().endReason == EndReason::Elimination);
  SB_CHECK(!results.outcome.alliancePresent && results.outcome.empirePresent);
  SB_CHECK(Surviving(results, snapshot, Side::Empire) >= 3);
}

void SameInputsSameHash()
{
  const BattleSnapshot snapshot = LineBattle(2, 2, 99);
  Battle first(snapshot, BattleOptions{});
  Battle second(snapshot, BattleOptions{});
  SB_CHECK(first.StateHash() == second.StateHash());
  for (int tick = 0; tick < 1200; ++tick) {
    first.Step();
    second.Step();
  }
  SB_CHECK(first.StateHash() == second.StateHash());
  first.RunHeadless(100000);
  second.RunHeadless(100000);
  SB_CHECK(first.StateHash() == second.StateHash());
  SB_CHECK(first.Tick() == second.Tick());
  Battle reseeded(LineBattle(2, 2, 100), BattleOptions{});
  for (int tick = 0; tick < 1200; ++tick) {
    reseeded.Step();
  }
  Battle reference(snapshot, BattleOptions{});
  for (int tick = 0; tick < 1200; ++tick) {
    reference.Step();
  }
  SB_CHECK(reseeded.StateHash() != reference.StateHash());
}

// Auto keeps fire on a target only until enough is committed to kill it in the window; the rest spills (§5.3).
void AllocatorAvoidsOverkill()
{
  for (const FireAllocation allocation : {FireAllocation::Auto, FireAllocation::Concentrate, FireAllocation::Spread}) {
    SnapshotBuilder builder;
    std::vector<UnitIndex> attackers;
    for (int i = 0; i < 4; ++i) {
      attackers.push_back(builder.Add(Wedge(Side::Empire, "Attacker")));
    }
    const UnitIndex weak = builder.Add(Broadside(Side::Alliance, "Weak"));
    const UnitIndex strong = builder.Add(Broadside(Side::Alliance, "Strong"));
    Battle battle(builder.Snapshot(), Uncommanded());
    BattleState& state = Mutable(battle);
    for (size_t i = 0; i < attackers.size(); ++i) {
      Place(battle, attackers[i], {-1200.0f + 800.0f * static_cast<float>(i), -3000.0f}, 0.5f * kPi);
    }
    Place(battle, weak, {-600.0f, 0.0f}, 0.0f);
    Place(battle, strong, {600.0f, 0.0f}, 0.0f);
    state.Ship(weak)->hull = 150.0f;
    state.Ship(weak)->shield = 0.0f;
    GroupState& group = state.groups[static_cast<size_t>(state.units[attackers[0]].group)];
    SB_CHECK(group.members.size() == 4);
    group.shipDoctrine.allocation = allocation;
    sim::ScoreGroupTargets(state, group);
    sim::AllocateFire(state, group);
    int onWeak = 0;
    int onStrong = 0;
    for (UnitIndex attacker : attackers) {
      onWeak += state.Ship(attacker)->primaryTarget == weak ? 1 : 0;
      onStrong += state.Ship(attacker)->primaryTarget == strong ? 1 : 0;
    }
    std::printf("allocation %d: %d on the damaged ship, %d on the healthy one\n", static_cast<int>(allocation), onWeak,
                onStrong);
    SB_CHECK(onWeak + onStrong == 4);
    switch (allocation) {
      case FireAllocation::Auto:
        SB_CHECK(onWeak >= 1 && onStrong >= 1);
        SB_CHECK(onWeak <= 2);
        break;
      case FireAllocation::Concentrate:
        SB_CHECK(onWeak == 4 || onStrong == 4);
        break;
      case FireAllocation::Spread:
        SB_CHECK(onWeak == 4); // every ship picks its own best target: overkill
        break;
    }
  }
}

// Pickets cannot outpace each other's shield recharge: the battle ends in a stalemate with both sides present.
void StalemateEndsWithBothPresent()
{
  SnapshotBuilder builder;
  builder.Add(Picket(Side::Alliance));
  builder.Add(Picket(Side::Empire));
  Battle battle(builder.Snapshot(), BattleOptions{});
  SB_CHECK(battle.RunHeadless(battle.Options().tickLimit + 1));
  SB_CHECK(battle.State().endReason == EndReason::Stalemate);
  SB_CHECK(battle.Tick() < battle.Options().tickLimit);
  const BattleResultSet results = battle.Results();
  SB_CHECK(results.outcome.alliancePresent && results.outcome.empirePresent);
  SB_CHECK(!results.outcome.allianceWithdrew && !results.outcome.empireWithdrew);
  for (const ShipResult& ship : results.ships) {
    SB_CHECK(ship.fate == UnitFate::Active);
  }
}

void NegligibleChipDamageDoesNotProlongAStalemate()
{
  SnapshotBuilder builder;
  const UnitIndex target = builder.Add(Broadside(Side::Alliance));
  builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  BattleState& state = Mutable(battle);
  const int limit = sim::Ticks(tuning::kStalemateSeconds);
  for (int tick = 0; tick <= limit && !state.finished; ++tick) {
    state.tick = tick;
    state.Ship(target)->hull -= 0.001f;
    sim::UpdateEnding(state);
  }
  SB_CHECK(state.endReason == EndReason::Stalemate);
  SB_CHECK(state.tick == limit);
  SB_CHECK(battle.Results().outcome.alliancePresent && battle.Results().outcome.empirePresent);
  SB_CHECK(state.Ship(target)->hull > 2290.0f);
}

void MeaningfulAttritionKeepsTheBattleRunning()
{
  SnapshotBuilder builder;
  const UnitIndex target = builder.Add(Broadside(Side::Alliance));
  builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  BattleState& state = Mutable(battle);
  for (int tick = 0; tick <= sim::Ticks(2.0f * tuning::kStalemateSeconds); ++tick) {
    state.tick = tick;
    state.Ship(target)->hull -= 0.05f;
    sim::UpdateEnding(state);
  }
  SB_CHECK(!state.finished);
}

// Hull worn off craft counts as progress even before any craft is lost: point defence spreading its fire over a
// squadron is breaking it, not a stalemate.
void CraftHullDamageKeepsTheBattleRunning()
{
  SnapshotBuilder builder;
  builder.Params().heldFirstBy = Side::Empire; // the Alliance attacks: its squadron starts in space
  const UnitIndex squadron = builder.Add(XwingLike(Side::Alliance, kNoUnit));
  builder.Add(Picket(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  BattleState& state = Mutable(battle);
  const SquadronState& flight = *state.Squadron(squadron);
  for (int tick = 0; tick <= sim::Ticks(2.0f * tuning::kStalemateSeconds); ++tick) {
    state.tick = tick;
    state.craft[flight.firstCraft + tick % flight.craftCount].hull -= 0.005f; // 12 hull per 120 s, 2 per craft
    sim::UpdateEnding(state);
  }
  SB_CHECK(!state.finished && state.endReason == EndReason::None);
  SB_CHECK(flight.alive == flight.initialAlive);
}

void TimeLimitEndsWithBothPresent()
{
  BattleOptions options;
  options.tickLimit = 40 * kTicksPerSecond;
  Battle battle(LineBattle(3, 3, 3), options);
  SB_CHECK(battle.RunHeadless(100000));
  SB_CHECK(battle.State().endReason == EndReason::TimeLimit);
  SB_CHECK(battle.Tick() == options.tickLimit);
  const BattleResultSet results = battle.Results();
  SB_CHECK(results.outcome.alliancePresent && results.outcome.empirePresent);
}

// The side threshold (the vote's strength ratio) withdraws the fleet; 10000 is the Retreat vote: leave at once.
void SideThresholdWithdrawsTheFleet()
{
  BattleSnapshot snapshot = LineBattle(2, 3, 8);
  snapshot.params.sides[0].withdrawThreshold = 10000.0f;
  Battle battle(snapshot, BattleOptions{});
  SB_CHECK(battle.RunHeadless(battle.Options().tickLimit + 1));
  const BattleResultSet results = battle.Results();
  SB_CHECK(!results.outcome.alliancePresent && results.outcome.allianceWithdrew);
  SB_CHECK(results.outcome.empirePresent);
  int withdrawn = 0;
  for (const ShipResult& ship : results.ships) {
    withdrawn += ship.fate == UnitFate::Withdrawn ? 1 : 0;
    if (ship.fate == UnitFate::Withdrawn) {
      SB_CHECK(ship.hull > 0);
    }
  }
  SB_CHECK(withdrawn == 2);
  // A human side's vote carries 0; under the AI it uses the fleet-withdrawal default instead.
  BattleSnapshot human = LineBattle(1, 4, 8);
  human.params.sides[0].human = true;
  BattleOptions options;
  options.humanSideAiWithdrawThreshold = 0.9f;
  Battle observed(human, options);
  observed.RunHeadless(observed.Options().tickLimit + 1);
  SB_CHECK(observed.Results().outcome.allianceWithdrew);
}

// The view model interpolates between ticks and carries what the overlay draws (§10.1).
void ViewModelDescribesTheBattle()
{
  SnapshotBuilder builder(12);
  const UnitIndex alliance = builder.Add(Broadside(Side::Alliance, "Liberty"));
  const UnitIndex carrier = builder.Add(CarrierShip(Side::Alliance, "Carrier"));
  builder.Add(XwingLike(Side::Alliance, carrier, "Red"));
  const UnitIndex empire = builder.Add(Wedge(Side::Empire, "Tyrant"));
  Battle battle(builder.Snapshot(), Uncommanded());
  battle.RunHeadless(15 * kTicksPerSecond);
  ViewModel launched;
  battle.BuildViewModel(1.0f, launched);
  SB_CHECK(launched.craft.size() == 12);
  battle.RunHeadless(30 * kTicksPerSecond);
  const BattleState& state = battle.State();
  ViewModel before;
  ViewModel after;
  ViewModel middle;
  battle.BuildViewModel(0.0f, before);
  battle.BuildViewModel(1.0f, after);
  battle.BuildViewModel(0.5f, middle);
  SB_CHECK(after.tick == battle.Tick());
  SB_CHECK(after.battleSeconds > before.battleSeconds);
  SB_CHECK(after.boundsMax.x == kBattlePlaneHalfExtent && after.planetRadius > 0.0f);
  const auto find = [](const ViewModel& view, UnitIndex unit) {
    return *std::find_if(view.units.begin(), view.units.end(), [&](const ViewUnit& entry) { return entry.unit == unit; });
  };
  SB_CHECK(find(before, alliance).position == state.units[alliance].previousPosition);
  SB_CHECK(find(after, alliance).position == state.units[alliance].position);
  const Vec2 halfway = Lerp(state.units[alliance].previousPosition, state.units[alliance].position, 0.5f);
  SB_CHECK(Distance(find(middle, alliance).position, halfway) < 1e-2f);
  const ViewUnit ship = find(after, empire);
  SB_CHECK(ship.radius == state.Profile(empire).radius && ship.side == Side::Empire);
  SB_CHECK(!ship.intent.empty());
  SB_CHECK(ship.target != kNoUnit);
  SB_CHECK(Length(ship.velocity) >= 0.0f);
  SB_CHECK(std::all_of(after.shots.begin(), after.shots.end(),
                       [](const ViewShot& shot) { return shot.progress >= 0.0f && shot.progress <= 1.0f; }));
  SB_CHECK(after.groups.size() >= 3);
  SB_CHECK(after.sides[0].strength > 0.0f && after.sides[0].initialStrength >= after.sides[0].strength * 0.5f);
  SB_CHECK(!after.recentEvents.empty() && after.recentEvents.front().text.find("launched") != std::string::npos);
  SB_CHECK(after.superlaserCharge < 0.0f && !after.deathStarRunOpen);
  const ViewUnitDetail detail = battle.Detail(carrier);
  SB_CHECK(detail.name == "Carrier" && detail.hangarCapacity == 6 && detail.hangarUsed == 1);
  SB_CHECK(detail.className == std::string(ArchetypeName(Archetype::Carrier)));
  bool firing = false;
  for (int tick = 0; tick < 60 * kTicksPerSecond && !firing; ++tick) {
    battle.Step();
    battle.BuildViewModel(1.0f, after);
    firing = std::any_of(after.units.begin(), after.units.end(), [](const ViewUnit& unit) { return unit.firing; });
  }
  SB_CHECK(firing);
}

} // namespace

int main()
{
  CapitalBattleRunsToAResult();
  StrongerFleetWins();
  SameInputsSameHash();
  AllocatorAvoidsOverkill();
  StalemateEndsWithBothPresent();
  NegligibleChipDamageDoesNotProlongAStalemate();
  MeaningfulAttritionKeepsTheBattleRunning();
  CraftHullDamageKeepsTheBattleRunning();
  TimeLimitEndsWithBothPresent();
  SideThresholdWithdrawsTheFleet();
  ViewModelDescribesTheBattle();
  std::puts("space battle battles passed");
  return 0;
}
