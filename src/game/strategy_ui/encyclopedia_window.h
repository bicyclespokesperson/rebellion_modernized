#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "core/layout.h"
#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/cool_game_window.h"

class CoolDragList;
class CoolLabel;
class CoolListItem;
class CoolListItemList;
class CoolStrobeButton;
class CoolStringField;
class CoolTabControl;
class DibBitmap;
class GalaxyWindow;
class String;
class TextScrollField;

// The two pages of the encyclopedia window.
enum EncyclopediaMode : int {
    kEncyclopediaList = 1,     // filter tabs, search field and the list of entries
    kEncyclopediaArticle = 2,  // picture, article text and previous/next buttons
};

// The encyclopedia (galaxy view child 0x19): a list of every entry the player knows (the galaxy view's
// m_encyclopediaItems), filtered by category tabs and searched by prefix, and an article page with the entry's
// picture (an EData/EDATA.nnn BMP file named by ENCYBMAP.DLL) and text (RT_RCDATA of ENCYTEXT.DLL).
class EncyclopediaWindow : public CoolGameWindow {
public:
    // Opens on the entry of `objectKey` (a game object) or, if that is unset, of `typeKey` (a data-table record);
    // on the list page if neither names a known entry.
    EncyclopediaWindow(HINSTANCE instance, int x, int y, int width, int height, GalaxyWindow* parent,
                       uint32_t childId, const ObjectKey& objectKey, const NullableKey& typeKey);  // 0x45d400
    virtual ~EncyclopediaWindow();  // 0x45d7a0 (scalar deleting destructor 0x45d780)

    virtual LRESULT REB_STDCALL WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x45d8f0
    virtual void PostCloseRequest() override;            // slot 12, 0x460070: resumes the game first
    virtual void OnCreate1() override;                   // slot 14, 0x45ddc0: builds the backgrounds and children
    virtual void DrawOverlay(HDC dc) override;           // slot 17, 0x45f090
    virtual void OnCommand(int a, int b, int c) override;  // slot 18, 0x45da70
    virtual void OnKeyDown(WPARAM key, LPARAM lParam) override;  // slot 19, 0x45fe60

    // Shows the entries of category tab `tabId` (0x6f: all). With `reload` the full list is set again even on the
    // article page (and, on the list page, the selection is dropped).
    void SetFilter(int tabId, uint32_t reload);           // 0x45f100
    void SetMode(int mode);                                // 0x45f480 EncyclopediaMode
    int SystemPictureId(int pictureIndex);                 // 0x45f660: EData picture of a star system type
    void LoadArticle();                                    // 0x45fa60: picture, text and title of m_selected
    void UpdateNavigationButtons(CoolListItem* item);      // 0x45fd20
    // The key of the object whose entry stands for `key`: a manufacturing manager is shown as its owner.
    ObjectKey* ResolveEntryKey(ObjectKey* out, const ObjectKey* key);  // 0x45fd90

    int m_mode;                          // +0x114 EncyclopediaMode
    int m_filterTab;                     // +0x118 child id of the selected category tab
    CoolTabControl* m_pageTabs;          // +0x11c tabs 0x67 (article) and 0x68 (list)
    CoolLabel* m_titleLabel;             // +0x120
    CoolTabControl* m_filterTabs;        // +0x124 category tabs 0x6f-0x75
    CoolStringField* m_searchField;      // +0x128 child id 100
    CoolDragList* m_list;                // +0x12c child id 0x65
    CoolLabel* m_categoryLabel;          // +0x130 tooltip text of the selected category tab
    CoolListItemList* m_filteredItems;   // +0x134 owned copies for the category tabs
    uint32_t m_unknown138;               // +0x138
    TextScrollField* m_articleText;      // +0x13c
    CoolStrobeButton* m_nextButton;      // +0x140 child id 0x84
    CoolStrobeButton* m_previousButton;  // +0x144 child id 0x83
    CoolListItem* m_selected;            // +0x148 entry of the galaxy view's list
    DibBitmap* m_picture;                // +0x14c owned
    String* m_text;                      // +0x150 owned scratch copy of the article text
    uint32_t m_unknown154;               // +0x154
    HMODULE m_textModule;                // +0x158 ENCYTEXT.DLL (resource slot 4)
    HMODULE m_bitmapModule;              // +0x15c ENCYBMAP.DLL (resource slot 5)
    GalaxyWindow* m_galaxy;              // +0x160
};

// Heap copy (delete with operator delete) of the EData directory: the registry value EncyLocation, or "EDATA\",
// always ending in a backslash.
char* GetEncyclopediaDataPath();  // 0x45f7b0
// Loads the BMP file named by string `pictureId` of `module` from the EData directory.
DibBitmap* LoadEncyclopediaPicture(uint32_t pictureId, HMODULE module);  // 0x45f970

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(EncyclopediaWindow, 0x164);
REB_ASSERT_OFFSET(EncyclopediaWindow, m_mode, 0x114);
REB_ASSERT_OFFSET(EncyclopediaWindow, m_pageTabs, 0x11c);
REB_ASSERT_OFFSET(EncyclopediaWindow, m_list, 0x12c);
REB_ASSERT_OFFSET(EncyclopediaWindow, m_filteredItems, 0x134);
REB_ASSERT_OFFSET(EncyclopediaWindow, m_articleText, 0x13c);
REB_ASSERT_OFFSET(EncyclopediaWindow, m_selected, 0x148);
REB_ASSERT_OFFSET(EncyclopediaWindow, m_textModule, 0x158);
REB_ASSERT_OFFSET(EncyclopediaWindow, m_galaxy, 0x160);
#pragma clang diagnostic pop
