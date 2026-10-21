#pragma once

#include "app/battle_engine.h"
#include "space_battle/battle.h"

// Pure translations into the strategic hand-back records. Handles are snapshot indices plus one.
BattleOutcome SpaceBattleOutcome(const reb::battle::BattleOutcomeSummary& summary);
CapitalShipResult SpaceCapitalShipResult(const reb::battle::BattleSnapshot& snapshot,
                                        const reb::battle::BattleResultSet& results, int handle);
FightSquadResult SpaceFightSquadResult(const reb::battle::BattleSnapshot& snapshot,
                                     const reb::battle::BattleResultSet& results, int handle);
DeathStarResult SpaceDeathStarResult(const reb::battle::BattleResultSet& results, int handle);

std::unique_ptr<BattleEngine> CreateSpaceBattleEngine();

// Persisted strategic controller kinds: human 1, Classic 3, Codex 4, Claude 5.
struct StrategicBattleContext {
  std::array<int, 2> controllerKinds{3, 3};
  int ruleDifficultyColumn = 7;
};

// Both sides use the battle AI for headless resolution, including a human's Simulate Results.
// Classic is Aggressive, Codex Balanced, Claude Cautious; human/remote/unknown use Balanced.
// Rule columns 0-7 select asymmetric campaign rule tables, not an established tactical skill scale;
// keep Normal difficulty for every column until a player-facing tactical difficulty is defined.
// Live battles reuse this translation, then disable aiControlled for the side taking command.
reb::battle::BattleOptions BattleOptionsFor(const reb::battle::BattleSnapshot& snapshot,
                                          const StrategicBattleContext& context);
// Reads actual saved session controllers; without a session uses configured AI preferences.
StrategicBattleContext CurrentStrategicBattleContext();
