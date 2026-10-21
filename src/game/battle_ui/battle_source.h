#pragma once
#include <cstdint>

#include "space_battle/commands.h"
#include "space_battle/view_model.h"

// What the battle screen drives: the simulator (reb::battle::Battle), or a fabricated stand-in for UI work and tests.
namespace reb::battle_ui
{

class BattleSource
{
  public:
  virtual ~BattleSource() = default;
  virtual bool Submit(const battle::Command& command) = 0;
  virtual void Step() = 0; // one 20 Hz tick
  virtual void SkipOutro() {}
  virtual bool Finished() const = 0;
  virtual int64_t Tick() const = 0;
  virtual void SetAiControlled(battle::Side side, bool ai) = 0;
  virtual void BuildViewModel(float alpha, battle::ViewModel& out) const = 0;
  virtual battle::ViewUnitDetail Detail(battle::UnitIndex unit) const = 0;
};

} // namespace reb::battle_ui
