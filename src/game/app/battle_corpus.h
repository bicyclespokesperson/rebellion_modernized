#pragma once
#include <string>

#include "app/battle_engine.h"

// The calibration corpus (docs/design/tactical_combat.md §11.7). With REB_BATTLE_CORPUS set, every battle writes
// <dir>/<campaign seed>-<n>.snapshot when it starts and <dir>/<campaign seed>-<n>.space.result once it has
// resolved, n counting the battles of this process from 1. Recording only reads the battle; the campaign is unchanged.
void RecordBattleSnapshot(const reb::battle::BattleSnapshot& snapshot);
void RecordBattleResults(BattleEngine& engine);

// A finished battle's results as text: the engine-independent results, the strategic outcome flags BattleResults reads,
// and how the battle ended. The battle runner writes the same form.
std::string FormatBattleResults(BattleEngine& engine);
