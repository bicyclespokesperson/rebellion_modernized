#pragma once
#include <string>
#include <cstdint>

#include "gameobj/object_key.h"
#include "platform/win32.h"

class CoolDragList;
class CoolLabel;
class CoolStringField;
class CoolTabControl;
class CustomDialogBox;
class GameObj;


// The key handling shared by the finder windows (system, fleet, troop and personnel finders): Tab is ignored, Escape
// closes the window, Left/Right step through the visible filter tabs, Up moves to the name field, Down to the list
// (when it has rows); every other key goes to the window's parent when `forwardOtherKeys` is set.
void FinderOnKeyDown(CustomDialogBox* window, CoolTabControl* tabs, CoolStringField* nameField, CoolDragList* list,
                     WPARAM key, LPARAM lParam, bool forwardOtherKeys = true);

// One kind of force (a special force or troop regiment record) counted for a system or fleet row of the personnel and
// troop finders.
struct ForceCell {
    uint16_t m_count;
    uint16_t m_flags;  // kCellFlagPresent, kCellFlagUnusable
    ObjectKey m_key;   // the record key that is counted in this cell
};
constexpr uint32_t kCellFlagPresent = 1;
constexpr uint32_t kCellFlagUnusable = 4;

// Adds `force` to the cell whose key is its record key (if any); a force that is not usable marks the cell.
void CountIntoCell(ForceCell* cells, int cellCount, GameObj* force);

// Draws the counts of `cells` side by side from `firstX`, each over a PatBlt of the brush (of `brushes`) for its
// state; an empty cell shows "-".
void DrawForceCells(const ForceCell* cells, int cellCount, CoolLabel* label, std::string* text, int firstX, int side,
                    const HBRUSH* brushes, HDC dc);
