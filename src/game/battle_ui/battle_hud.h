#pragma once
#include "battle_ui/battle_screen.h"

// The battle screen's built-in HUD panels (tactical_combat.md §10.2): the top bar and the selected-unit box.
namespace reb::battle_ui
{

// Pause and speeds, the battle clock, both sides' strength, and the player's command signals.
class TopBarPanel : public HudPanel
{
  public:
  void Draw(Ui& ui, BattleScreen& screen) override;
};

// The selection: one unit's hull, shields, systems and intent, or a list of the selected units.
class InfoBoxPanel : public HudPanel
{
  public:
  void Draw(Ui& ui, BattleScreen& screen) override;
};

// A line of key reminders in the corner.
class KeyHintsPanel : public HudPanel
{
  public:
  void Draw(Ui& ui, BattleScreen& screen) override;
};

std::string FormatClock(float seconds);

} // namespace reb::battle_ui
