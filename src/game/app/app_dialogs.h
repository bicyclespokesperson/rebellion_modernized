#pragma once
#include <cstdint>

#include "platform/win32.h"
#include "ui/cool_game_window.h"

// Yes/No question: two lines of text (either may be 0) and the buttons Yes (id 0x65, Return) and No (id 0x66,
// Escape). "Quit and return to cockpit?", "Save anyway?", "Resume Game Play?" ...
class AppDialog : public CoolGameWindow {
public:
    // `message` identifies the question to the main window (g_appDialogMessage); `headlineId` and `detailId` are
    // the texts' string ids, 0 for none.
    AppDialog(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t stringSlot,
              uint32_t bitmapSlot, uint32_t backgroundId, uint32_t childId, uint32_t style, uint32_t flags,
              uint32_t message, uint32_t headlineId, uint32_t detailId);  // 0x416b90
    ~AppDialog() override;  // 0x416c30 (scalar deleting destructor 0x416c10): clears g_appDialogMessage

    void OnCreate1() override;                           // slot 14, 0x416dd0: FocusAndRemember
    void OnCreate2() override;                           // slot 15, 0x416de0: draws the texts
    void OnCreate3() override;                           // slot 16, 0x416e90: the Yes and No buttons
    void OnCommand(int a, int b, int c) override;        // slot 18, 0x416ef0: Yes 0x65 / No 0x66
    void OnKeyDown(WPARAM key, LPARAM lParam) override;  // slot 19, 0x416f30: Return accepts, Escape rejects
    int Virtual14() override;                            // slot 20, 0x416f60: m_enabled

    int m_enabled;       // +0x114 1; the factories delete the dialog if the virtual returns 0
    uint32_t m_headlineId;  // +0x118 string id of the upper text, 0: none
    uint32_t m_detailId;    // +0x11c string id of the lower text, 0: none
};

// Notice with one acknowledge button (id 0x65; Return too): "Your Opponent Has Left The Game", "Selected Game
// Saved Successfully" ... The button's tooltip is the string `buttonTooltipId`.
class AppNoticeDialog : public AppDialog {
public:
    AppNoticeDialog(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t stringSlot,
                    uint32_t bitmapSlot, uint32_t backgroundId, uint32_t childId, uint32_t style, uint32_t flags,
                    uint32_t message, uint32_t headlineId, uint32_t detailId, uint32_t buttonTooltipId);  // 0x416f80
    ~AppNoticeDialog() override;  // 0x417010 (scalar deleting destructor 0x416ff0)

    void OnCreate3() override;                           // slot 16, 0x417150: the OK button
    void OnCommand(int a, int b, int c) override;        // slot 18, 0x417180: 0x65 accepts
    void OnKeyDown(WPARAM key, LPARAM lParam) override;  // slot 19, 0x4171a0: Return accepts

    uint32_t m_buttonTooltipId;  // +0x120
};

// "Waiting For Opponent To Resume": a text without buttons that ignores the keyboard; the game removes it with
// DestroyAppDialog. Always message 0x472.
class AppWaitDialog : public AppDialog {
public:
    AppWaitDialog(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t stringSlot,
                  uint32_t bitmapSlot, uint32_t backgroundId, uint32_t childId, uint32_t style, uint32_t flags,
                  uint32_t message, uint32_t headlineId, uint32_t detailId);  // 0x4171c0
    ~AppWaitDialog() override;  // 0x417250 (scalar deleting destructor 0x417230)

    void OnCreate3() override;                           // slot 16, 0x619730 (shared): does nothing
    void OnCommand(int a, int b, int c) override;        // slot 18, 0x417380: 0x65 rejects
    void OnKeyDown(WPARAM key, LPARAM lParam) override;  // slot 19, 0x403ef0 (shared): does nothing
};

// Factories. The trailing two arguments of every one are pushed by all callers (500, 80 or 600, 120) and never
// read. They return 1 if the dialog was shown, 0 if there is no `app` or (ShowAppDialog) one is already open.
int ShowAppDialog(uint32_t message, CoolWindow* app, uint32_t headlineId, uint32_t detailId, int ignoredA,
                  int ignoredB);  // 0x416c50
int ShowAppNoticeDialog(uint32_t message, CoolWindow* app, uint32_t headlineId, uint32_t detailId,
                        uint32_t buttonTooltipId, int ignoredA, int ignoredB);  // 0x417020
int ShowAppWaitDialog(CoolWindow* app, uint32_t headlineId, uint32_t detailId, int ignoredA, int ignoredB);  // 0x417260

// Removes the dialog; 0 if there was none.
int DestroyAppDialog();  // 0x416d90
void CenterAppDialog();  // 0x416f70

extern AppDialog* g_appDialog;       // 0x6b146c the open dialog or null
extern uint32_t g_appDialogMessage;  // 0x6b1470 message id of the open question, 0 when closed
