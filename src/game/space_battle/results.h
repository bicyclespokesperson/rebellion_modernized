#pragma once
#include <cstdint>
#include <vector>

#include "space_battle/snapshot.h"

// What a battle hands back to the strategic game, keyed by snapshot unit index. Engine-independent; the app layer
// (app/battle_engine.h) converts it to the records BattleResults::Apply consumes.
namespace reb::battle
{

enum class UnitFate : uint8_t { Active, Withdrawn, Destroyed, Docked };

struct ShipResult {
  UnitIndex unit = kNoUnit;
  UnitFate fate = UnitFate::Active;
  int32_t hull = 0; // 0 when destroyed
  CriticalHits criticals;
};

struct SquadronResult {
  UnitIndex unit = kNoUnit;
  UnitFate fate = UnitFate::Active;
  int32_t size = 0;            // craft left; 0 when destroyed
  UnitIndex carrier = kNoUnit; // where it ended up docked or assigned; kNoUnit = the system
};

struct DeathStarResult {
  UnitIndex unit = kNoUnit;
  bool alive = true;
  bool withdrew = false;
  int32_t hull = 0;
};

// Encodes into the strategic BattleOutcome flags (app/battle_result_records.h): a stalemate or time limit reports both sides
// present, which BattleResults resolves in favour of the side that held the system first (tactical_combat.md §6.11).
struct BattleOutcomeSummary {
  bool alliancePresent = false; // has active units when the battle ends
  bool empirePresent = false;
  bool allianceWithdrew = false; // the side withdrew and at least one unit got away
  bool empireWithdrew = false;
};

struct BattleResultSet {
  BattleOutcomeSummary outcome;
  std::vector<ShipResult> ships;
  std::vector<SquadronResult> squadrons;
  std::vector<DeathStarResult> deathStars;
};

} // namespace reb::battle
