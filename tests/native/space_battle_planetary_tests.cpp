// Planetary batteries (tactical_combat.md §6.13): cover, targeting, damage kinds, the Under the guns approach, stand-off
// avoidance, the strength estimate, saves and determinism.
#include "space_battle_test_support.h"

#include "space_battle/battle_save.h"
#include "space_battle/tuning.h"

using namespace sbtest;

namespace
{

DefenseEntry Battery(Side side, WeaponKind weapon, int32_t attack, const std::string& name)
{
  DefenseEntry entry;
  entry.identity = Identity(side, name);
  entry.identity.family = weapon == WeaponKind::Ion ? 0x22 : 0x23;
  entry.identity.className = name;
  entry.weapon = weapon;
  entry.attackStrength = attack;
  return entry;
}

DefenseEntry LnrSeriesII(Side side = Side::Alliance)
{
  return Battery(side, WeaponKind::Turbolaser, 5000, "LNR Series II");
}

DefenseEntry Kdy150(Side side = Side::Alliance)
{
  return Battery(side, WeaponKind::Ion, 2000, "KDY-150");
}

// The Alliance holds the system (the planet is behind it) and owns `defenses`; the Empire attacks.
SnapshotBuilder Defended(std::vector<DefenseEntry> defenses, uint32_t seed = 7)
{
  SnapshotBuilder builder(seed);
  builder.Params().heldFirstBy = Side::Alliance;
  builder.Defenses() = std::move(defenses);
  return builder;
}

// A point `surface` meters out from the planet's surface, toward the battle.
Vec2 FromSurface(const BattleState& state, float surface)
{
  return state.planetCenter + state.arrivalDirection * (tuning::kPlanetRadius + surface);
}

int BatteryShots(const BattleState& state, UnitIndex target)
{
  int count = 0;
  for (const PendingShot& shot : state.shots) {
    count += shot.battery >= 0 && shot.target == target ? 1 : 0;
  }
  return count;
}

// Stops `unit` where it is and keeps it there (engines off for the test's span).
void Pin(Battle& battle, UnitIndex unit, Vec2 position)
{
  Place(battle, unit, position, 0.0f);
  Mutable(battle).Ship(unit)->engineOffTicks = 1000000;
}

bool Logged(const BattleState& state, const std::string& text)
{
  return std::any_of(state.events.begin(), state.events.end(),
                     [&](const BattleEvent& event) { return event.text.find(text) != std::string::npos; });
}

void ProfilesScaleWithTheRootOfAttackStrength()
{
  SnapshotBuilder builder = Defended({LnrSeriesII(), Battery(Side::Alliance, WeaponKind::Turbolaser, 800, "LNR I"),
                                      Kdy150()});
  builder.Add(Wedge(Side::Alliance));
  builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  const std::vector<DefenseProfile>& profiles = battle.State().defenseProfiles;
  SB_CHECK(profiles.size() == 3 && battle.State().defenses.size() == 3);
  SB_CHECK_NEAR(profiles[0].dps, tuning::kPlanetaryDpsPerRootAttack * std::sqrt(5000.0f), 1e-3f);
  SB_CHECK_NEAR(profiles[0].dps / profiles[1].dps, 2.5f, 1e-3f); // LNR II against LNR I
  SB_CHECK(profiles[2].weapon == WeaponKind::Ion);
  // The emplacements stand on the limb facing the battle, spread either side of its centre.
  for (const DefenseProfile& profile : profiles) {
    SB_CHECK(std::fabs(AngleDelta(AngleOf(battle.State().arrivalDirection), profile.siteAngle))
             <= tuning::kPlanetarySiteSpread + 1e-4f);
  }
  SB_CHECK(HasPlanetaryCover(battle.State(), Side::Alliance) && !HasPlanetaryCover(battle.State(), Side::Empire));
}

void BatteriesFireOnlyInsideTheirCover()
{
  SnapshotBuilder builder = Defended({LnrSeriesII()});
  builder.Add(Wedge(Side::Alliance));
  const UnitIndex enemy = builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  const float radius = battle.State().Profile(enemy).radius;
  Pin(battle, enemy, FromSurface(battle.State(), tuning::kPlanetaryCoverReach + radius + 500.0f));
  battle.RunHeadless(10 * kTicksPerSecond);
  SB_CHECK(battle.State().defenses[0].lastFireTick < 0);
  SB_CHECK(BatteryShots(battle.State(), enemy) == 0);
  Pin(battle, enemy, FromSurface(battle.State(), tuning::kPlanetaryCoverReach - 1000.0f));
  const float shields = battle.State().Ship(enemy)->shield;
  battle.RunHeadless(4 * kTicksPerSecond);
  const DefenseState& battery = battle.State().defenses[0];
  SB_CHECK(battery.lastFireTick >= 0 && battery.target == enemy);
  SB_CHECK(Logged(battle.State(), "Planetary defenses open fire"));
  bool fromThePlanet = false;
  for (const PendingShot& shot : battle.State().shots) {
    if (shot.battery == 0) {
      fromThePlanet = shot.shooter == kNoUnit && shot.side == Side::Alliance && shot.kind == ShotKind::Turbolaser
                      && std::fabs(Distance(shot.from, battle.State().planetCenter) - tuning::kPlanetRadius) < 1.0f;
    }
  }
  SB_CHECK(fromThePlanet);
  battle.RunHeadless(20 * kTicksPerSecond);
  SB_CHECK(battle.State().Ship(enemy)->shield < shields || battle.State().Ship(enemy)->hull < 2600.0f);
}

void BatteriesIgnoreFightersAndTheirOwnSide()
{
  SnapshotBuilder builder = Defended({LnrSeriesII(), Kdy150()});
  const UnitIndex own = builder.Add(Wedge(Side::Alliance));
  const UnitIndex fighters = builder.Add(XwingLike(Side::Empire, kNoUnit, "Raiders"));
  builder.Add(Wedge(Side::Empire)); // far away, outside the cover
  Battle battle(builder.Snapshot(), Uncommanded());
  Pin(battle, own, FromSurface(battle.State(), 2000.0f));
  battle.RunHeadless(2);
  SquadronState& raiders = *Mutable(battle).Squadron(fighters);
  for (int i = 0; i < raiders.craftCount; ++i) {
    Mutable(battle).craft[raiders.firstCraft + i].position = FromSurface(battle.State(), 1500.0f);
  }
  battle.RunHeadless(10 * kTicksPerSecond);
  for (const DefenseState& battery : battle.State().defenses) {
    SB_CHECK(battery.lastFireTick < 0);
  }
}

// The batteries shoot the intruder nearest the planet, or the side's Concentrate Fire target when it is in reach.
void BatteriesTakeTheNearestIntruderOrTheSignalTarget()
{
  SnapshotBuilder builder = Defended({LnrSeriesII()});
  const UnitIndex flagship = builder.Add(Wedge(Side::Alliance));
  builder.AddCharacter(Side::Alliance, flagship, true, false, 90, 10);
  const UnitIndex near = builder.Add(Wedge(Side::Empire, "Near"));
  const UnitIndex far = builder.Add(Wedge(Side::Empire, "Far"));
  Battle battle(builder.Snapshot(), Uncommanded());
  Pin(battle, near, FromSurface(battle.State(), 3000.0f));
  Pin(battle, far, FromSurface(battle.State(), 9000.0f) + Vec2{0.0f, 4000.0f});
  battle.RunHeadless(4 * kTicksPerSecond);
  SB_CHECK(battle.State().defenses[0].target == near);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderSignal{SignalKind::ConcentrateFire, far})));
  battle.RunHeadless(4 * kTicksPerSecond);
  SB_CHECK(battle.State().defenses[0].target == far);
}

// A KDY-150 hits shields only; once they are down its disruption knocks out the engines, and then it moves on to an
// intruder it has not stopped yet.
void IonCannonStripsShieldsThenTakesTheEngines()
{
  SnapshotBuilder builder = Defended({Kdy150()});
  builder.Add(TransportShip(Side::Alliance)); // unarmed: only the planet shoots
  const UnitIndex first = builder.Add(Wedge(Side::Empire, "First"));
  const UnitIndex second = builder.Add(Wedge(Side::Empire, "Second"));
  Battle battle(builder.Snapshot(), Uncommanded());
  Place(battle, first, FromSurface(battle.State(), 1000.0f), 0.0f);
  Pin(battle, second, FromSurface(battle.State(), 6000.0f) + Vec2{0.0f, 3000.0f});
  BattleState& state = Mutable(battle);
  const float hull = state.Ship(first)->hull;
  state.Ship(first)->shield = 0.0f;
  int64_t limit = state.tick + 120 * kTicksPerSecond;
  while (state.Ship(first)->engineOffTicks == 0 && state.tick < limit) {
    state.Ship(first)->shield = 0.0f; // keep the shields down so the ion fire overflows into disruption
    battle.Step();
  }
  SB_CHECK(state.Ship(first)->engineOffTicks > 0);
  SB_CHECK(state.Ship(first)->hull == hull); // ion fire never touches the hull
  SB_CHECK(Logged(state, "engines knocked out by planetary ion fire"));
  limit = state.tick + 8 * kTicksPerSecond;
  while (state.defenses[0].target != second && state.tick < limit) {
    state.Ship(first)->shield = 0.0f;
    battle.Step();
  }
  SB_CHECK(state.defenses[0].target == second);
}

// Under the guns keeps a defender's line inside its cover while the enemy stays out of it; Direct does not.
void UnderTheGunsHoldsTheLineInCover()
{
  for (const bool batteries : {true, false}) {
    SnapshotBuilder builder = Defended(batteries ? std::vector<DefenseEntry>{LnrSeriesII()} : std::vector<DefenseEntry>{});
    const UnitIndex line = builder.Add(Wedge(Side::Alliance));
    builder.Add(Wedge(Side::Alliance, "Second"));
    const UnitIndex enemy = builder.Add(Wedge(Side::Empire));
    Battle battle(builder.Snapshot(), Uncommanded());
    const GroupState& group = *battle.State().Group(battle.State().units[line].group);
    SB_CHECK((group.shipDoctrine.approach == Approach::UnderGuns) == batteries);
    Pin(battle, enemy, FromSurface(battle.State(), tuning::kPlanetaryCoverReach + 12000.0f));
    battle.RunHeadless(120 * kTicksPerSecond);
    const float surface = Distance(battle.State().units[line].position, battle.State().planetCenter)
                          - tuning::kPlanetRadius;
    std::printf("under the guns (%s): line %.0f m from the surface, cover reaches %.0f m\n",
                batteries ? "batteries" : "no batteries", surface, tuning::kPlanetaryCoverReach);
    if (batteries) {
      SB_CHECK(surface <= tuning::kPlanetaryCoverReach);
      SB_CHECK(battle.State().units[line].intent.kind == IntentKind::HoldingCover);
    } else {
      SB_CHECK(surface > tuning::kPlanetaryCoverReach);
    }
  }
}

// An explicit attack order leaves the cover to reach its target.
void AttackOrdersLeaveTheCover()
{
  SnapshotBuilder builder = Defended({LnrSeriesII()});
  const UnitIndex line = builder.Add(Wedge(Side::Alliance));
  const UnitIndex enemy = builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  Pin(battle, enemy, FromSurface(battle.State(), tuning::kPlanetaryCoverReach + 12000.0f));
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderAttack{{enemy}}, {line})));
  battle.RunHeadless(120 * kTicksPerSecond);
  const float surface = Distance(battle.State().units[line].position, battle.State().planetCenter) - tuning::kPlanetRadius;
  SB_CHECK(surface > tuning::kPlanetaryCoverReach);
}

// A stand-off attacker keeps out of the enemy batteries' reach as it does out of their ships'.
void StandOffKeepsOutOfTheCover()
{
  SnapshotBuilder builder = Defended({LnrSeriesII()});
  const UnitIndex defender = builder.Add(Wedge(Side::Alliance));
  const UnitIndex attacker = builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  Pin(battle, defender, FromSurface(battle.State(), 3000.0f));
  OrderSetShipDoctrine standOff;
  standOff.doctrine.range = EngagementRange::StandOff;
  standOff.fields = kShipRange;
  SB_CHECK(battle.Submit(MakeCommand(Side::Empire, standOff, {attacker})));
  battle.RunHeadless(150 * kTicksPerSecond);
  const BattleState& state = battle.State();
  SB_CHECK(!InPlanetaryCover(state, Side::Alliance, state.units[attacker].position, state.Profile(attacker).radius));
  SB_CHECK(state.defenses[0].lastFireTick < 0);
}

// Batteries add firepower to their side's strength estimate but no durability: they cannot hold the system alone.
void StrengthCountsBatteryFirepower()
{
  SnapshotBuilder with = Defended({LnrSeriesII(), Kdy150()});
  SnapshotBuilder without = Defended({});
  for (SnapshotBuilder* builder : {&with, &without}) {
    builder->Add(Wedge(Side::Alliance));
    builder->Add(Wedge(Side::Empire));
  }
  Battle a(with.Snapshot(), Uncommanded());
  Battle b(without.Snapshot(), Uncommanded());
  SB_CHECK(SideStrength(a.State(), Side::Alliance) > SideStrength(b.State(), Side::Alliance));
  SB_CHECK(SideStrength(a.State(), Side::Empire) == SideStrength(b.State(), Side::Empire));
  const float expected = a.State().defenseProfiles[0].dps + 0.5f * a.State().defenseProfiles[1].dps;
  SB_CHECK_NEAR(PlanetaryFirepower(a.State(), Side::Alliance), expected, 1e-3f);
}

// The battle AI fights a defensive battle under its batteries, and sallies out once it is clearly the stronger.
void DefenderAiFightsUnderTheGunsUnlessMuchStronger()
{
  for (const bool outnumbered : {true, false}) {
    SnapshotBuilder builder = Defended({LnrSeriesII()});
    const UnitIndex line = builder.Add(Wedge(Side::Alliance));
    for (int i = 0; i < (outnumbered ? 2 : 0); ++i) {
      builder.Add(Wedge(Side::Empire, "Attacker"));
    }
    if (!outnumbered) {
      builder.Add(Picket(Side::Empire, "Lone Picket"));
    }
    Battle battle(builder.Snapshot(), Uncommanded());
    auto commander = MakeBattleCommander();
    std::vector<Command> commands;
    commander->Update(battle, Side::Alliance, commands);
    for (const Command& command : commands) {
      battle.Submit(command);
    }
    const Approach approach = battle.State().EffectiveShipDoctrine(line).approach;
    SB_CHECK((approach == Approach::UnderGuns) == outnumbered);
  }
}

// A battle with batteries saves and restores exactly, and replays from its command log.
void SavesAndReplaysWithBatteries()
{
  SnapshotBuilder builder = Defended({LnrSeriesII(), Kdy150()}, 23);
  builder.Add(Wedge(Side::Alliance));
  builder.Add(Broadside(Side::Alliance));
  builder.Add(Wedge(Side::Empire));
  builder.Add(Wedge(Side::Empire, "Second"));
  builder.Add(Broadside(Side::Empire));
  const BattleSnapshot snapshot = builder.Snapshot();
  Battle reference(snapshot, BattleOptions{});
  bool fired = false;
  while (!fired && reference.Tick() < 240 * kTicksPerSecond) {
    reference.Step();
    fired = std::any_of(reference.State().shots.begin(), reference.State().shots.end(),
                        [](const PendingShot& shot) { return shot.battery >= 0; });
  }
  SB_CHECK(fired);
  const std::vector<uint8_t> saved = reference.SaveState();
  std::unique_ptr<Battle> restored = Battle::RestoreState(snapshot, BattleOptions{}, saved);
  SB_CHECK(restored != nullptr && restored->StateHash() == reference.StateHash());
  reference.RunHeadless(reference.Options().tickLimit);
  restored->RunHeadless(reference.Options().tickLimit);
  SB_CHECK(reference.Finished() && restored->StateHash() == reference.StateHash());

  BattleOptions replayOptions;
  replayOptions.aiControlled = {false, false};
  Battle replay(snapshot, replayOptions);
  for (const Command& command : reference.CommandLog()) {
    SB_CHECK(replay.Submit(command));
  }
  replay.RunHeadless(reference.Options().tickLimit);
  std::vector<uint8_t> replayed;
  std::vector<uint8_t> original;
  WriteState(replay.State(), replayed);
  WriteState(reference.State(), original);
  SB_CHECK(replayed == original); // the commanders' private state aside, which the replay has none of
  std::printf("planetary battle: %.0f s, saved and replayed exactly\n", reference.State().BattleSeconds());
}

// A version-1 state block (before planetary batteries) still restores.
void VersionOneStateBlocksRestore()
{
  SnapshotBuilder builder = Defended({}, 31);
  builder.Add(Wedge(Side::Alliance));
  builder.Add(Wedge(Side::Empire));
  const BattleSnapshot snapshot = builder.Snapshot();
  Battle reference(snapshot, BattleOptions{});
  reference.RunHeadless(60 * kTicksPerSecond);
  SB_CHECK(!reference.State().shots.empty());
  // A full save is the state block and then the commanders' blobs: swap in a version-1 state block.
  const std::vector<uint8_t> full = reference.SaveState();
  std::vector<uint8_t> current;
  WriteState(reference.State(), current);
  std::vector<uint8_t> versionOne;
  WriteState(reference.State(), versionOne, 1);
  SB_CHECK(versionOne.size() < current.size());
  versionOne.insert(versionOne.end(), full.begin() + static_cast<std::ptrdiff_t>(current.size()), full.end());
  std::unique_ptr<Battle> restored = Battle::RestoreState(snapshot, BattleOptions{}, versionOne);
  SB_CHECK(restored != nullptr);
  SB_CHECK(restored->StateHash() == reference.StateHash());
}

// The view model carries the batteries, their cover and the ships inside it.
void ViewModelShowsBatteriesAndCover()
{
  SnapshotBuilder builder = Defended({LnrSeriesII(), Kdy150()});
  builder.Add(Wedge(Side::Alliance));
  const UnitIndex enemy = builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  Pin(battle, enemy, FromSurface(battle.State(), 4000.0f));
  battle.RunHeadless(5 * kTicksPerSecond);
  ViewModel view;
  battle.BuildViewModel(1.0f, view);
  SB_CHECK(view.defenses.size() == 2);
  SB_CHECK_NEAR(view.coverRadius, tuning::kPlanetRadius + tuning::kPlanetaryCoverReach, 1e-3f);
  SB_CHECK(view.defenses[0].name == "LNR Series II" && view.defenses[1].weapon == WeaponKind::Ion);
  SB_CHECK(view.defenses[0].sinceFired >= 0.0f && view.defenses[0].target == enemy);
  const auto unit = std::find_if(view.units.begin(), view.units.end(), [&](const ViewUnit& v) { return v.unit == enemy; });
  SB_CHECK((unit->flags & kViewUnderPlanetaryGuns) != 0);
  SB_CHECK(std::any_of(view.shots.begin(), view.shots.end(), [](const ViewShot& shot) { return shot.battery >= 0; }));
}

} // namespace

int main()
{
  ProfilesScaleWithTheRootOfAttackStrength();
  BatteriesFireOnlyInsideTheirCover();
  BatteriesIgnoreFightersAndTheirOwnSide();
  BatteriesTakeTheNearestIntruderOrTheSignalTarget();
  IonCannonStripsShieldsThenTakesTheEngines();
  UnderTheGunsHoldsTheLineInCover();
  AttackOrdersLeaveTheCover();
  StandOffKeepsOutOfTheCover();
  StrengthCountsBatteryFirepower();
  DefenderAiFightsUnderTheGunsUnlessMuchStronger();
  SavesAndReplaysWithBatteries();
  VersionOneStateBlocksRestore();
  ViewModelShowsBatteriesAndCover();
  std::puts("space battle planetary batteries passed");
  return 0;
}
