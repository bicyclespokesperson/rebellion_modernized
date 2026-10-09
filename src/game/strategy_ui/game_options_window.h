#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "core/layout.h"
#include "platform/win32.h"
#include "ui/game_window_object.h"

class CoolLabel;
class CoolSlider;
class CoolStringField;
class CoolStrobeButton;

// The "Saved Games / Game Options" screen (window id 5): six save slots (name field, save and load buttons), the
// music and sound volume sliders, the option toggles and the way back to the game, the front-end or the
// multiplayer lobby. `m_state` is the application state it was opened for: 4/5 are the multiplayer lobby's
// load list, 8/9/0xc the front-end and the strategic game; m_battleFlowState tells whether a game is running.
class GameOptionsWindow : public GameWindowObject {
public:
    GameOptionsWindow(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                      int state);  // 0x406950
    virtual ~GameOptionsWindow();  // 0x406ac0 (scalar deleting destructor 0x406aa0)

    virtual LRESULT REB_STDCALL WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x406b80
    virtual void DrawOverlay(HDC dc) override;                                                   // slot 14, 0x408bc0

    // Sets up the background picture and its palette.
    void CreateBackdrop();  // 0x4070e0
    void CreateControls();  // 0x407180
    // Loads the game of the selected slot (after the sessions are torn down).
    void LoadSelectedSlot();  // 0x407f60
    // Saves into the selected slot under the name typed into its field.
    void SaveSelectedSlot();  // 0x408000
    // Reports a save or load request of `kind` (1 save, 2 load) of `slot` to the other side of a network game.
    void SendSaveCommand(int kind, int slot, String name);  // 0x4080c0
    // Shows the dialog of one of the 0x474-0x478 messages.
    void ShowResultDialog(uint32_t message);  // 0x408170
    // Result of a save: tells the player and enables the slot's load button.
    void OnSaveResult(int saved, int slot);  // 0x408240
    // One of the option toggle buttons (0x99 music, 0xa1-0xa6 the tactical display options) changed.
    void OnOptionToggled(uint32_t buttonId, int unused);  // 0x4082b0
    void CreateLabels();  // 0x4084b0
    void DeleteLabels();  // 0x408db0
    // Enables or disables the save and load button of every slot from the state of its save file.
    void UpdateSlotButtons();  // 0x408f10

    int m_state;                        // +0x9c application state the window was created for
    uint32_t m_options;                 // +0xa0 display options (bit 0x100: music; the other bits the tactical toggles)
    int m_selectedSlot;                 // +0xa4
    int m_battleFlowState;              // +0xa8 0 without a game; 0x14-0x17 while in a tactical battle
    int m_slotLoadable[6];              // +0xac the load button of slot n + 1 is enabled
    CoolStringField* m_nameFields[6];   // +0xc4 name of the saved game of each slot
    CoolSlider* m_musicSlider;          // +0xdc child 0x8d
    CoolSlider* m_volumeSlider;         // +0xe0 child 0x8e
    uint32_t m_unknownE4;               // +0xe4
    CoolLabel* m_musicTitle;            // +0xe8
    CoolLabel* m_optionTitles[5];       // +0xec the five tactical display options
    CoolLabel* m_musicValue;            // +0x100
    CoolLabel* m_optionValues[5];       // +0x104 "On" / "Off" next to each option
    CoolLabel* m_versionLabel;          // +0x118
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(GameOptionsWindow, 0x11c);
REB_ASSERT_OFFSET(GameOptionsWindow, m_state, 0x9c);
REB_ASSERT_OFFSET(GameOptionsWindow, m_slotLoadable, 0xac);
REB_ASSERT_OFFSET(GameOptionsWindow, m_nameFields, 0xc4);
REB_ASSERT_OFFSET(GameOptionsWindow, m_musicSlider, 0xdc);
REB_ASSERT_OFFSET(GameOptionsWindow, m_musicTitle, 0xe8);
REB_ASSERT_OFFSET(GameOptionsWindow, m_musicValue, 0x100);
REB_ASSERT_OFFSET(GameOptionsWindow, m_versionLabel, 0x118);
#pragma clang diagnostic pop
