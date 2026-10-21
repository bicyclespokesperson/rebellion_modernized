#include "app/battle_engine_space.h"

#include <algorithm>
#include <numeric>

#include "space_battle/battle.h"

namespace
{

namespace battle = reb::battle;

int StrategicUnitState(battle::UnitFate fate)
{
  switch (fate) {
    case battle::UnitFate::Active:
      return 2;
    case battle::UnitFate::Withdrawn:
      return 9;
    case battle::UnitFate::Destroyed:
      return 0xb;
    case battle::UnitFate::Docked:
      return 1;
  }
  return 2;
}

void SetIdentity(CapitalShipResult& result, const battle::UnitIdentity& identity, int handle)
{
  result.m_unk00 = handle;
  result.m_unk08 = static_cast<int>(identity.family);
  result.m_unk0c = static_cast<int>(identity.id);
}

bool CarrierAvailable(const battle::BattleResultSet& results, const battle::SquadronResult& squadron)
{
  const bool withdrew = squadron.fate == battle::UnitFate::Withdrawn;
  return std::ranges::any_of(results.ships, [&](const auto& ship) {
           return ship.unit == squadron.carrier && ship.fate != battle::UnitFate::Destroyed
                  && (ship.fate == battle::UnitFate::Withdrawn) == withdrew;
         })
         || std::ranges::any_of(results.deathStars, [&](const auto& station) {
              return station.unit == squadron.carrier && station.alive && station.withdrew == withdrew;
            });
}

class SpaceBattleEngine final : public BattleEngine
{
  public:
  std::vector<int> Load(const battle::BattleSnapshot& snapshot) override
  {
    m_battle = std::make_unique<battle::Battle>(snapshot, BattleOptionsFor(snapshot, CurrentStrategicBattleContext()));
    m_results = {};
    std::vector<int> handles(snapshot.units.size());
    std::iota(handles.begin(), handles.end(), 1);
    return handles;
  }

  battle::Battle* LiveBattle() override
  {
    return m_battle.get();
  }

  bool RestoreLiveBattle(const battle::BattleSnapshot& snapshot, std::span<const uint8_t> state,
                        const battle::BattleOptions& options) override
  {
    m_battle = battle::Battle::RestoreState(snapshot, options, state);
    m_results = {};
    return m_battle != nullptr;
  }

  void Start() override
  {
  }

  bool Advance() override
  {
    const bool finished = m_battle->RunHeadless(battle::kTicksPerSecond);
    if (finished) {
      m_results = m_battle->Results();
    }
    return finished;
  }

  int64_t Steps() const override
  {
    return m_battle->Tick();
  }

  BattleOutcome Outcome() override
  {
    m_results = m_battle->Results();
    return SpaceBattleOutcome(m_results.outcome);
  }

  CapitalShipResult CapitalShipResultFor(int handle) override
  {
    return SpaceCapitalShipResult(m_battle->Snapshot(), m_results, handle);
  }

  FightSquadResult FightSquadResultFor(int handle) override
  {
    return SpaceFightSquadResult(m_battle->Snapshot(), m_results, handle);
  }

  DeathStarResult DeathStarResultFor(int handle) override
  {
    return SpaceDeathStarResult(m_results, handle);
  }

  battle::BattleResultSet Results() override
  {
    m_results = m_battle->Results();
    return m_results;
  }

  bool EndedInStalemate() const override
  {
    const auto outcome = m_battle->Results().outcome;
    return outcome.alliancePresent && outcome.empirePresent && m_battle->Tick() < m_battle->Options().tickLimit;
  }

  void Finish() override
  {
    m_battle.reset();
    m_results = {};
  }

  private:
  std::unique_ptr<battle::Battle> m_battle;
  battle::BattleResultSet m_results;
};

} // namespace

BattleOutcome SpaceBattleOutcome(const reb::battle::BattleOutcomeSummary& summary)
{
  BattleOutcome outcome{};
  outcome.m_sideAAlive = summary.alliancePresent;
  outcome.m_sideBAlive = summary.empirePresent;
  outcome.m_sideAFinal = summary.allianceWithdrew;
  outcome.m_sideBFinal = summary.empireWithdrew;
  return outcome;
}

CapitalShipResult SpaceCapitalShipResult(const reb::battle::BattleSnapshot& snapshot,
                                        const reb::battle::BattleResultSet& results, int handle)
{
  CapitalShipResult result{};
  const auto ship = std::ranges::find(results.ships, handle - 1, &battle::ShipResult::unit);
  if (ship != results.ships.end()) {
    const auto& entry = std::get<battle::CapitalShipEntry>(snapshot.units[ship->unit]);
    SetIdentity(result, entry.identity, handle);
    result.m_hull = static_cast<float>(ship->hull);
    result.m_speedCH = ship->criticals.speed;
    result.m_shieldRechargeCH = ship->criticals.shieldRecharge;
    result.m_weaponRechargeCH = ship->criticals.weaponRecharge;
    result.m_tractorBeamCH = ship->criticals.tractor;
    // ApplyCapitalShip splits this count over the installed drives, primary first.
    result.m_hyperdriveDamage = ship->criticals.hyperdrivePrimary + ship->criticals.hyperdriveBackup;
    result.m_unk28 = StrategicUnitState(ship->fate);
  } else if (!results.deathStars.empty()) {
    // A Death Star can also carry squadrons.
    const auto& station = results.deathStars.front();
    const auto& entry = std::get<battle::DeathStarEntry>(snapshot.units[station.unit]);
    SetIdentity(result, entry.identity, station.unit + 1);
    result.m_hull = static_cast<float>(station.hull);
    result.m_unk28 = StrategicUnitState(!station.alive ? battle::UnitFate::Destroyed
                                             : station.withdrew ? battle::UnitFate::Withdrawn : battle::UnitFate::Active);
  }
  return result;
}

FightSquadResult SpaceFightSquadResult(const reb::battle::BattleSnapshot& snapshot,
                                     const reb::battle::BattleResultSet& results, int handle)
{
  FightSquadResult result{};
  const auto squadron = std::ranges::find(results.squadrons, handle - 1, &battle::SquadronResult::unit);
  if (squadron != results.squadrons.end()) {
    const auto& entry = std::get<battle::SquadronEntry>(snapshot.units[squadron->unit]);
    result.m_unk00 = handle;
    result.m_unk08 = static_cast<int>(entry.identity.family);
    result.m_unk0c = static_cast<int>(entry.identity.id);
    result.m_squadSize = squadron->size;
    result.m_unk1c = StrategicUnitState(squadron->fate);
    if (squadron->carrier != battle::kNoUnit && squadron->fate != battle::UnitFate::Destroyed
        && CarrierAvailable(results, *squadron)) {
      result.m_unk10 = squadron->carrier + 1;
      result.m_unk14 = 1;
    }
  }
  return result;
}

DeathStarResult SpaceDeathStarResult(const reb::battle::BattleResultSet& results, int handle)
{
  DeathStarResult result{};
  const auto station = std::ranges::find(results.deathStars, handle - 1, &battle::DeathStarResult::unit);
  if (station != results.deathStars.end()) {
    result.m_alive = station->alive;
    result.m_hull = static_cast<float>(station->hull);
    result.m_unk10 = station->withdrew;
  }
  return result;
}

std::unique_ptr<BattleEngine> CreateSpaceBattleEngine()
{
  return std::make_unique<SpaceBattleEngine>();
}

reb::battle::BattleOptions BattleOptionsFor(const reb::battle::BattleSnapshot& snapshot,
                                          const StrategicBattleContext& context)
{
  reb::battle::BattleOptions options;
  for (size_t side = 0; side < options.commanders.size(); ++side) {
    const int kind = snapshot.params.sides[side].human ? 1 : context.controllerKinds[side];
    options.commanders[side].temperament = kind == 3 ? reb::battle::Temperament::Aggressive
                                               : kind == 5 ? reb::battle::Temperament::Cautious
                                                           : reb::battle::Temperament::Balanced;
  }
  return options;
}
