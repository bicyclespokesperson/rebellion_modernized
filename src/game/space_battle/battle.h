#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

#include "space_battle/commander.h"
#include "space_battle/commands.h"
#include "space_battle/results.h"
#include "space_battle/snapshot.h"
#include "space_battle/view_model.h"

// The space battle simulator (tactical_combat.md §6): one battle on a 2D plane, stepped at a fixed 20 Hz. The same
// object runs live under the player's command and headless for Simulate Results and AI battles. It is deterministic
// for a given snapshot, seed and command log on one platform, depends on nothing outside src/game/space_battle, and
// has no SDL or strategic-game types.
namespace reb::battle
{

inline constexpr int kTicksPerSecond = 20;
inline constexpr float kTickSeconds = 1.0f / kTicksPerSecond;
// One range point of the records' weapon and tractor ranges, on the battle plane (§6.1). The design's 100 m is scaled
// up so the original's compressed ship lengths leave room between ships (tuning.h, kWorldScale).
inline constexpr float kMetersPerRangePoint = 150.0f;
// The battle plane spans [-kBattlePlaneHalfExtent, kBattlePlaneHalfExtent] on both axes.
inline constexpr float kBattlePlaneHalfExtent = 33000.0f;

struct BattleOptions {
  // Sides commanded by the built-in AI; the others only act on submitted commands and their doctrine.
  std::array<bool, 2> aiControlled{true, true};
  // Fleet-withdrawal strength ratio for a human side commanded by the AI (§8.2): a human's vote carries 0.
  float humanSideAiWithdrawThreshold = 0.33f;
  int64_t tickLimit = 15 * 60 * kTicksPerSecond; // §6.11
  // Direction (radians) from the battle centre toward where the attacker arrives from: the bearing of the system it
  // came from on the galaxy map (§4).
  float attackerBearing = 0.0f;
  // The strategic seam maps controller kind to temperament; difficulty changes decisions, never ship statistics.
  std::array<CommanderParams, 2> commanders{};
  // Builds the commander of an AI-controlled side; empty means DefaultCommander. Restore uses it too, then restores
  // each commander's saved state.
  std::function<std::unique_ptr<Commander>(Side)> makeCommander;
};

struct BattleState; // the simulation's internals (battle_state.h)

class Battle
{
  public:
  Battle(const BattleSnapshot& snapshot, const BattleOptions& options);
  ~Battle();
  Battle(const Battle&) = delete;
  Battle& operator=(const Battle&) = delete;

  // Queues a command for the next tick; returns false (and ignores it) when it is invalid for that side.
  bool Submit(const Command& command);
  // Advances one tick. Does nothing once finished.
  void Step();
  // Runs until finished or `maxTicks` more ticks have passed; returns Finished().
  bool RunHeadless(int64_t maxTicks);

  // Live-only, unsaved delay after decision. Restoring a decided battle finishes on its next step.
  void SetOutroSeconds(float seconds);
  void SkipOutro();
  bool Finished() const;
  int64_t Tick() const;
  const BattleSnapshot& Snapshot() const;
  const BattleOptions& Options() const;
  void SetAiControlled(Side side, bool ai); // Observe Battle, Get Immediate Result

  BattleResultSet Results() const;

  // `alpha` in [0, 1) interpolates between the previous and the current tick.
  void BuildViewModel(float alpha, ViewModel& out) const;
  ViewUnitDetail Detail(UnitIndex unit) const;

  // Every command accepted so far, in order: with the snapshot, enough to re-simulate the battle (§11.4).
  std::span<const Command> CommandLog() const;
  // Full state (versioned, binary), for mid-battle saves; Restore needs the same snapshot.
  std::vector<uint8_t> SaveState() const;
  static std::unique_ptr<Battle> RestoreState(const BattleSnapshot& snapshot, const BattleOptions& options,
                                              std::span<const uint8_t> state);
  // Hash of the full state, for golden replay tests.
  uint64_t StateHash() const;

  const BattleState& State() const;

  private:
  explicit Battle(std::unique_ptr<BattleState> state);
  void RunCommanders();
  void ApplyDueCommands();
  std::unique_ptr<BattleState> m_state;
  std::array<std::unique_ptr<Commander>, 2> m_commanders;
};

} // namespace reb::battle
