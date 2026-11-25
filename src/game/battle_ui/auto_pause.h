#pragma once
#include <functional>
#include <string>
#include <vector>

#include "battle_ui/hud_settings.h"
#include "space_battle/view_model.h"

// Watches the view model for the auto-pause events of tactical_combat.md §5.7 and for command-phase boundaries. It
// compares each frame with the one before, so it needs nothing from the simulator beyond the view model.
namespace reb::battle_ui
{

struct AutoPauseHit {
  AutoPauseEvent event = AutoPauseEvent::GravityWell;
  battle::UnitIndex unit = battle::kNoUnit;
  std::string text;
};

// What the watcher needs to know about a unit beyond the view model.
struct UnitFacts {
  std::string name;
  bool carrier = false; // has hangar capacity
  bool tractor = false; // has a tractor beam
};
using UnitFactsLookup = std::function<const UnitFacts&(battle::UnitIndex)>;
// A ship's effective withdrawal rule (its group's, or its override); asked only when a threshold may have been crossed.
using WithdrawRuleLookup = std::function<battle::WithdrawRule(battle::UnitIndex)>;

class AutoPauseWatcher
{
  public:
  // The events since the previous call, for `player`'s point of view; the first call only records the state.
  std::vector<AutoPauseHit> Update(const battle::ViewModel& view, battle::Side player, const UnitFactsLookup& facts,
                                   const WithdrawRuleLookup& withdrawRule);

  private:
  struct Seen {
    bool present = false;
    bool gone = false; // destroyed
    float hull = 1.0f;
    float shield = 1.0f;
  };
  std::vector<Seen> m_seen;
  std::vector<battle::UnitIndex> m_activeWells;
  float m_superlaser = -1.0f;
  bool m_started = false;
};

} // namespace reb::battle_ui
