#pragma once
class MessageBase;
// The game manager's side of the object model's hook table (0x4fcd90-0x4fd280). The installer (0x4fcde0) fills the
// callbacks declared in gameobj/game_hooks.h with the wrappers below; each one forwards to the game manager (0x6b2fcc)
// and does nothing, or returns its default, while there is none. The remaining wrappers are plain accessors that
// other code calls directly.
#include <cstdint>

#include "app/player_controller_decl.h"
#include "core/stream_io.h"
#include "events/game_manager.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_settings.h"

// Two more slots of the hook table that only the installer fills (0x6b2adc / 0x6b2ae4).
extern int (*g_gameHook2ADC)(HookArg);
extern int (*g_gameHook2AE4)(HookArg);

// 4-byte float values the game manager's clients keep (0x4fcd90, 0x4fcdb0, 0x4fcdd0).
void AssignGameTime(GameTime* self, const GameTime* other); // 0x4fcd90
void ReadGameTime(GameTime* self, istream* in); // 0x4fcdb0
void ClearGameTime(GameTime* self); // 0x4fcdd0

void InstallGameManagerHooks(); // 0x4fcde0

// ---- the installed hooks ----
int GameHookTickCount(); // 0x4fcee0 (slot 0x6b2b04)
int GameHookDay(); // 0x4fcf00 (slot 0x6b2b08)
void GameHookScheduleTimer(HookArg timer, int delay); // 0x4fcfd0 (slot 0x6b2ad8)
void GameHookScheduleCommand(HookArg event); // 0x4fcff0 (slot 0x6b2b00)
void GameHookScheduleEvent(HookArg event); // 0x4fd010 (slot 0x6b2af8)
int GameHookDispatchPostedEvent(HookArg event); // 0x4fd030 (slot 0x6b2ad4)
int GameHookIsSetupDone(); // 0x4fd060 (slot 0x6b2af0)
int GameHookNoteDispatch(HookArg item); // 0x4fd0d0 (slots 0x6b2adc, 0x6b2ae4)
int GameHookNextSerialA(); // 0x4fd240 (slot 0x6b2af4)
int GameHookNextSerialB(); // 0x4fd260 (slot 0x6b2afc)

// ---- accessors ----
int GetGameSettings(GameSettings* out); // 0x4fce50, always 1
void BuildAvailableOrdersViaManager(int side, ObjectKeyValueSet* team, EventKindSet* orders); // 0x4fcf20
int QueryBattleState(int side, uint32_t* a, uint32_t* b, int* c, uint32_t* d); // 0x4fcf50
void CopyCurrentSnapshot(GameStateSnapshot* out); // 0x4fd080
int SetSideSnapshot(GameStateSnapshot* snapshot, int which); // 0x4fd0a0, 1 without a manager
int GetManagerStateDiffers(); // 0x4fd100
int GetManagerUnk34(); // 0x4fd120
void SetManagerUnk34(int value); // 0x4fd140
int GetManagerUnk38(); // 0x4fd160
int GetManagerUnk3c(); // 0x4fd180
void SetManagerUnk3c(int value); // 0x4fd1a0
int GetManagerUnk40(); // 0x4fd1c0
int GetManagerUnk44(); // 0x4fd1e0
void SetManagerUnk44(int value); // 0x4fd200
int AddManagerNotice(MessageBase* notice); // 0x4fd220, always 1
