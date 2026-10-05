#pragma once
#include <cstdint>

#include "core/layout.h"
#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/custom_dialog_box.h"

class CoolDragList;
class CoolLabel;
class CoolListItemList;
class CoolStringField;
class CoolTabControl;
class GalaxyWindow;

// The pages of the fleet finder (the page tabs) and the filter tabs of each page.
enum FleetFinderMode : int {
    kFleetFinderFleets = 1,
    kFleetFinderCapitalShips = 2,
};
enum FleetFinderFilter : int {
    kFleetsAll = 1,
    kFleetsAlliance = 2,
    kFleetsEmpire = 3,
};

// The Fleet Finder (galaxy view child 0x15, F3): two pages (the fleets, or the capital ships) with a name field and a
// list filtered by three tabs. Only objects that exist and are not on their way (their location is not a side root)
// are listed. Opening a row shows the object in its sector or system window.
class FleetFinderWindow : public CustomDialogBox {
public:
    FleetFinderWindow(HINSTANCE instance, int x, int y, int width, int height, GalaxyWindow* parent, uint32_t childId);  // 0x461750
    virtual ~FleetFinderWindow();  // 0x461880 (scalar deleting destructor 0x461860)

    virtual LRESULT __stdcall WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x462a50
    virtual void OnCreate1() override;                    // slot 14, 0x461960
    virtual void DrawOverlay(HDC dc) override;            // slot 17, 0x462a30
    virtual void OnCommand(int a, int command, int c) override;  // slot 18, 0x462770
    virtual void OnKeyDown(WPARAM key, LPARAM lParam) override;  // slot 19, 0x463360

    // Fills the list of the current page for the filter tab `filter` unless it is the current one.
    void SetFilter(int filter, int unused);  // 0x462be0
    // Switches to page `mode` (FleetFinderMode) and shows its filter again.
    void SetMode(int mode);                  // 0x4632d0

    uint32_t m_unknown140;         // +0x140 not initialised
    CoolTabControl* m_pageTabs;    // +0x144 child id 0xfa
    int m_mode;                    // +0x148 FleetFinderMode, 0 before OnCreate1 ends
    CoolTabControl* m_filterTabs;  // +0x14c child id 100
    CoolStringField* m_nameField;  // +0x150 child id 0xcd
    CoolDragList* m_list;          // +0x154 child id 0xcc
    GalaxyWindow* m_galaxy;        // +0x158
    CoolLabel* m_filterLabel;      // +0x15c tooltip text of the selected filter tab
    int m_filter;                  // +0x160 FleetFinderFilter shown, 0 while the list is rebuilt
    int m_previousFilter;          // +0x164 the filter SetMode shows again
    CoolListItemList* m_items;     // +0x168 owned
    ObjectKey m_selected;          // +0x16c
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(FleetFinderWindow, 0x170);
REB_ASSERT_OFFSET(FleetFinderWindow, m_pageTabs, 0x144);
REB_ASSERT_OFFSET(FleetFinderWindow, m_mode, 0x148);
REB_ASSERT_OFFSET(FleetFinderWindow, m_filterTabs, 0x14c);
REB_ASSERT_OFFSET(FleetFinderWindow, m_nameField, 0x150);
REB_ASSERT_OFFSET(FleetFinderWindow, m_list, 0x154);
REB_ASSERT_OFFSET(FleetFinderWindow, m_galaxy, 0x158);
REB_ASSERT_OFFSET(FleetFinderWindow, m_filterLabel, 0x15c);
REB_ASSERT_OFFSET(FleetFinderWindow, m_filter, 0x160);
REB_ASSERT_OFFSET(FleetFinderWindow, m_previousFilter, 0x164);
REB_ASSERT_OFFSET(FleetFinderWindow, m_items, 0x168);
REB_ASSERT_OFFSET(FleetFinderWindow, m_selected, 0x16c);
#pragma clang diagnostic pop
