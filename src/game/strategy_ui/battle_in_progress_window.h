#pragma once
#include <cstdint>

#include "platform/win32.h"
#include "ui/cool_game_window.h"

class GalaxyWindow;
class istream;
class ostream;

// The "Battle in Progress..." banner (galaxy view child type 2/side, 380x200, centred in the galaxy view's panel area)
// shown while the opponent in a multiplayer game fights a battle; its text is drawn in the side's colour.
class BattleInProgressWindow : public CoolGameWindow
{
public:
  BattleInProgressWindow(HINSTANCE instance, GalaxyWindow* parent, uint32_t childId); // 0x49ee20
  ~BattleInProgressWindow() override; // 0x49ee90 (scalar deleting destructor 0x49ee70)

  void SaveState(ostream* out) override; // slot 9, 0x49f0f0: the child id
  void LoadState(istream* in) override; // slot 10, 0x49f110
  void OnCreate1() override; // slot 14, 0x49eea0

  GalaxyWindow* m_galaxy; // +0x114
};
