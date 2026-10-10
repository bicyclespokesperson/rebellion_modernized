#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "audio/sound_manager.h"
#include "core/layout.h"
#include "platform/win32.h"
#include "ui/cool_game_window.h"
#include "ui/cool_label.h"

class CoolDragList;
class CoolStrobeButton;
class CoolTabControl;
class DibBitmap;
class GalaxyWindow;
class MessageItemList;
class MessageListItem;
class TextScrollField;

// Message index tabs (child ids of the tab buttons) and the mode the window is in.
enum MessageIndexMode : int {
    kIndexModeList = 1,     // the list of messages
    kIndexModeMessage = 2,  // one message
    kIndexModeCompose = 3,  // writing a message to the other player (multiplayer)
};

// The Message Index (galaxy view child 0x13, F6): the notices of the human player (UiNotice records) in a list with
// a tab per category; opening one shows its text and pictures, plays its sounds and offers the buttons its class
// allows (go to the object, accept, decline, next, previous, delete). In multiplayer the same window composes
// messages. The window also hosts the modal "report" hand-over of the type-0x1c windows (ModalReportClose).
class MessageIndexWindow : public CoolGameWindow {
public:
    MessageIndexWindow(HINSTANCE instance, CoolWindow* parent, uint32_t childId, int mode, int tab);  // 0x466350
    virtual ~MessageIndexWindow();  // 0x4664d0 (scalar deleting destructor 0x4664b0)

    virtual LRESULT REB_STDCALL WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x467e80
    virtual void PostCloseRequest() override;                  // slot 12, 0x46a6a0
    virtual void OnCreate1() override;                         // slot 14, 0x4665f0
    virtual void DrawOverlay(HDC dc) override;                 // slot 17, 0x468970
    virtual void OnCommand(int a, int b, int c) override;      // slot 18, 0x467f10
    virtual void OnKeyDown(WPARAM key, LPARAM lParam) override;  // slot 19, 0x46a4d0

    // Rebuilds the rows from the human player's notices; `keepAllSelected` keeps every selected row selected,
    // otherwise only the current message.
    int RebuildList(int keepAllSelected);   // 0x468ab0
    // Shows only the rows whose categories pass the tab's filter and makes sure one is selected.
    void UpdateRowVisibility();             // 0x468f20
    // Switches to a mode (kIndexMode*; 0x65: the back button, 0x68: compose); returns 1 if the layout changed.
    int SetMode(int request);               // 0x468fb0
    // Selects the category tab `tab` (0x79 all ... 0x82).
    void SelectCategory(int tab);           // 0x4697b0
    // After the window was reopened: returns to the list, or refreshes the single message.
    void Refresh();                         // 0x469d20
    // Shows `item`'s message (or opens its report window).
    void ShowMessage(MessageListItem* item);  // 0x469de0
    // Enables the next/previous buttons by what the current row has around it.
    void UpdateNavButtons();                // 0x46a200
    // The picture `pictureId` of the message index resources (the encyclopedia DLL if it is not in the strategy art).
    DibBitmap* LoadPicture(uint32_t pictureId);  // 0x46a280
    // Draws the two pictures of a message into the page background.
    void ShowPictures(uint32_t pictureA, uint32_t pictureB);  // 0x46a320
    // Stops the two sounds of the previous message.
    void StopSounds();                      // 0x46a6e0

    GalaxyWindow* m_galaxy;         // +0x114
    int m_mode;                     // +0x118 MessageIndexMode, 0 before OnCreate1
    int m_tab;                      // +0x11c the selected category tab, 0 for none
    HMODULE m_artModule;            // +0x120 resource slot 7
    HMODULE m_encyclopediaModule;   // +0x124 resource slot 5 (ENCYBMAP.DLL)
    SoundRequest m_soundA;          // +0x128 sound of the shown message
    int m_unk13c;                   // +0x13c
    SoundRequest m_soundB;          // +0x140
    int m_unk154;                   // +0x154
    uint32_t m_filter;              // +0x158 category bit of the selected tab
    CoolWindow* m_unk15c;           // +0x15c never set
    CoolTabControl* m_tabs;         // +0x160
    DibBitmap* m_pictureBackdrop;   // +0x164 the page background under the message pictures
    CoolDragList* m_list;           // +0x168
    MessageItemList* m_items;       // +0x16c owned
    MessageListItem* m_current;     // +0x170 the message shown in kIndexModeMessage
    CoolLabel m_title;              // +0x174
    CoolLabel m_tabLabel;           // +0x1a8
    TextScrollField* m_text;        // +0x1dc the message text / the text being composed
};

// The state the type-0x1c report windows leave for the message index to continue (ids at 0x6b2904-0x6b2914).
void ModalReportClose(int owner, int notice);  // 0x469d60
void ModalReportReset(int owner);              // 0x469db0

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(MessageIndexWindow, 0x1e0);
REB_ASSERT_OFFSET(MessageIndexWindow, m_galaxy, 0x114);
REB_ASSERT_OFFSET(MessageIndexWindow, m_soundA, 0x128);
REB_ASSERT_OFFSET(MessageIndexWindow, m_soundB, 0x140);
REB_ASSERT_OFFSET(MessageIndexWindow, m_filter, 0x158);
REB_ASSERT_OFFSET(MessageIndexWindow, m_tabs, 0x160);
REB_ASSERT_OFFSET(MessageIndexWindow, m_list, 0x168);
REB_ASSERT_OFFSET(MessageIndexWindow, m_current, 0x170);
REB_ASSERT_OFFSET(MessageIndexWindow, m_title, 0x174);
REB_ASSERT_OFFSET(MessageIndexWindow, m_tabLabel, 0x1a8);
REB_ASSERT_OFFSET(MessageIndexWindow, m_text, 0x1dc);
#pragma clang diagnostic pop
