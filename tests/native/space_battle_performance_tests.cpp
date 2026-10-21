#include "space_battle_test_support.h"

using namespace sbtest;

// The per-tick budget of §11.3: 60 capital ships and 100 squadrons (about 1,200 craft) under 2 ms on the host.
int main()
{
  SnapshotBuilder builder(2024);
  for (Side side : {Side::Alliance, Side::Empire}) {
    std::vector<UnitIndex> carriers;
    for (int i = 0; i < 30; ++i) {
      UnitIndex ship = kNoUnit;
      switch (i % 5) {
        case 0:
          ship = builder.Add(Wedge(side, "Wedge"));
          carriers.push_back(ship);
          break;
        case 1:
          ship = builder.Add(Broadside(side, "Broadside"));
          break;
        case 2:
          ship = builder.Add(Picket(side, "Picket"));
          break;
        case 3:
          ship = builder.Add(CarrierShip(side, "Carrier"));
          carriers.push_back(ship);
          break;
        default:
          ship = builder.Add(Broadside(side, "Broadside"));
          break;
      }
    }
    for (int i = 0; i < 50; ++i) {
      const UnitIndex carrier = carriers[static_cast<size_t>(i) % carriers.size()];
      switch (i % 3) {
        case 0:
          builder.Add(XwingLike(side, carrier, "Multirole"));
          break;
        case 1:
          builder.Add(TieLike(side, carrier, "Interceptor"));
          break;
        default:
          builder.Add(BomberLike(side, carrier, "Bomber"));
          break;
      }
    }
  }
  Battle battle(builder.Snapshot(), BattleOptions{});
  int ships = 0;
  int squadrons = 0;
  for (const UnitState& unit : battle.State().units) {
    ships += unit.kind == UnitKind::CapitalShip ? 1 : 0;
    squadrons += unit.kind == UnitKind::Squadron ? 1 : 0;
  }
  SB_CHECK(ships == 60 && squadrons == 100);
  // Let the fleets close and every hangar empty, then time the thick of the battle.
  battle.RunHeadless(70 * kTicksPerSecond);
  int flying = 0;
  for (const SquadronState& squadron : battle.State().squadrons) {
    flying += squadron.place == SquadronPlace::InSpace ? 1 : 0;
  }
  const int ticks = 40 * kTicksPerSecond;
  size_t shots = 0;
  double worst = 0.0;
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < ticks; ++i) {
    const auto before = std::chrono::steady_clock::now();
    battle.Step();
    worst = std::max(worst, Milliseconds(std::chrono::steady_clock::now() - before));
    shots += battle.State().shots.size();
  }
  SB_CHECK(!battle.Finished());
  const double perTick = Milliseconds(std::chrono::steady_clock::now() - start) / ticks;
  std::printf("performance: %.3f ms per tick, worst %.3f ms (60 ships, 100 squadrons, %d flying, %zu shots in flight on "
              "average)\n",
              perTick, worst, flying, shots / static_cast<size_t>(ticks));
  SB_CHECK(flying >= 60);
  SB_CHECK(perTick < 2.0);
  std::puts("space battle performance passed");
  return 0;
}
