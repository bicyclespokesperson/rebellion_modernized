#pragma once
#include "battle_ui/input.h"
#include "battle_ui/ui_types.h"

// Where the battle screen's parts go (tactical_combat.md §10.2). The battle view fills the screen below the top bar;
// panels are drawn over it.
namespace reb::battle_ui
{

struct ScreenLayout {
  Rect screen;
  Rect topBar;
  Rect view;   // the camera's viewport
  Rect roster; // left column: task forces and flights (HUD)
  Rect events; // right column: the event log (HUD)
  Rect card;   // bottom strip: the doctrine card (HUD)
  // The battle view between the HUD's columns and above its card: framing and centring put the battle here.
  Rect battleArea;
  Rect infoBox; // the selected or hovered unit
  Rect hints;   // key hints, bottom right
};

ScreenLayout ComputeLayout(const ScreenInfo& screen);

} // namespace reb::battle_ui
