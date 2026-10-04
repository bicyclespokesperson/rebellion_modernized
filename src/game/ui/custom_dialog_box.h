#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/cool_game_window.h"

class CoolLabel;
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
    virtual LRESULT __stdcall WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x4ac3a0
    // Opens the object pop-up menu for the selection (or the clicked object).
    virtual int OnRightButtonUp(int x, int y, CoolWindow* sender) override;  // slot 7, 0x4ac5c0
    // Hands the selection to the game (game_ui_facade 0x41cdf0) with `a` as the command.
    virtual void Virtual08(int a, int b, int c) override;  // slot 8, 0x4ac730

    virtual void GetSelection(ObjectKeyValueSet* out, CoolWindow* source);  // slot 22, 0x461740 (shared): nothing
    virtual void Virtual23(int a, int b);                                   // slot 23, 0x461740
    virtual void Virtual24(int a, int b, int c, int d);                     // slot 24, 0x461730 (shared): nothing
    virtual void Virtual25(int a, int b);                                   // slot 25, 0x461740
    // The object at (x, y): none (the unset key) here.
    virtual ObjectKey GetKeyAt(int x, int y);                              // slot 26, 0x4ac4b0
    virtual void Virtual27(int a);                                          // slot 27, 0x45d3f0 (shared): nothing
    virtual ObjectKey GetDropKeyAt(int x, int y);                          // slot 28, 0x4ac520: GetKeyAt
    virtual int Virtual29();                                                // slot 29, 0x5a5b60 (shared): returns 0
    // Opens an edit field over the object's name.
    virtual void BeginRename(IdNode* target);                               // slot 30, 0x4ac7a0
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
    IdNode* m_renameTarget;            // +0x13c deleted by the destructor
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ObjectKeyIdNode, 0x28);
REB_ASSERT_OFFSET(ObjectKeyIdNode, m_ids, 0x1c);
REB_ASSERT_SIZE(ObjectKeyIdSet, 0x08);
REB_ASSERT_SIZE(CustomDialogBox, 0x140);
REB_ASSERT_OFFSET(CustomDialogBox, m_objects, 0x114);
REB_ASSERT_OFFSET(CustomDialogBox, m_selection, 0x11c);
REB_ASSERT_OFFSET(CustomDialogBox, m_titleLabel, 0x124);
REB_ASSERT_OFFSET(CustomDialogBox, m_titleRect, 0x128);
REB_ASSERT_OFFSET(CustomDialogBox, m_renameField, 0x138);
REB_ASSERT_OFFSET(CustomDialogBox, m_renameTarget, 0x13c);
#pragma clang diagnostic pop
