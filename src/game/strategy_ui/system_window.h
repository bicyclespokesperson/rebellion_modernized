#pragma once
#include <cstdint>

#include "core/layout.h"
#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/custom_dialog_box.h"

class GameObjNotifier;
class RenameRequest;
class String;

// The star system window (galaxy view child type 9, id (system id << 6) | 9): the tabs of the system's fleets,
// troops and facilities, and one status panel per manufacturing manager (slot 1 shipyard, 2 training facility,
// 3 construction yard, 0x38 bytes each starting at +0x168).
class SystemWindow : public CustomDialogBox {
public:
    // The slot (1-3) whose panel belongs to the manufacturing manager `manager`; 0 if none does.
    int SlotOfManager(const ObjectKey* manager);  // 0x4583e0
    // Shows the name of the character `character` on the slot's heading (unless it already does).
    void SetSlotCharacter(int slot, GameObjNotifier* manager, const ObjectKey* character);  // 0x457f30
    // Sets the slot's status text to `text`, or to the default text of the slot / the manager's object.
    void SetSlotStatusText(int slot, GameObjNotifier* manager, String* text);  // 0x457b40
    // Redraws the picture and count of what the slot's manager produces.
    void SetSlotProduction(int slot, GameObjNotifier* manager);  // 0x457c90
    // Sets the slot's progress bar to `position` of `range`.
    void SetSlotProgress(int slot, GameObjNotifier* manager, int position, int range);  // 0x458040
    // Starts renaming for `request` once the window's tab is shown.
    void BeginRenameRequest(RenameRequest* request);  // 0x458b50
};
