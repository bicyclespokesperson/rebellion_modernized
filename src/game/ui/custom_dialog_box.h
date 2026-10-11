#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/cool_game_window.h"

class CoolLabel;
class EventKindSet;
class GameObj;
class GameObjNotifier;
class CoolStringField;
class DibBitmap;

// An object key together with the ids of the list rows (or other places) that show it.
class ObjectKeyIdNode : public ObjectKeyNode {
public:
    ObjectKeyIdNode();                                   // 0x4acd10
    ObjectKeyIdNode(const ObjectKey* key, uint32_t id);  // 0x4acd80: id 0 adds none
    virtual ~ObjectKeyIdNode();                          // 0x4acdf0 (scalar deleting destructor 0x4acd60)

    void AddId(uint32_t id);  // 0x4ace40: unless already present

    IdAvlTree m_ids;  // +0x1c
};

// ObjectKeySet of ObjectKeyIdNodes: the objects a CustomDialogBox shows.
class ObjectKeyIdSet : public ObjectKeySet {
public:
    ObjectKeyIdSet();           // 0x4acb50
    virtual ~ObjectKeyIdSet();  // 0x4acb90 (scalar deleting destructor 0x4acb70)

    // Takes `node` (if its key is complete): a node for a key already present only adds `id` to that node and is
    // deleted. id 0 adds no id.
    void Add(ObjectKeyIdNode* node, uint32_t id);  // 0x4acba0
    // Removes the set's node with `node`'s key, and deletes `node`.
    void Remove(ObjectKeyIdNode* node);             // 0x4acca0
};

class TextMessage;  // the order a rename is applied to (human_player.cpp builds it: the new name goes into m_text)

// "CustomDialogBox": the base class of the strategy object windows (sector, system, fleet, object status, mission
// windows...). A CoolGameWindow with a title label, the set of objects it shows, the selection, and in-place
// renaming of an object through a CoolStringField. Right-clicking an object opens the object pop-up menu.
class CustomDialogBox : public CoolGameWindow {
public:
    // `backgroundSource`, when given, is the picture the window's background is cut from at (x, y).
    CustomDialogBox(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t childId,
                    DibBitmap* backgroundSource);  // 0x4ac120
    virtual ~CustomDialogBox();  // 0x4ac2f0 (scalar deleting destructor 0x4ac2d0)

    // Escape closes the window; a click ends a rename in progress.
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x4ac3a0
    // Opens the object pop-up menu for the selection (or the clicked object).
    virtual int OnRightButtonUp(int x, int y, CoolWindow* sender) override;  // slot 7, 0x4ac5c0
    // Hands the selection to the game (game_ui_facade 0x41cdf0) with `a` as the command.
    virtual void Virtual08(int a, int b, int c) override;  // slot 8, 0x4ac730

    virtual void GetSelection(ObjectKeyValueSet* out, CoolWindow* source);  // slot 22, 0x461740 (shared): nothing
    // `object` was added to a shown object (an object of the window's keys): slot 23, 0x461740 (shared): nothing.
    virtual void OnChildAdded(const ObjectKey* key, GameObjNotifier* object);
    // `object` moved to `otherObject` (slot 24, 0x461730, shared: nothing).
    virtual void OnChildMoved(const ObjectKey* key, GameObjNotifier* object, const ObjectKey* otherKey,
                              GameObjNotifier* otherObject);
    // `object` changed (slot 25, 0x461740, shared: nothing).
    virtual void OnObjectUpdated(const ObjectKey* key, GameObjNotifier* object);
    // The object at (x, y): none (the unset key) here.
    virtual ObjectKey GetKeyAt(int x, int y);                              // slot 26, 0x4ac4b0
    // Brings the page of the window that shows `key` to the front (slot 27, 0x45d3f0, shared): nothing.
    virtual void ShowObjectPage(const ObjectKey* key);
    virtual ObjectKey GetDropKeyAt(int x, int y);                          // slot 28, 0x4ac520: GetKeyAt
    virtual int Virtual29();                                                // slot 29, 0x5a5b60 (shared): returns 0
    // Opens an edit field over the name of the object `target` refers to (nothing if the object is unknown).
    virtual void BeginRename(TextMessage* target);                               // slot 30, 0x4ac7a0
    virtual void CommitRename();                                            // slot 31, 0x4ac950
    virtual void CancelRename();                                            // slot 32, 0x4aca40
    // The first selected object's key; 0 if nothing is selected.
    virtual int GetFirstSelectedKey(ObjectKey* out);                        // slot 33, 0x4aca80
    virtual RECT* GetRenameRect(RECT* out);                                 // slot 34, 0x4acb20: 75x20 at 0,0
    virtual void Virtual35();                                               // slot 35, 0x619730 (shared): nothing

    ObjectKeyIdSet m_objects;          // +0x114
    ObjectKeyValueSet m_selection;     // +0x11c
    CoolLabel* m_titleLabel;           // +0x124 owned
    RECT m_titleRect;                  // +0x128
    CoolStringField* m_renameField;    // +0x138 owned, while renaming
    TextMessage* m_renameTarget;   // +0x13c deleted by the destructor
};
