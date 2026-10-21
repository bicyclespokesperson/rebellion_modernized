#pragma once
#include "battle_ui/battle_source.h"
#include "space_battle/battle.h"

// The simulator as the battle screen's source. The caller owns the Battle and keeps it alive while a screen uses it.
namespace reb::battle_ui
{

class LiveBattle : public BattleSource
{
  public:
  explicit LiveBattle(battle::Battle& battle)
      : m_battle(battle)
  {
  }

  bool Submit(const battle::Command& command) override
  {
    return m_battle.Submit(command);
  }
  void Step() override
  {
    m_battle.Step();
  }
  void SkipOutro() override
  {
    m_battle.SkipOutro();
  }
  bool Finished() const override
  {
    return m_battle.Finished();
  }
  int64_t Tick() const override
  {
    return m_battle.Tick();
  }
  void SetAiControlled(battle::Side side, bool ai) override
  {
    m_battle.SetAiControlled(side, ai);
  }
  void BuildViewModel(float alpha, battle::ViewModel& out) const override
  {
    m_battle.BuildViewModel(alpha, out);
  }
  battle::ViewUnitDetail Detail(battle::UnitIndex unit) const override
  {
    return m_battle.Detail(unit);
  }

  private:
  battle::Battle& m_battle;
};

} // namespace reb::battle_ui
