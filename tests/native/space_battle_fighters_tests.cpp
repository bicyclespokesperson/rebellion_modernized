#include "space_battle_test_support.h"

#include "space_battle/doctrine.h"
#include "space_battle/tuning.h"

using namespace sbtest;
namespace tuning = reb::battle::tuning;

namespace
{
int CraftAlive(const BattleState& state, UnitIndex squadron)
{
  return state.Squadron(squadron)->alive;
}

int Torpedoes(const BattleState& state, UnitIndex unit)
{
  const SquadronState& squadron = *state.Squadron(unit);
  int total = 0;
  for (int i = 0; i < squadron.craftCount; ++i) {
    const CraftState& craft = state.craft[squadron.firstCraft + i];
    total += craft.alive ? craft.torpedoes : 0;
  }
  return total;
}

std::vector<int64_t> LaunchTicks(Battle& battle, const std::vector<UnitIndex>& squadrons, int64_t limit)
{
  std::vector<int64_t> ticks(squadrons.size(), -1);
  while (battle.Tick() < limit) {
    battle.Step();
    for (size_t i = 0; i < squadrons.size(); ++i) {
      if (ticks[i] < 0 && battle.State().Squadron(squadrons[i])->place == SquadronPlace::InSpace) {
        ticks[i] = battle.Tick();
      }
    }
  }
  std::sort(ticks.begin(), ticks.end());
  return ticks;
}

// One squadron per 5 s + 0-7 s from each hangar; Scramble triples the rate (§6.8, §5.6).
void CarriersLaunchOnTheOriginalCadence()
{
  for (const bool scramble : {false, true}) {
    SnapshotBuilder builder;
    const UnitIndex carrier = builder.Add(CarrierShip(Side::Alliance));
    builder.AddCharacter(Side::Alliance, carrier, true, false, 90, 50);
    std::vector<UnitIndex> squadrons;
    for (int i = 0; i < 4; ++i) {
      squadrons.push_back(builder.Add(XwingLike(Side::Alliance, carrier)));
    }
    builder.Add(Picket(Side::Empire));
    Battle battle(builder.Snapshot(), Uncommanded());
    if (scramble) {
      SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderSignal{SignalKind::Scramble, kNoUnit})));
      SB_CHECK(battle.State().sides[0].signalsReady == battle.State().sides[0].signalsMax - 1);
    }
    const std::vector<int64_t> ticks = LaunchTicks(battle, squadrons, 70 * kTicksPerSecond);
    const float slowest = (tuning::kLaunchBaseSeconds + static_cast<float>(tuning::kLaunchRandomSeconds)) * kTicksPerSecond;
    for (size_t i = 0; i < ticks.size(); ++i) {
      SB_CHECK(ticks[i] > 0);
      if (i > 0) {
        const int64_t gap = ticks[i] - ticks[i - 1];
        if (scramble) {
          SB_CHECK(gap <= static_cast<int64_t>(slowest / tuning::kScrambleRate) + 2);
        } else {
          SB_CHECK(gap >= static_cast<int64_t>(tuning::kLaunchBaseSeconds * kTicksPerSecond));
          SB_CHECK(gap <= static_cast<int64_t>(slowest) + 1);
        }
      }
    }
    std::printf("%s launches at %.1f %.1f %.1f %.1f s\n", scramble ? "scrambled" : "normal", ticks[0] / 20.0,
                ticks[1] / 20.0, ticks[2] / 20.0, ticks[3] / 20.0);
  }
}

void LaunchOnOrderWaitsForTheOrder()
{
  SnapshotBuilder builder;
  const UnitIndex carrier = builder.Add(CarrierShip(Side::Alliance));
  const UnitIndex squadron = builder.Add(XwingLike(Side::Alliance, carrier));
  builder.Add(Picket(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  const GroupId flight = battle.State().units[squadron].group;
  OrderSetFlightDoctrine doctrine;
  doctrine.doctrine.launch = LaunchRule::OnOrder;
  doctrine.fields = kFlightLaunch;
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, doctrine, {}, {flight})));
  SB_CHECK(battle.State().groups[flight].flightDoctrine.launch == LaunchRule::OnOrder); // applied at once
  battle.RunHeadless(30 * kTicksPerSecond);
  SB_CHECK(battle.State().Squadron(squadron)->place == SquadronPlace::Docked);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderLaunch{}, {carrier})));
  battle.RunHeadless(15 * kTicksPerSecond);
  SB_CHECK(battle.State().Squadron(squadron)->place == SquadronPlace::InSpace);
}

// Bombers strip a shieldless target with per-craft torpedoes, go home when they are spent, rearm and come back.
void TorpedoesAreAmmunitionAndRearm()
{
  SnapshotBuilder builder;
  CapitalShipEntry hulk = TransportShip(Side::Alliance, "Hulk");
  hulk.shieldMax = 0;
  hulk.hullMax = hulk.hull = 6000;
  hulk.sublight = 0;
  const UnitIndex target = builder.Add(hulk);
  const UnitIndex carrier = builder.Add(CarrierShip(Side::Empire));
  const UnitIndex bombers = builder.Add(BomberLike(Side::Empire, carrier));
  Battle battle(builder.Snapshot(), Uncommanded());
  Place(battle, target, {2000.0f, 0.0f}, 0.0f);
  Place(battle, carrier, {6000.0f, 0.0f}, kPi);
  const int full = Torpedoes(battle.State(), bombers);
  SB_CHECK(full == 12 * 8);
  bool spent = false;
  bool rearmed = false;
  float hullWhenSpent = 0.0f;
  while (battle.Tick() < 400 * kTicksPerSecond && !rearmed) {
    battle.Step();
    const BattleState& state = battle.State();
    const SquadronState& squadron = *state.Squadron(bombers);
    if (!spent && squadron.place == SquadronPlace::InSpace && Torpedoes(state, bombers) == 0) {
      spent = true;
      hullWhenSpent = state.Ship(target)->hull;
    }
    rearmed = spent && squadron.place == SquadronPlace::InSpace && Torpedoes(state, bombers) > 0;
  }
  SB_CHECK(spent && rearmed);
  // Most of the hull damage is the torpedoes: 96 torpedoes at most land 96 x the torpedo damage.
  const float dealt = 6000.0f - hullWhenSpent;
  std::printf("torpedo run: %.0f hull damage when the torpedoes ran out, rearmed by %.0f s\n", dealt,
              battle.State().BattleSeconds());
  SB_CHECK(dealt > 0.5f * static_cast<float>(full) * tuning::kTorpedoDamage);
  SB_CHECK(dealt < 1.3f * static_cast<float>(full) * tuning::kTorpedoDamage);
}

// Laser batteries engage fighters attacking other ships: a picket alongside protects a ship with no lasers (§6.4).
void PointDefenceCoversNeighbours()
{
  int survivors[2] = {0, 0};
  for (const bool escorted : {false, true}) {
    SnapshotBuilder builder;
    CapitalShipEntry hulk = TransportShip(Side::Alliance, "Hulk");
    hulk.hullMax = hulk.hull = 6000;
    hulk.sublight = 0;
    const UnitIndex target = builder.Add(hulk);
    UnitIndex picket = kNoUnit;
    if (escorted) {
      CapitalShipEntry screen = Picket(Side::Alliance, "Screen");
      screen.sublight = 0;
      picket = builder.Add(screen);
    }
    const UnitIndex carrier = builder.Add(CarrierShip(Side::Empire));
    const UnitIndex bombers = builder.Add(BomberLike(Side::Empire, carrier));
    Battle battle(builder.Snapshot(), Uncommanded());
    Place(battle, target, {0.0f, 0.0f}, 0.0f);
    if (picket != kNoUnit) {
      Place(battle, picket, {0.0f, 700.0f}, 0.0f);
    }
    Place(battle, carrier, {7000.0f, 0.0f}, kPi);
    bool torpedoesFired = false;
    while (!battle.Finished() && battle.Tick() < 90 * kTicksPerSecond) {
      battle.Step();
      torpedoesFired = torpedoesFired || std::ranges::any_of(battle.State().shots, [&](const PendingShot& shot) {
        return shot.shooter == bombers && shot.kind == ShotKind::Torpedo;
      });
    }
    SB_CHECK(torpedoesFired);
    survivors[escorted ? 1 : 0] = CraftAlive(battle.State(), bombers);
    if (escorted) {
      battle.RunHeadless(90 * kTicksPerSecond);
      const int after180 = CraftAlive(battle.State(), bombers);
      std::printf("escorted strike after 180 s: %d bombers left\n", after180);
      SB_CHECK(after180 > 0 && after180 < survivors[1]);
      // The picket's lasers fired at the bombers even though they never attacked it.
      SB_CHECK(battle.State().Ship(picket)->hull == battle.State().Profile(picket).hullMax);
    }
  }
  std::printf("bombers left after 90 s: %d unescorted, %d with a picket alongside\n", survivors[0], survivors[1]);
  SB_CHECK(survivors[1] >= 3 && survivors[1] + 3 <= survivors[0]);
}

// A destroyed carrier takes its docked squadrons with it; launched ones find another hangar or are stranded (§6.8).
void CarrierLossRules()
{
  for (const bool spareHangar : {true, false}) {
    SnapshotBuilder builder;
    const UnitIndex carrier = builder.Add(CarrierShip(Side::Alliance));
    const UnitIndex docked = builder.Add(XwingLike(Side::Alliance, carrier, "Docked"));
    const UnitIndex flying = builder.Add(BomberLike(Side::Alliance, carrier, "Flying"));
    UnitIndex spare = kNoUnit;
    if (spareHangar) {
      CapitalShipEntry wedge = Wedge(Side::Alliance, "Spare");
      spare = builder.Add(wedge);
    } else {
      builder.Add(Broadside(Side::Alliance, "No hangar"));
    }
    builder.Add(Picket(Side::Empire));
    Battle battle(builder.Snapshot(), Uncommanded());
    BattleState& state = Mutable(battle);
    sim::LaunchSquadron(state, flying);
    state.Ship(carrier)->shield = 0.0f;
    sim::DamageShip(state, carrier, 10000.0f, ShotKind::Turbolaser, Subsystem::Hull);
    SB_CHECK(state.units[docked].fate == UnitFate::Destroyed);
    SB_CHECK(state.units[flying].fate == UnitFate::Active);
    const BattleResultSet results = battle.Results();
    const auto flyingResult = std::find_if(results.squadrons.begin(), results.squadrons.end(),
                                           [&](const SquadronResult& result) { return result.unit == flying; });
    SB_CHECK(flyingResult->carrier == spare);
    SB_CHECK(flyingResult->size == 12);
  }
}

// TIE-like fighters cannot jump: at a withdrawal they dock with a departing ship, or stay behind without room.
void FightersWithoutHyperdriveNeedAHangar()
{
  for (const bool room : {true, false}) {
    SnapshotBuilder builder;
    builder.Params().heldFirstBy = Side::Alliance; // the Empire attacks, so its system-based squadrons arrive flying
    const UnitIndex carrier = builder.Add(CarrierShip(Side::Empire));
    std::vector<UnitIndex> aboard;
    for (int i = 0; i < (room ? 0 : 6); ++i) {
      SquadronEntry fill = TieLike(Side::Empire, carrier, "Aboard");
      fill.identity.typeBits = 0x582;
      aboard.push_back(builder.Add(fill));
    }
    const UnitIndex tie = builder.Add(TieLike(Side::Empire, kNoUnit, "Tie"));
    SquadronEntry defender = XwingLike(Side::Empire, kNoUnit, "Defender");
    defender.identity.typeBits = 0x584;
    const UnitIndex jumper = builder.Add(defender);
    CapitalShipEntry far = Picket(Side::Alliance, "Far");
    far.weapons = {};
    builder.Add(far);
    BattleOptions options = Uncommanded();
    options.tickLimit = 200 * kTicksPerSecond;
    Battle battle(builder.Snapshot(), options);
    for (UnitIndex squadron : aboard) {
      Mutable(battle).Squadron(squadron)->stayDocked = true; // the hangar stays full
    }
    SB_CHECK(battle.State().Squadron(tie)->place == SquadronPlace::InSpace);
    battle.RunHeadless(5 * kTicksPerSecond);
    Command withdraw = MakeCommand(Side::Empire, OrderWithdraw{});
    for (const GroupState& group : battle.State().groups) {
      if (group.side == Side::Empire) {
        withdraw.target.groups.push_back(group.id);
      }
    }
    SB_CHECK(battle.Submit(withdraw));
    battle.RunHeadless(options.tickLimit);
    const BattleState& state = battle.State();
    SB_CHECK(state.units[carrier].fate == UnitFate::Withdrawn);
    SB_CHECK(state.units[jumper].fate == UnitFate::Withdrawn);
    const BattleResultSet results = battle.Results();
    const auto tieResult = std::find_if(results.squadrons.begin(), results.squadrons.end(),
                                        [&](const SquadronResult& result) { return result.unit == tie; });
    if (room) {
      SB_CHECK(tieResult->fate == UnitFate::Withdrawn);
      SB_CHECK(tieResult->carrier == carrier);
    } else {
      SB_CHECK(tieResult->fate == UnitFate::Active); // stayed behind, still fighting
      for (UnitIndex squadron : aboard) {
        SB_CHECK(state.units[squadron].fate == UnitFate::Withdrawn);
      }
    }
  }
}

Command HoldInPlanetHangar(Side side, std::vector<UnitIndex> squadrons)
{
  OrderSetFlightDoctrine doctrine;
  doctrine.doctrine.launch = LaunchRule::OnOrder;
  doctrine.fields = kFlightLaunch;
  return MakeCommand(side, doctrine, std::move(squadrons));
}

// A planet's garrison has no departing hangar: one that can jump launches and spools from the planet, one that cannot
// stays to defend it rather than counting as present forever without fighting.
void GarrisonWithdrawsOnlyWithAHyperdrive()
{
  SnapshotBuilder builder;
  builder.Params().heldFirstBy = Side::Alliance; // the planet and its garrison are the Alliance's
  builder.Add(Broadside(Side::Alliance));
  const UnitIndex xwings = builder.Add(XwingLike(Side::Alliance, kNoUnit, "Garrison X"));
  const UnitIndex ties = builder.Add(TieLike(Side::Alliance, kNoUnit, "Garrison Tie"));
  builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  SB_CHECK(battle.Submit(HoldInPlanetHangar(Side::Alliance, {xwings, ties})));
  SB_CHECK(battle.State().Squadron(xwings)->place == SquadronPlace::Docked);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderWithdraw{}, {xwings, ties})));
  const SquadronState& jumper = *battle.State().Squadron(xwings);
  SB_CHECK(jumper.place == SquadronPlace::InSpace && jumper.withdrawing && jumper.spoolTotal > 0);
  const SquadronState& garrison = *battle.State().Squadron(ties);
  SB_CHECK(garrison.place == SquadronPlace::Docked && !garrison.withdrawing && !garrison.stayDocked);
  battle.RunHeadless(jumper.spoolTotal + kTicksPerSecond);
  SB_CHECK(battle.State().units[xwings].fate == UnitFate::Withdrawn);
  SB_CHECK(battle.State().units[ties].fate == UnitFate::Active);
}

// An attacker facing nothing but a garrison still docked at the planet goes to draw it out. A garrison that never comes
// out holds no space: when the battle stalls it concedes, and its squadrons are lost (§6.11).
void AttackerClosesOnADockedGarrison()
{
  SnapshotBuilder builder;
  builder.Params().heldFirstBy = Side::Alliance;
  const UnitIndex garrison = builder.Add(TieLike(Side::Alliance, kNoUnit, "Garrison"));
  const UnitIndex wedge = builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  SB_CHECK(battle.Submit(HoldInPlanetHangar(Side::Alliance, {garrison})));
  const BattleState& state = battle.State();
  const float start = Distance(state.units[wedge].position, state.planetCenter);
  battle.RunHeadless(90 * kTicksPerSecond);
  const float end = Distance(state.units[wedge].position, state.planetCenter);
  std::printf("garrison: attacker closed from %.0f m to %.0f m of the planet's centre\n", start, end);
  SB_CHECK(end < start - 10000.0f);
  SB_CHECK(end < tuning::kPlanetRadius + 2.0f * tuning::kGarrisonApproach);
  SB_CHECK(state.Squadron(garrison)->place == SquadronPlace::Docked);
  SB_CHECK(SidePresent(state, Side::Alliance) && !battle.Finished()); // waiting for its first launch, still in it
  SB_CHECK(battle.RunHeadless(battle.Options().tickLimit));
  SB_CHECK(state.endReason == EndReason::Elimination);
  SB_CHECK(state.units[garrison].fate == UnitFate::Destroyed);
  const BattleResultSet results = battle.Results();
  SB_CHECK(!results.outcome.alliancePresent && results.outcome.empirePresent);
}

// The planet's last squadron in space does not go home to rearm, since docking would concede the battle: it fights on
// with its torpedoes spent until the laser picket it cannot hurt has destroyed it.
void LastSquadronInSpaceFightsOnRatherThanDock()
{
  SnapshotBuilder builder;
  builder.Params().heldFirstBy = Side::Alliance;
  const UnitIndex bombers = builder.Add(BomberLike(Side::Alliance, kNoUnit, "Garrison bombers"));
  const UnitIndex picket = builder.Add(Picket(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  int64_t spentTick = -1;
  while (!battle.Finished() && battle.Tick() < battle.Options().tickLimit) {
    battle.Step();
    const SquadronState& squadron = *battle.State().Squadron(bombers);
    SB_CHECK(squadron.task != SquadronTask::Return);
    if (spentTick < 0 && squadron.launched && Torpedoes(battle.State(), bombers) == 0) {
      spentTick = battle.Tick();
    }
  }
  const BattleState& state = battle.State();
  std::printf("last squadron: torpedoes spent at %.0f s, destroyed by %.0f s\n", spentTick / 20.0, state.BattleSeconds());
  SB_CHECK(spentTick > 0);
  SB_CHECK(state.endReason == EndReason::Elimination);
  SB_CHECK(state.units[bombers].fate == UnitFate::Destroyed);
  SB_CHECK(state.units[picket].fate == UnitFate::Active && state.Ship(picket)->hull == state.Profile(picket).hullMax);
  const BattleResultSet results = battle.Results();
  SB_CHECK(!results.outcome.alliancePresent && results.outcome.empirePresent);
}

// A squadron may dock while its side still holds space, but it cannot come out again once that cover is gone: when the
// last ship falls the side is beaten, and the squadrons in its hangars are lost with it.
void DockedSquadronsAreLostWithTheirCover()
{
  SnapshotBuilder builder;
  builder.Params().heldFirstBy = Side::Alliance;
  const UnitIndex ship = builder.Add(Broadside(Side::Alliance));
  const UnitIndex garrison = builder.Add(XwingLike(Side::Alliance, kNoUnit, "Garrison"));
  builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  battle.RunHeadless(kTicksPerSecond);
  BattleState& state = Mutable(battle);
  SB_CHECK(state.Squadron(garrison)->place == SquadronPlace::InSpace && state.Squadron(garrison)->launched);
  SB_CHECK(CoveredWhenDocked(state, garrison));
  sim::DockSquadron(state, garrison, kNoUnit, true);
  sim::UpdateEnding(state);
  SB_CHECK(state.endReason == EndReason::None); // the ship still holds space
  sim::DestroyUnit(state, ship);
  SB_CHECK(!SidePresent(state, Side::Alliance));
  sim::UpdateEnding(state);
  SB_CHECK(state.endReason == EndReason::Elimination);
  SB_CHECK(state.units[garrison].fate == UnitFate::Destroyed);
  const BattleResultSet results = battle.Results();
  SB_CHECK(!results.outcome.alliancePresent && results.outcome.empirePresent);
  const auto result = std::find_if(results.squadrons.begin(), results.squadrons.end(),
                                   [&](const SquadronResult& squadron) { return squadron.unit == garrison; });
  SB_CHECK(result != results.squadrons.end() && result->fate == UnitFate::Destroyed && result->size == 0);
}

void InterceptorsDogfightBombers()
{
  SnapshotBuilder builder;
  builder.Params().heldFirstBy = Side::Empire; // the Alliance attacks, its system-based squadrons flying
  builder.Add(Broadside(Side::Alliance));
  const UnitIndex red = builder.Add(XwingLike(Side::Alliance, kNoUnit, "Red"));
  const UnitIndex wedge = builder.Add(Wedge(Side::Empire));
  const UnitIndex bombers = builder.Add(BomberLike(Side::Empire, wedge, "Bombers"));
  Battle battle(builder.Snapshot(), Uncommanded());
  OrderSetFlightDoctrine intercept;
  intercept.doctrine.mission = FlightMission::Intercept;
  intercept.doctrine.priorities = MakePriorities({TargetTag::Bomber, TargetTag::Fighter}, false);
  intercept.fields = kFlightMission | kFlightPriorities;
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, intercept, {red})));
  SB_CHECK(battle.State().units[red].overrides == (kFlightMission | kFlightPriorities));
  bool intercepted = false;
  while (battle.Tick() < 150 * kTicksPerSecond) {
    battle.Step();
    const SquadronState& squadron = *battle.State().Squadron(red);
    intercepted = intercepted || (squadron.task == SquadronTask::Intercept && squadron.target == bombers);
  }
  SB_CHECK(intercepted);
  const int lost = 12 - CraftAlive(battle.State(), bombers);
  std::printf("dogfight: bombers lost %d craft, interceptors lost %d\n", lost, 12 - CraftAlive(battle.State(), red));
  SB_CHECK(lost >= 4);
  // Clearing the override puts the squadron back on its flight's doctrine.
  OrderSetFlightDoctrine reset;
  reset.fields = 0;
  reset.clear = kFlightMission | kFlightPriorities;
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, reset, {red})));
  SB_CHECK(battle.State().units[red].overrides == 0);
}

void CommanderBoostsNearbySquadrons()
{
  SnapshotBuilder builder;
  builder.Params().heldFirstBy = Side::Empire;
  const UnitIndex flagship = builder.Add(Broadside(Side::Alliance));
  builder.AddCharacter(Side::Alliance, flagship, false, true, 40, 80);
  const UnitIndex squadron = builder.Add(XwingLike(Side::Alliance, kNoUnit));
  builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  BattleState& state = Mutable(battle);
  Place(battle, flagship, {0.0f, 0.0f}, 0.0f);
  state.Squadron(squadron)->centroid = {2000.0f, 0.0f};
  SB_CHECK_NEAR(CommanderBonus(state, squadron, 0.25f), 0.2f, 1e-4f);
  state.Squadron(squadron)->centroid = {tuning::kCommanderRadius + 100.0f, 0.0f};
  SB_CHECK(CommanderBonus(state, squadron, 0.25f) == 0.0f);
  SB_CHECK(state.Profile(flagship).staticTags & TagBit(TargetTag::Flagship));
}

} // namespace

// Bolts at a fighter are drawn to where it is now, not where it was when the shot was fired: a hit ends on the craft
// and a miss passes the miss offset beside it, however far the craft flew during the bolt's flight.
void BoltsAtFightersEndAtTheCraft()
{
  SnapshotBuilder builder;
  builder.Params().heldFirstBy = Side::Empire;
  builder.Add(Broadside(Side::Alliance));
  builder.Add(XwingLike(Side::Alliance, kNoUnit, "Red"));
  const UnitIndex wedge = builder.Add(Wedge(Side::Empire));
  builder.Add(TieLike(Side::Empire, wedge, "Ties"));
  Battle battle(builder.Snapshot(), Uncommanded());
  int checked = 0;
  float farthest = 0.0f;
  ViewModel view;
  while (battle.Tick() < 120 * kTicksPerSecond && checked < 200) {
    battle.Step();
    battle.BuildViewModel(1.0f, view);
    const BattleState& state = battle.State();
    for (const ViewShot& shot : view.shots) {
      if (shot.targetCraft < 0 || !state.craft[shot.targetCraft].alive) {
        continue;
      }
      const auto pending = std::ranges::find_if(state.shots, [&](const PendingShot& s) {
        return s.fireTick == shot.fireTick && s.targetCraft == shot.targetCraft && s.shooter == shot.shooter;
      });
      SB_CHECK(pending != state.shots.end());
      const Vec2 craft = state.craft[shot.targetCraft].position;
      const float off = Distance(shot.to, craft);
      SB_CHECK_NEAR(off, shot.hit ? 0.0f : tuning::kCraftMissOffset, 0.5f);
      farthest = std::max(farthest, Distance(pending->to, craft));
      ++checked;
    }
  }
  std::printf("bolts at fighters: %d checked, targets moved up to %.0f m from the aim point\n", checked, farthest);
  SB_CHECK(checked > 0);
}

int main()
{
  CarriersLaunchOnTheOriginalCadence();
  LaunchOnOrderWaitsForTheOrder();
  TorpedoesAreAmmunitionAndRearm();
  PointDefenceCoversNeighbours();
  CarrierLossRules();
  FightersWithoutHyperdriveNeedAHangar();
  GarrisonWithdrawsOnlyWithAHyperdrive();
  AttackerClosesOnADockedGarrison();
  LastSquadronInSpaceFightsOnRatherThanDock();
  DockedSquadronsAreLostWithTheirCover();
  InterceptorsDogfightBombers();
  CommanderBoostsNearbySquadrons();
  BoltsAtFightersEndAtTheCraft();
  std::puts("space battle fighters passed");
  return 0;
}
