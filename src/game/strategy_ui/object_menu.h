#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "platform/win32.h"
#include "strategy_ui/menu_window.h"
#include "ui/cool_game_window.h"

class CoolWindow;
class DibBitmap;

// ---- The object pop-up menu (the "Game Menu Window", 0x4422a0-0x442d70) ----------------------------------------

// A menu level: the window of the entries of one MenuItemList. Clicking an entry sends its command to `m_owner`
// (its Virtual08) and closes the whole menu; a click outside closes it too.
class PopupMenuWindow : public MenuWindowBase {
public:
    PopupMenuWindow(HINSTANCE resources, int x, int y, int width, int height, PopupMenuWindow* parentMenu,
                    CoolWindow* window, CoolWindow* owner, uint32_t entryId, DibBitmap* backdrop);  // 0x4422f0
    virtual ~PopupMenuWindow();  // 0x442370 (scalar deleting destructor 0x442350)

    virtual void Hide() override;                           // slot 2, 0x442b80: Show(0)
    virtual LRESULT REB_STDCALL WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x442d10
    virtual void ShowAndTrack(int command) override;        // slot 11, 0x442b70: CoolWindow::Show
    virtual void PostCloseRequest() override;               // slot 12, 0x442cf0
    virtual int HandleMouse(MenuItem* item, LONG x, LONG y, int message) override;  // slot 22, 0x4424c0

    // Installs the mouse hook (0x442b90) that closes the menu on a click outside it; true if it is installed.
    bool InstallMouseHook();  // 0x442c20

    PopupMenuWindow* m_parentMenu;  // +0x14c
    CoolWindow* m_owner;            // +0x150 receives the chosen command
};

// Adds the entry for `command` (RCDATA `command` of the resource DLL of slot `kind`: its parent, sibling, texts and
// pictures) to the menu that is about to be shown, creating its parents first; returns the entry's list of
// sub-entries. `flags`: 1 = unavailable, 2 = checked. Colours: text, shadow, unavailable text.
IdSortedLinkList* AddMenuEntry(uint32_t command, uint32_t kind, uint32_t flags, int x, int y, CoolWindow* window,
                               uint32_t color, uint32_t shadowColor, uint32_t disabledColor);  // 0x442590
// Opens the menu built by the AddMenuEntry calls at (x, y) over `owner`, closing the one that is open. The last
// argument is ignored.
PopupMenuWindow* ShowMenu(int x, int y, HMODULE resources, CoolWindow* window, CoolWindow* owner,
                          DibBitmap* ignored);  // 0x442380
// Closes the open menu (always returns 1).
int DismissMenu();  // 0x442430

// The 13 words of a menu record (RCDATA): [1] the id of the entry whose sub-menu this is, [2] its own id, [3] the
// id of an entry that is added next to it, [5] and [11] a pair of numbers for the item, [12] its resource slot.
struct MenuRecord {
    uint16_t m_words[13];
};
// Copies the menu record `command` of the resource DLL in slot `kind`; 1 if there is no such record.
int ReadMenuRecord(uint32_t command, uint32_t kind, MenuRecord* out);  // 0x442790
// Sizes the entries of `list` to the widest / tallest, stacks them and returns the size of the menu.
void LayoutMenu(int x, int y, IdSortedLinkList* list, int* outX, int* outY, uint32_t* outWidth, int* outHeight);  // 0x442a80
// Builds the window of one menu level from `list` (emptying it) and, recursively, those of its sub-menus.
PopupMenuWindow* BuildMenuWindow(int x, int y, IdSortedLinkList* list, HMODULE resources, PopupMenuWindow* parentMenu,
                                 CoolWindow* window, CoolWindow* owner, DibBitmap* backdrop);  // 0x442860
int CleanupMenuTree(MenuItem* item);          // 0x442c90
LinkNode* GetSubItemsHead(MenuItem* item);    // 0x442cd0
BOOL RemoveMenuHook();                        // 0x442c60
LRESULT CALLBACK MenuMouseHook(int code, WPARAM wParam, LPARAM lParam);  // 0x442b90

// Static initializer of the menu list (0x4422a0) and its atexit destructor.
void StaticInitMenuList();  // 0x4422a0
void InitMenuList();        // 0x4422b0
void RegisterMenuListDestructor();  // 0x4422c0

extern MenuItemList g_menuRoot;          // 0x6b28e0
extern PopupMenuWindow* g_menuWindow;    // 0x6b28f4 the open menu, or null

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(PopupMenuWindow, 0x154);
#pragma clang diagnostic pop
