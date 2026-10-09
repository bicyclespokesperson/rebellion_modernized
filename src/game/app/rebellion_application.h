#pragma once
#include <cstdint>

#include "app/saved_game_info_decl.h"
#include "core/game_string.h"
#include "core/layout.h"
#include "core/stopwatch.h"
#include "core/string_map.h"
#include "core/utility_classes.h"
#include "platform/win32.h"
#include "ui/cool_app.h"

class AppCommand;
class StrategicWindow;
class TacticalWindow;

// What the front-end state machine (RebellionApplication::SetState) is showing. The values are the original
// state numbers; 6 and 15 are accepted but reset to 0 immediately.
enum AppState : int {
    kStateNone = 0,
    kStateIntroMovie = 1,       // first introduction movie (movie 0x65)
    kStateIntroMovie2 = 2,      // second introduction movie (movie 0x66)
    kStateMainMenu = 3,         // the front-end window
    kStateLobbyHost = 4,        // multiplayer lobby window
    kStateLobbyJoin = 5,        // multiplayer lobby window (other role)
    kStateNewGameMovie = 7,     // movie before a new game starts, then the game is created
    kStateStrategic = 8,        // the strategic game window
    kStateTactical = 9,         // the tactical window
    kStateBattleSwitch = 10,    // switch between strategic and tactical, driven by the battle-flow state
    kStateOptionsMenu = 11,     // options window opened from the main menu
    kStateOptionsInGame = 12,   // options window opened during a game
    kStateConfirmQuit = 13,     // the quit confirmation is up over the previous state
    kStateQuit = 14,            // shut down and post WM_QUIT
    kStateVictoryMovieEmpire = 16,
    kStateVictoryMovieAlliance = 17,
    kStateDefeatMovieEmpire = 18,
    kStateDefeatMovieAlliance = 19,
};

// Bits of RebellionApplication::m_commandLineFlags.
enum AppCommandLineFlags : uint32_t {
    kAppWindowed = 0x001,          // -W: caption and frame, no display mode switch
    kAppNoAutosave = 0x010,        // -N (WinMain always sets it)
    kAppFlag100 = 0x100,           // -L
};

// Custom messages the shell posts to itself and to its active window.
enum AppMessage : uint32_t {
    kMsgRequestFinished = 0x402,   // posted to the active window with the id of the movie / state that ended
    kMsgChildDone = 0x405,         // wParam 0xd: close the confirmation dialog, 0xf: a child window finished (lParam = id)
    kMsgMovieDone = 0x465,
    kMsgDestroyWindow = 0x469,     // lParam = window object: DestroyWindow + delete
    kMsgDialogAnswer = 0x46a,      // wParam = dialog message id
    kMsgDialogEnded = 0x46b,
    kMsgDeactivated = 0x479,       // sent to the active window when the application is deactivated
    kMsgActivated = 0x47a,
    kMsgAccelerator = 0x483,       // WM_COMMAND 3001..3031 turned into a message for the active window
};

// The application object: the single top-level window ("STARWARS_REBELLION", 640x480, WS_POPUP) and the owner of
// the screens (front-end, strategic, tactical). WinMain builds it and runs its message loop.
class RebellionApplication : public CoolApp {
public:
    RebellionApplication(HINSTANCE instance, uint32_t commandLineFlags, const String* commandLine);  // 0x413000
    virtual ~RebellionApplication();  // 0x413440 (scalar deleting destructor 0x413420)

    // Slot 5: the window procedure.
    virtual LRESULT __stdcall WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // 0x4138a0

    // The main loop: creates and shows the window, then pumps messages and steps the game while the queue is empty.
    // Returns WM_QUIT's wParam. `unused` is always 1.
    WPARAM Run(int showCommand, int unused);  // 0x413560

    // Records how the game that is about to start was configured (WinMain path of a new / loaded / network game).
    void SetGameParams(int side, int gameType, int param8, int paramF0, int connectionKind, int networkMode);  // 0x4142d0
    // Leaves the current state and enters `state` (an AppState).
    void SetState(int state);  // 0x414330
    // Plays the movie `movieId` (0x65..0x6d) in the movie window; `a`/`b` go to the movie window.
    void PlayMovie(int movieId, const String* caption, void(__cdecl* callback)(uint32_t, uint32_t, int));  // 0x4146e0
    void BeginMovieState();   // 0x414830
    void DoMainChildWindow(int previousState);  // 0x414a80: front-end, lobby or options window for the current state
    void ShowStrategicWindow(int unused);       // 0x414eb0
    void DestroyStrategicWindow();              // 0x4150d0
    void ShowTacticalWindow();                  // 0x415120
    void DestroyTacticalWindow();               // 0x415300
    void DestroyAllWindows();                   // 0x415350 (0x415345 is a data-table entry pointing at padding)
    void ForwardToStrategicWindow();            // 0x415420: if a strategic window shows a modal child, run its handler
    int LoadPendingGame();                      // 0x415440
    int SavePendingGame();                      // 0x415600
    SavedGameInfo ReadSaveHeader(int slot, int isNetworkGame);   // 0x415cd0
    String GetSaveName(int slot, int isNetworkGame);             // 0x415a00
    String MakeSaveFileName(int slot, int isNetworkGame);        // 0x415ad0
    int GetSavedGameType(int slot, int isNetworkGame);                               // 0x415df0
    // Sends a save request to the other players (the args are those of the command at 0x4210c0).
    void SendSaveCommand(int a, int b, String name, int d);  // 0x4158e0
    int HasWaitedLongEnough();                  // 0x415e20: true once 0.25 s passed since ResetWaitTimer
    void PromptForPause();                      // 0x415e60
    void OnDialogEndedEnableSpeed();            // 0x415ec0
    void ShowResumeNotice();                    // 0x415f40
    void PlayClickSound();                      // 0x415eb0 (forwards to the sound module)
    int SaveStrategicState(void* stream);       // 0x415f60
    int LoadStrategicState(void* stream);       // 0x415f80
    int SaveTacticalState(void* stream);        // 0x4160a0
    int LoadTacticalState(void* stream);        // 0x4160c0
    void ProcessPendingSaveLoad();              // 0x4161b0
    int IsSessionActive();                      // 0x416270: a load or save is pending, or a game is running
    BOOL ShowHelp();                            // 0x4162a0
    void SetVolume(int volume);                 // 0x416360
    void SetDisplayOptions(uint32_t options);   // 0x4163a0
    void SetIdentityPaletteForDisplay();        // 0x4163d0
    StringMap* CopyNetworkSettings(StringMap* out);  // 0x401b20

    // Pieces of Run and WindowProc (the original has them inline).
    void PumpMessages(MSG* message, int* quit);
    void PollMultiplayerSync(int quit);
    void ReportOpponentTrouble();
    LRESULT OnCreate();
    LRESULT OnActivateApp(WPARAM wParam, LPARAM lParam);
    void OnDeactivated();
    void OnActivated();
    void EnterFrontEndAfterFirstActivation();
    LRESULT OnQueryNewPalette(WPARAM wParam, LPARAM lParam);
    LRESULT OnChildDone(UINT message, WPARAM wParam, LPARAM lParam);
    void ReturnToActiveWindowAfterChild(LPARAM childId);
    LRESULT OnDialogAnswer(WPARAM wParam);
    LRESULT OnSaveAnswer();

    // Blocks the original repeated inline: destroy the overlay, detach the active window, blank the window.
    void ReleaseOverlayWindow();
    void DetachActiveWindow();
    void FillWindowBlack();

    PersistentInt m_activationFlags;  // +0xb4 bit 0x10000000: the application is deactivated
    CoolWindow* m_strategicWindow;    // +0xb8
    CoolWindow* m_tacticalWindow;     // +0xbc
    CoolWindow* m_activeWindow;       // +0xc0 the screen that has the focus (one of the others or the front-end)
    CoolWindow* m_overlayWindow;      // +0xc4 movie window or confirmation dialog
    int m_pendingLoadSlot;            // +0xc8 save slot to load at the next opportunity, 0: none
    int m_pendingSaveSlot;            // +0xcc save slot to write, 0: none
    String m_pendingSaveName;         // +0xd0 name for the pending save
    int m_state;                      // +0xdc AppState
    int m_previousState;              // +0xe0
    int m_gameType;                   // +0xe4 1: single player, 2: network game
    int m_param8;                     // +0xe8
    int m_param10;                    // +0xec
    int m_paramF0;                    // +0xf0
    int m_syncState;                  // +0xf4 result of the multiplayer sync poll (2: all players ready)
    int m_peersReady;                 // +0xf8
    int m_connected;                  // +0xfc
    int m_pauseRequested;             // +0x100 a pause dialog is up
    int m_networkMode;                // +0x104 7: network game
    int m_side;                       // +0x108 1: Alliance, otherwise Empire
    int m_waitElapsed;                // +0x10c set once the stopwatch passed 0.25 s
    Stopwatch m_waitTimer;            // +0x110
    uint32_t m_commandLineFlags;      // +0x128 AppCommandLineFlags
    int m_volume;                     // +0x12c negated registry "Volume"
    int m_unknown130;                 // +0x130
    int m_unknown134;                 // +0x134
    int m_unknown138;                 // +0x138
    int m_autosaveTick;               // +0x13c
    uint32_t m_displayOptions;        // +0x140 registry "DisplayOpts"
    uint32_t m_displayOptions2;       // +0x144 0x13110000
    StringMap m_networkSettings;      // +0x148 latency / drop / dupe tuning, loaded from the command line
    int m_mismatchOptions;            // +0x154 registry "MismatchOpts"
    int m_unknown158;                 // +0x158
    int m_unknown15c;                 // +0x15c
};

// ---- Application-level helpers (0x401000-0x401c70): they act on g_app, doing nothing without one -----------------

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(RebellionApplication, 0x160);
REB_ASSERT_OFFSET(RebellionApplication, m_activationFlags, 0xb4);
REB_ASSERT_OFFSET(RebellionApplication, m_strategicWindow, 0xb8);
REB_ASSERT_OFFSET(RebellionApplication, m_overlayWindow, 0xc4);
REB_ASSERT_OFFSET(RebellionApplication, m_pendingSaveName, 0xd0);
REB_ASSERT_OFFSET(RebellionApplication, m_state, 0xdc);
REB_ASSERT_OFFSET(RebellionApplication, m_gameType, 0xe4);
REB_ASSERT_OFFSET(RebellionApplication, m_networkMode, 0x104);
REB_ASSERT_OFFSET(RebellionApplication, m_waitTimer, 0x110);
REB_ASSERT_OFFSET(RebellionApplication, m_commandLineFlags, 0x128);
REB_ASSERT_OFFSET(RebellionApplication, m_networkSettings, 0x148);
REB_ASSERT_OFFSET(RebellionApplication, m_mismatchOptions, 0x154);
#pragma clang diagnostic pop

int __stdcall WinMain(HINSTANCE instance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand);  // 0x4165b0
void ChangeToInstalledLocation();   // 0x416820
int InstalledLocationHasBigData();  // 0x416950
