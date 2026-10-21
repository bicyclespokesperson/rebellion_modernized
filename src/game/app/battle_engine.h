#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <span>
#include <vector>

#include "space_battle/results.h"
#include "space_battle/snapshot.h"
#include "app/battle_result_records.h"

namespace reb::battle { class Battle; struct BattleOptions; }

// The battle engine behind the strategic seam (docs/design/tactical_combat.md §11.1). BattleSetup builds a
// BattleSnapshot from the strategic objects and loads it; the battle flow steps the engine headless, one Advance per
// frame; BattleResults reads the outcome and per-unit records back by the handles Load returned.

class BattleEngine
{
  public:
  virtual ~BattleEngine() = default;

  // Registers every snapshot unit and returns one handle per unit (0: not registered). BattleSetup stores them in the
  // strategic objects with SetDataField04; BattleResults passes them back to the result accessors below.
  virtual std::vector<int> Load(const reb::battle::BattleSnapshot& snapshot) = 0;
  virtual void Start() = 0;
  // One frame of the battle; true once it is over.
  virtual bool Advance() = 0;
  virtual int64_t Steps() const = 0;

  virtual BattleOutcome Outcome() = 0;
  virtual CapitalShipResult CapitalShipResultFor(int handle) = 0;
  virtual FightSquadResult FightSquadResultFor(int handle) = 0;
  virtual DeathStarResult DeathStarResultFor(int handle) = 0;
  // The same results keyed by snapshot unit index, for the battle corpus and the runner.
  virtual reb::battle::BattleResultSet Results() = 0;
  // Whether a stalemate rule ended the battle rather than elimination, withdrawal or the tick limit.
  virtual bool EndedInStalemate() const = 0;

  // Ends the battle and releases the engine's state; no result can be read afterwards.
  virtual void Finish() = 0;
  virtual reb::battle::Battle* LiveBattle() { return nullptr; }
  virtual bool RestoreLiveBattle(const reb::battle::BattleSnapshot&, std::span<const uint8_t>,
                                const reb::battle::BattleOptions&) { return false; }
};

// REB_BATTLE_CORPUS: when set, every battle writes its snapshot and results there (battle_corpus.h).
std::optional<std::string> BattleCorpusDirectory();
