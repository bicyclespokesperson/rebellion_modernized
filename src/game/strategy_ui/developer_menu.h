#pragma once

class GameWindowObject;

constexpr int kDeveloperSwitchSides = 0xf001;
constexpr int kDeveloperToggleEnhancements = 0xf002;

// Opens the D-key developer menu over either the cockpit or the strategic view.
void ShowAiDeveloperMenu(GameWindowObject* window, int x, int y, int resourceSlot, bool canSwitchSides);
// Returns true when command was an AI personality selection or the enhancements toggle.
bool HandleAiDeveloperMenuCommand(int command);
bool IsTextInputActive();
