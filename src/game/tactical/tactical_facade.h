#pragma once
#include <cstdint>

#include "app/player_controller_decl.h"
#include "core/game_string.h"
#include "tactical/tactical_types.h"

// The tactical engine's facade (0x595b10-0x596fa0): the only entry points the strategic game uses. The battle flow
// (app/battle_flow.h) registers the units of a battle through the Register* functions (each returns the tactical handle
// the unit is known by, 0 on failure), starts the engine, and later reads the results back.
//
// In an abstract (auto-resolve) battle the units go to the headless simulator, in a 3D battle to the object manager.

class TacticalObjectManager;
class TacticalRegistry;
class TacticalUnit;
struct HeadlessSimulator;

// Begins a battle: sets up TacticalGame (tactical_game.h). `battleKindNotAlliance`.. are the system's control details; the
// two floats are scaled by 0.4 before use.
void __cdecl TacticalBeginBattle(int humanIsNotAlliance, int opponentIsAi, uint32_t systemField44, String systemName,
                                 float sideAValue, float sideBValue, int sideAC, int sideBC, GameTime timeA,
                                 GameTime timeB, int battleKindNotAlliance, int systemFlag8, int controlIsEmpire,
                                 int controlIsAlliance, int abstractFlag);  // 0x595d60

// Registers a capital ship; the 42 words after the name are the ship's figures as read from the strategic object (hull,
// weapons, damage grid, critical hits ...).
int __cdecl TacticalRegisterCapitalShip(
    uint32_t family, uint32_t id, int notAlliance, uint32_t typeBits, String name, float hull, float currentHull,
    float damageControl, int freeHangar, float tractorPower, float tractorRange, int gravityWell, float interdiction,
    float field64, float field68, float field60, float hyperdriveCount, float gridTotal, float weaponRecharge,
    float shieldRecharge, int grid02, int grid12, int grid22, int grid32, int grid01, int grid11, int grid21,
    int grid31, int grid00, int grid10, int grid20, int grid30, int speedCH, int shieldCH, int weaponCH,
    int hyperdriveCH, int tractorCH, float a42, float a41, float a40);  // 0x595f60

int __cdecl TacticalRegisterDeathStar(uint32_t family, uint32_t id, int notCompleted, int hasFacility, float hull,
                                      float currentHull, int hangarFree);  // 0x595ef0

int __cdecl TacticalRegisterFightSquad(
    uint32_t family, uint32_t id, int notAlliance, uint32_t typeBits, String name, int carrierHandle,
    int carrierIsCapitalShip, float field64, float field68, float field60, int squadSize, int currentSize, int zero,
    float hyperdriveCount, int grid02, int grid12, int grid22, int grid32, int grid01, int grid11, int grid21,
    int grid31, int grid00, int grid10, int grid20, int grid30, float a42, float a41, float a40, int totalTorpedoes,
    float torpedoRange);  // 0x5964c0

int __cdecl TacticalRegisterCharacter(uint32_t family, uint32_t id, String name, int notAlliance, int isMajor,
                                      int isFirstCommander, int isSecondCommander, float scaleA,
                                      float scaleB, int shipHandle, int injured,
                                      uint32_t typeBits);  // 0x596340

// Special forces and troop regiments.
int __cdecl TacticalRegisterSpecialForce(uint32_t family, uint32_t id, uint32_t typeBits, String name, int notAlliance,
                                         int shipHandle);  // 0x5961e0

// Starts the registered battle (switches the application into the tactical states).
void TacticalStart();  // 0x596820

// Results, read back after the battle (BattleResults, app/battle_flow.h). Each returns `out`.
CapitalShipResult* __cdecl TacticalGetCapitalShipResult(CapitalShipResult* out, int handle);  // 0x596870
FightSquadResult* __cdecl TacticalGetFightSquadResult(FightSquadResult* out, int handle);     // 0x5968e0
CharacterResult* __cdecl TacticalMakeCharacterResult(CharacterResult* out, int handle);       // 0x596930
DeathStarResult* __cdecl TacticalGetDeathStarResult(DeathStarResult* out);                    // 0x596980
BattleOutcome* __cdecl TacticalGetBattleOutcome(BattleOutcome* out, int unused);              // 0x5969d0

// The registry (DAT_006bc470), created on first use.
TacticalRegistry* GetTacticalRegistry();  // 0x596bd0

// ---- small forwarders to TacticalGame, the hub, the object manager and the registry ----
void __cdecl TacticalPostNotice(int a, int unused);          // 0x595b10
void __cdecl TacticalHubSync(int a);                         // 0x595b30
int TacticalHasDeathStar();                                  // 0x595b50
void __cdecl TacticalPostPair(int a, int b);                 // 0x595b70
int TacticalHubIsRunning();                                  // 0x595b90
void __cdecl TacticalCopyPlayerFlags8(uint32_t* out);        // 0x595ba0
void __cdecl TacticalCopyPlayerFlags4(uint32_t* out);        // 0x595bc0
void TacticalCallClientSlot1();                              // 0x595be0
int TacticalReturnMinusOne();                                // 0x595bf0
void __cdecl TacticalHubSelectA(int a);                      // 0x595c00
void __cdecl TacticalHubSelectB(int a);                      // 0x595c20
int TacticalGetPlayerSide();                                 // 0x595c40
int TacticalPlayerSideIsActive();                            // 0x595c50
int TacticalModeIsSix();                                     // 0x595c60
void __cdecl TacticalPostMessage1(int arg);                  // 0x595c80
void __cdecl TacticalPostMessage0(int arg);                  // 0x595ca0
void __cdecl TacticalPostMessage3IfSet(int arg);             // 0x595cc0
void __cdecl TacticalPostMessage2IfSet(int arg);             // 0x595ce0
void __cdecl TacticalForwardEvent(int a, int b, int c);      // 0x595d00
void __cdecl TacticalCopyManagerBlock(uint32_t* out, int index);  // 0x595d20: 4 dwords of the object manager at +0x960 + index * 16
void InitTacticalFacadeScale();                              // 0x595ed0
int __cdecl TacticalCanCommandShip(void* ship);              // 0x596830
int __cdecl TacticalCanCommandFighter(void* fighter);        // 0x596850
void TacticalPause();                                        // 0x596a10
void TacticalResume();                                       // 0x596a30
int TacticalIsPaused();                                      // 0x596a50
void TacticalPostNoticeIfPlayerSideCanAct();                 // 0x596a70
int TacticalPlayerSideCanAct();                              // 0x596a90
void TacticalCheckAnySideFlag();                             // 0x596ab0
void TacticalHubCheckSides();                                // 0x596ac0
int16_t TacticalGetBackdropId();                             // 0x596ad0
// The tactical stats of a unit (0 for none): through the registry entry named by the ship's +0x178, or directly.
uint32_t __cdecl TacticalGetCarrierStat68(void* ship);       // 0x596af0
uint32_t __cdecl TacticalGetUnitStat68(void* unit);          // 0x596b20
uint32_t __cdecl TacticalGetCarrierStat38(void* ship);       // 0x596b30
uint32_t __cdecl TacticalGetUnitStat38(void* unit);          // 0x596b60
void __cdecl TacticalNotifySync(int handle);                 // 0x596b70
void __cdecl TacticalNotifySyncQuiet(int handle);            // 0x596b90
int TacticalWorldCheck();                                    // 0x596bb0
void TacticalObjectManagerSlot7();                           // 0x596bc0
