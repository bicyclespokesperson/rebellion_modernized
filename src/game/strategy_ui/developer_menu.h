#pragma once

class GameWindowObject;

constexpr int kDeveloperSwitchSides = 0xf001;

// Opens the key-3 menu over either the cockpit or the strategic view.
void ShowAiDeveloperMenu(GameWindowObject* window, int x, int y, int resourceSlot, bool canSwitchSides);
// Returns true when command was an AI personality selection.
bool HandleAiDeveloperMenuCommand(int command);
