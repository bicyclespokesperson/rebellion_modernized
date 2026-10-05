#pragma once
#include "platform/win32.h"

class CoolDragList;
class CoolStringField;
class CoolTabControl;
class CustomDialogBox;

// The key handling shared by the finder windows (system, fleet, troop and personnel finders): Tab is ignored, Escape
// closes the window, Left/Right step through the visible filter tabs, Up moves to the name field, Down to the list
// (when it has rows); every other key goes to the window's parent.
void FinderOnKeyDown(CustomDialogBox* window, CoolTabControl* tabs, CoolStringField* nameField, CoolDragList* list,
                     WPARAM key, LPARAM lParam);
