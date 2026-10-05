#pragma once
#include <cstdint>

#include "core/layout.h"
#include "platform/win32.h"
#include "ui/cool_label.h"
#include "ui/game_window_object.h"

class CoolCycleButton;
class CoolFrameButton;
class CoolStrobeButton;
class CoolStrobeTimer;
class DibBitmap;

// The front-end screen ("shuttle cockpit", window id 4 of the application): the new game buttons for the two
// sides, the multiplayer / load / options / quit consoles and the three option groups. It is built from COMMON.DLL
// (slot 8) artwork and unloads that DLL (and the strings of slot 1) again when a game starts.
//
// The cycle button (m_cycleButton) does not report which of its three positions was clicked, so WM_LBUTTONUP looks
// the click up in the three rectangles of m_regions and replays it onto the button.
class MainMenuWindow : public GameWindowObject {
public:
    MainMenuWindow(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent);  // 0x404c30
    virtual ~MainMenuWindow();  // 0x404d90 (scalar deleting destructor 0x404d70)

    virtual LRESULT __stdcall WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x405050

    void DestroyControls();  // 0x404ec0: deletes every control (some of the pointers stay dangling)
    void HideControls();     // 0x404fa0
    void CreateControls();   // 0x405560
    void OnCommand(uint32_t commandId);  // 0x406000

    CoolStrobeTimer* m_strobeTimer;      // +0x9c owned, steps the console button animations
    CoolLabel* m_hintLabel;              // +0xa0 owned, the text under the option toggle
    CoolStrobeButton* m_quitButton;      // +0xa4 owned, command 0x73
    CoolStrobeButton* m_toggleButton;    // +0xa8 owned, command 0x71
    CoolCycleButton* m_cycleButton;      // +0xac owned, command 0x6a
    CoolFrameButton* m_empireButton;     // +0xb0 owned, command 0x66
    CoolFrameButton* m_allianceButton;   // +0xb4 owned, command 0x65
    CoolFrameButton* m_modeButton0;      // +0xb8 owned, command 0x6e
    CoolFrameButton* m_modeButton1;      // +0xbc owned, command 0x6f
    CoolFrameButton* m_modeButton2;      // +0xc0 owned, command 0x70
    CoolFrameButton* m_multiplayerButton;  // +0xc4 owned, command 0x67
    CoolFrameButton* m_loadButton;       // +0xc8 owned, command 0x68
    CoolFrameButton* m_optionsButton;    // +0xcc owned, command 0x69
    DibBitmap* m_cycleImages[3];         // +0xd0 owned, drawn next to the cycle button for values 3, 2, 1
    RectNode* m_clickedRegion;           // +0xdc the region of m_regions the last button release hit
    uint32_t m_unknownE0;                // +0xe0
    RectTree m_regions;                  // +0xe4 IndexedRectNodes of the cycle button's three positions
    int m_cycleValue;                    // +0xec 0..3, the value the cycle button shows (starts at 1)
    int m_gameType;                      // +0xf0 1, 2 after the multiplayer button
    int m_connectionKind;                // +0xf4 1 or 2, the option toggle
    int m_mode;                          // +0xf8 0..2, which of the mode buttons is checked
    uint32_t m_unknownFC;                // +0xfc
    int m_leaving;                       // +0x100 set when a game starts: the sound system stays up
    int m_playingMovie;                  // +0x104
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(MainMenuWindow, 0x108);
REB_ASSERT_OFFSET(MainMenuWindow, m_strobeTimer, 0x9c);
REB_ASSERT_OFFSET(MainMenuWindow, m_cycleButton, 0xac);
REB_ASSERT_OFFSET(MainMenuWindow, m_cycleImages, 0xd0);
REB_ASSERT_OFFSET(MainMenuWindow, m_regions, 0xe4);
REB_ASSERT_OFFSET(MainMenuWindow, m_cycleValue, 0xec);
REB_ASSERT_OFFSET(MainMenuWindow, m_playingMovie, 0x104);
#pragma clang diagnostic pop

extern MainMenuWindow* g_mainMenuWindow;  // 0x6b06f4

// The movie-done callback of the quit button's movie (0x406520).
void MainMenuMovieDone();   // 0x406520
// The 66 ms timer callback (0x4054a0): steps the console buttons' animations.
void MainMenuStrobeStep();  // 0x4054a0
