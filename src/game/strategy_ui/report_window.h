#pragma once
#include <cstdint>

#include "app/player_controller_decl.h"
#include "core/game_string.h"
#include "gameobj/object_key.h"
#include "ui/cool_game_window.h"
#include "ui/cool_label.h"

class CoolDragList;
class CoolListItemList;
class CoolStrobeButton;
class CoolTabControl;
class GalaxyWindow;
class GameObjNotifier;

// A PlayerMessage of kind 7 (queued by the human player, shown by UiOpenType1cWindow): the report of an event with
// a title, a caption, the object it is about (a mission or a fleet) and the object sets of the two sides.
class ReportMessage : public PlayerMessage {
public:
    uint8_t m_unk24[0x0a];   // +0x24
    uint16_t m_bitmapId;     // +0x2e picture (GOKRES) of the report
    uint8_t m_unk30[0x14];   // +0x30
    String m_title;          // +0x44
    String m_caption;        // +0x50
    ObjectKey m_objectKey;   // +0x5c the object the report is about (its sector can be opened)
    int m_unk60;             // +0x60
    int m_showsSecondList;   // +0x64 non-zero: the fighter / troop kind tabs are the default
    int m_secondSide;        // +0x68 colour / art variant of the 0x67 tab buttons
    int m_side;              // +0x6c 1 Alliance, 2 Empire: picks the text colour
    ObjectKeySet m_sets[6];  // +0x70 the participants: tab 0x67 uses 0, 2, 5, tab 0x66 uses 1, 3, 4
};

// Window type 0x1c of the galaxy view (GalaxyWindow::OpenType1cWindow): the report of a ReportMessage. A tab
// control (child 100) selects the report page (0x65), the two participant lists (0x66 / 0x67) or the options page
// (0x68); the lists are built by FillList from the sub tab (0x97-0x9c, one per kind of unit) that is selected.
class ReportWindow : public CoolGameWindow {
public:
    ReportWindow(HINSTANCE instance, GalaxyWindow* parent, uint32_t childId, ReportMessage* report);  // 0x44c410
    virtual ~ReportWindow();  // 0x44c550 (scalar deleting destructor 0x44c530)

    virtual void OnCreate1() override;                           // slot 14, 0x44c630
    virtual void DrawOverlay(HDC dc) override;                   // slot 17, 0x44e290
    virtual void OnCommand(int a, int b, int c) override;        // slot 18, 0x44df90
    virtual void OnKeyDown(WPARAM key, LPARAM lParam) override;  // slot 19, 0x44e180

    void SelectPage(int tabId);                                              // 0x44e3d0
    void FillList(int subTabId, int force);                                  // 0x44e500
    void AddListItem(GameObjNotifier* object, int flag, String* heading);    // 0x44edb0

    uint8_t m_unk114[4];            // +0x114
    GalaxyWindow* m_galaxy;         // +0x118
    uint8_t m_unk11c[4];            // +0x11c
    CoolDragList* m_list;           // +0x120 child id 6: the participants
    CoolListItemList* m_items;      // +0x124 the rows of m_list
    CoolLabel m_label;              // +0x128 draws the tab captions (DrawOverlay)
    CoolTabControl* m_pageTabs;     // +0x15c child id 100
    CoolTabControl* m_unitTabs;     // +0x160 child id 0x96: the kinds of unit of the lists
    CoolStrobeButton* m_closeButton;  // +0x164 child id 10
    CoolStrobeButton* m_previousButton;  // +0x168 child id 12
    CoolStrobeButton* m_nextButton;      // +0x16c child id 11
    uint8_t m_unk170[4];            // +0x170
    ReportMessage* m_report;        // +0x174
    HMODULE m_gokres;               // +0x178 GOKRES.DLL (resource slot 10)
    int m_currentPage;              // +0x17c 0x65-0x68
    int m_listKind;                 // +0x180 the unit tab (0x97-0x9c) the list was built for
    int m_nextItemId;               // +0x184
};
