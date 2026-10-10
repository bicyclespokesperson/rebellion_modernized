#pragma once
#include <cstdint>

#include "core/layout.h"
#include "platform/win32.h"
#include "ui/cool_game_window.h"

class ChildIterator;
class CoolDragList;
class CoolListItemList;
class CoolStrobeButton;
class CoolTabControl;
class GalaxyWindow;
class GameObjNotifier;
class Notice2;

// A [first, last) range of object families; {0, 0} accepts everything (0x4510f0).
struct VoteFamilyRange {
    VoteFamilyRange(uint32_t first, uint32_t last);  // 0x4510f0

    uint32_t m_first;  // +0x00
    uint32_t m_last;   // +0x04
};

// The multiplayer vote window (galaxy view child type 6, 470x331): shown when the opponent's fleet arrives at a
// system the player has forces in, it lists what is there on four tabs (the notice text, the forces of side 1, of
// side 2 and the facilities, troops and characters of the system) and asks whether to fight now (0x65), to retreat
// (0x66) or to have the battle resolved by the game (0x67, only offered when the notice is an attack). The answer
// goes to the human player (UiHumanSetVote); afterwards a "Battle in Progress..." banner replaces the window.
class VoteWindow : public CoolGameWindow {
public:
    VoteWindow(HINSTANCE instance, GalaxyWindow* parent, uint32_t childId, Notice2* notice);  // 0x44f670
    virtual ~VoteWindow();  // 0x44f790 (scalar deleting destructor 0x44f770)

    virtual void OnCreate1() override;                           // slot 14, 0x44f860
    virtual void OnCommand(int a, int b, int c) override;        // slot 18, 0x450540
    virtual void OnKeyDown(WPARAM key, LPARAM lParam) override;  // slot 19, 0x450640

    void SetTab(int tab);                                                              // 0x450660
    void FillList(int page);                                                           // 0x450770
    void AddItems(ChildIterator* objects, VoteFamilyRange families, int widePicture);  // 0x450ce0
    void AddItem(GameObjNotifier* object, int fontSlot, int widePicture, int withPicture);  // 0x450e10
    void ShowButtons(int which, int disabled);                                         // 0x450ff0
    void OpenBattleBanner();                                                           // 0x451050

    Notice2* m_notice;             // +0x114
    GalaxyWindow* m_galaxy;        // +0x118
    CoolWindow* m_extra;           // +0x11c never created
    CoolStrobeButton* m_fightButton;    // +0x120 (0x65)
    CoolStrobeButton* m_retreatButton;  // +0x124 (0x66)
    CoolStrobeButton* m_resolveButton;  // +0x128 (0x67)
    CoolDragList* m_list;          // +0x12c
    CoolListItemList* m_items;     // +0x130 rows of m_list
    CoolTabControl* m_tabs;        // +0x134
    HMODULE m_module;              // +0x138 gokres.dll
    int m_tab;                     // +0x13c
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(VoteFamilyRange, 0x08);
REB_ASSERT_SIZE(VoteWindow, 0x140);
REB_ASSERT_OFFSET(VoteWindow, m_notice, 0x114);
REB_ASSERT_OFFSET(VoteWindow, m_galaxy, 0x118);
REB_ASSERT_OFFSET(VoteWindow, m_extra, 0x11c);
REB_ASSERT_OFFSET(VoteWindow, m_fightButton, 0x120);
REB_ASSERT_OFFSET(VoteWindow, m_resolveButton, 0x128);
REB_ASSERT_OFFSET(VoteWindow, m_list, 0x12c);
REB_ASSERT_OFFSET(VoteWindow, m_items, 0x130);
REB_ASSERT_OFFSET(VoteWindow, m_tabs, 0x134);
REB_ASSERT_OFFSET(VoteWindow, m_module, 0x138);
REB_ASSERT_OFFSET(VoteWindow, m_tab, 0x13c);
#pragma clang diagnostic pop
