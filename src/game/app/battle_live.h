#pragma once
#include <memory>

#include "battle_ui/command_hud.h"
#include "space_battle/battle.h"

// The campaign owns the engine; this session borrows its simulator until results are applied.
class LiveBattleSession final : public reb::battle_ui::BattleHost
{
  public:
  LiveBattleSession(reb::battle::Battle& battle, reb::battle::Side player, bool restoring = false);
  ~LiveBattleSession();
  bool Open();
  bool Advance();
  void GetImmediateResult() override;
  void SetObserving(bool observing) override;
  reb::battle_ui::BattleScreen& Screen();
  reb::battle::Battle& Simulator();

  private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

// The smoke exercises the complete live flow with the dummy renderer; ordinary dummy runs stay headless.
bool LiveBattlePresenterAvailable();
