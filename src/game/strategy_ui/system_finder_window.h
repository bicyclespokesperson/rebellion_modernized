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

// The tabs of the system finder: which systems the list shows.
enum SystemFinderFilter : int {
    kSystemsAll = 1,
    kSystemsAlliance = 2,    // controlled by the Alliance
    kSystemsEmpire = 3,      // controlled by the Empire
    kSystemsContested = 4,   // explored systems with control side 3
    kSystemsUnexplored = 5,
};

// The System Finder (galaxy view child 0x14, F2): a name field and a list of the systems of the galaxy, filtered by
// five tabs; selecting one and pressing the open button (or double-clicking) opens its sector window.
class SystemFinderWindow : public CustomDialogBox {
public:
    SystemFinderWindow(HINSTANCE instance, int x, int y, int width, int height, GalaxyWindow* parent, uint32_t childId);  // 0x460090
    virtual ~SystemFinderWindow();  // 0x4601a0 (scalar deleting destructor 0x460180)

    virtual LRESULT __stdcall WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x461000
    virtual void OnCreate1() override;                    // slot 14, 0x460270
    virtual void DrawOverlay(HDC dc) override;            // slot 17, 0x460fe0
    virtual void OnCommand(int a, int command, int c) override;  // slot 18, 0x460d10
    virtual void OnKeyDown(WPARAM key, LPARAM lParam) override;  // slot 19, 0x461590

    // Fills the list with the systems of the tab `filter` (SystemFinderFilter) unless it is the current one.
    void SetFilter(int filter, int unused);  // 0x4611d0

    uint32_t m_unknown140;        // +0x140 not initialised
    CoolTabControl* m_tabs;       // +0x144 child id 100
    CoolStringField* m_nameField;  // +0x148 child id 0xcb
    CoolDragList* m_list;         // +0x14c child id 0xca
    GalaxyWindow* m_galaxy;       // +0x150
    CoolLabel* m_filterLabel;     // +0x154 tooltip text of the selected tab
    int m_filter;                 // +0x158 SystemFinderFilter shown, 0 before the first SetFilter
    CoolListItemList* m_items;    // +0x15c owned, the ObjectListItem rows
    ObjectKey m_selected;         // +0x160
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(SystemFinderWindow, 0x164);
REB_ASSERT_OFFSET(SystemFinderWindow, m_tabs, 0x144);
REB_ASSERT_OFFSET(SystemFinderWindow, m_nameField, 0x148);
REB_ASSERT_OFFSET(SystemFinderWindow, m_list, 0x14c);
REB_ASSERT_OFFSET(SystemFinderWindow, m_galaxy, 0x150);
REB_ASSERT_OFFSET(SystemFinderWindow, m_filterLabel, 0x154);
REB_ASSERT_OFFSET(SystemFinderWindow, m_filter, 0x158);
REB_ASSERT_OFFSET(SystemFinderWindow, m_items, 0x15c);
REB_ASSERT_OFFSET(SystemFinderWindow, m_selected, 0x160);
#pragma clang diagnostic pop
