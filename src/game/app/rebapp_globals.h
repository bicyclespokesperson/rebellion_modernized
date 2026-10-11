#pragma once
#include <cstdint>

#include "app/saved_game_info_decl.h"
#include "core/game_string.h"
#include "core/string_map.h"
#include "platform/win32.h"

class CoolWindow;

// The application-level helpers at the start of the image (0x401000-0x401c70). Almost all of them forward to
// `g_app` (the RebellionApplication) or to the battle-flow object and quietly do nothing when it does not exist yet,
// so any module can call them without checking.

// ---- Registry locations (HKLM\SOFTWARE\LucasArts Entertainment Company LLC\Star Wars Rebellion) ----------------
// Each returns a heap copy of the value (delete it with operator delete), or null.
char* GetDllLocation();        // 0x401440 "DllLocation"
char* GetDataLocation();       // 0x401560 "DataLocation"
char* GetTooBigLocation();     // 0x401660 "TooBigLocation"
char* GetInstalledLocation();  // 0x401760 "InstalledLocation"

// ---- Application settings ---------------------------------------------------------------------------------
StringMap* GetNetworkSettings(StringMap* out);   // 0x401000: copy of the network tuning values (empty without an app)
int AppGetSide();                                // 0x401060: 1: Alliance
int AppGetGameType();                            // 0x401080: 1: single player, 2: network game
void AppSetGameParams(int side, int gameType, int param8, int param10, int connectionKind, int networkMode);  // 0x4010a0
int AppHasWaitedLongEnough();                    // 0x4010d0
void AppResetWaitTimer();                        // 0x4010f0
int AppGetUnknown138();                          // 0x401960
void AppSetUnknown138(int value);                // 0x401980
uint32_t AppGetDisplayOptions();                 // 0x4019e0: 0x100 without an app
uint32_t AppGetDisplayOptions2();                // 0x401a00: 0x13110000 without an app
void AppSetDisplayOptions(uint32_t options);     // 0x401a20: also stored in the registry
void AppSetDisplayOptions2(uint32_t options);    // 0x401a40
void AppSetVolume(int volume);                   // 0x401a60: also stored in the registry
int AppGetVolume();                              // 0x401a80
uint32_t AppGetCommandLineFlags();               // 0x401ae0
int AppGetNetworkMode();                         // 0x401b00
void AppSetAutosaveTick(int tick);               // 0x401420
int AppGetMusicVolume();                         // 0x401ac0
void AppSetMusicVolume(int volume);              // 0x401aa0 (sound module: also stored in the registry)
void AppSetMusicSwitch(int enabled);             // 0x401ab0 (sound module: music on/off)

// ---- Screens and dialogs ---------------------------------------------------------------------------------
void AppSetState(int state);                     // 0x401040
void AppOpenOptions();                           // 0x4012d0
void AppForwardToStrategicWindow();              // 0x401030
void AppPlayMovie(int movieId, const String* caption, void(* callback)(uint32_t, uint32_t, int));  // 0x4019a0
int AppShowHelp();                               // 0x4019c0
void AppDestroyTacticalWindow();                 // 0x401ad0
void AppPromptPause();                           // 0x401930
void AppShowResumeNotice();                      // 0x401940
void AppPlayClickSound();                        // 0x401950
void AppEnableSpeedAfterDialog();                // 0x401240
// The question / notice / acknowledge dialogs of the app dialog family (app_dialogs).
void AppShowQuestionDialog(int message, int textA, int textB, int x, int y);                // 0x401110
void AppShowNoticeDialog(int message, int a, int textA, int textB, int x, int y);          // 0x401150
void AppShowAckDialog(int a, int textA, int x, int y);                                      // 0x401200
// Posts the dialog outcome to `app` (g_app when null): message 0x46a when accepted, 0x46b when not.
void AppPostDialogResult(int accepted, CoolWindow* app);                                    // 0x4011a0

// ---- Save and load ---------------------------------------------------------------------------------------
int AppSaveStrategicState(void* stream);         // 0x401250
int AppLoadStrategicState(void* stream);         // 0x401270
int AppSaveTacticalState(void* stream);          // 0x401290
int AppLoadTacticalState(void* stream);          // 0x4012b0
void AppProcessSaveLoad();                       // 0x4012f0
int AppIsSaveLoadPending();                      // 0x401300
void AppRequestSave(int slot, String name);      // 0x401320
void AppRequestLoad(int slot);                   // 0x401380
void AppSendSaveCommand(int a, int b, String name, int d);   // 0x4013a0
SavedGameInfo AppReadSaveHeader(int slot, int isNetworkGame);  // 0x401860
int AppGetSavedGameType(int slot, int isNetworkGame);  // 0x401910: without an app returns its second argument

// ---- Battle-flow object (the game session driver, 0x409230-0x40a050) ----------------------------------------
void AppRestoreGame(void* stream);               // 0x401b70
void AppStartGame(void* setup, int gameType, int side);  // 0x401b90
void AppSaveGame(void* stream);                  // 0x401bb0
void AppStepGame(int* stepped);                  // 0x401bd0: sets *stepped = 1, then lets the game advance
void AppReleaseBattleFlow();                     // 0x401bf0
int AppGetBattleFlowState();                     // 0x401c10
int AppIsGameRunning();                          // 0x401c30
void AppSetAutosaveFlag(int value);              // 0x401c50

extern uint32_t g_appDialogMessage;                // 0x6b1470
extern int g_isNetworkGame;                      // 0x6b1208
