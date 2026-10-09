#pragma once
#include "core/reb_abi.h"
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
void REB_CDECL TacticalBeginBattle(int humanIsNotAlliance, int opponentIsAi, uint32_t systemField44, String systemName,
                                 float sideAValue, float sideBValue, int sideAC, int sideBC, GameTime timeA,
                                 GameTime timeB, int battleKindNotAlliance, int systemFlag8, int controlIsEmpire,
                                 int controlIsAlliance, int abstractFlag);  // 0x595d60

// Registers a capital ship; the 42 words after the name are the ship's figures as read from the strategic object (hull,
// weapons, damage grid, critical hits ...).
int REB_CDECL TacticalRegisterCapitalShip(
    uint32_t family, uint32_t id, int notAlliance, uint32_t typeBits, String name, float hull, float currentHull,
    float damageControl, int freeHangar, float tractorPower, float tractorRange, int gravityWell, float interdiction,
    float field64, float field68, float field60, float hyperdriveCount, float gridTotal, float weaponRecharge,
    float shieldRecharge, int grid02, int grid12, int grid22, int grid32, int grid01, int grid11, int grid21,
    int grid31, int grid00, int grid10, int grid20, int grid30, int speedCH, int shieldCH, int weaponCH,
    int hyperdriveCH, int tractorCH, float a42, float a41, float a40);  // 0x595f60

int REB_CDECL TacticalRegisterDeathStar(uint32_t family, uint32_t id, int notCompleted, int hasFacility, float hull,
                                      float currentHull, int hangarFree);  // 0x595ef0

int REB_CDECL TacticalRegisterFightSquad(
    uint32_t family, uint32_t id, int notAlliance, uint32_t typeBits, String name, int carrierHandle,
    int carrierIsCapitalShip, float field64, float field68, float field60, int squadSize, int currentSize, int zero,
    float hyperdriveCount, int grid02, int grid12, int grid22, int grid32, int grid01, int grid11, int grid21,
    int grid31, int grid00, int grid10, int grid20, int grid30, float a42, float a41, float a40, int totalTorpedoes,
    float torpedoRange);  // 0x5964c0

int REB_CDECL TacticalRegisterCharacter(uint32_t family, uint32_t id, String name, int notAlliance, int isMajor,
                                      int isFirstCommander, int isSecondCommander, float scaleA,
                                      float scaleB, int shipHandle, int injured,
                                      uint32_t typeBits);  // 0x596340

// Special forces and troop regiments.
int REB_CDECL TacticalRegisterSpecialForce(uint32_t family, uint32_t id, uint32_t typeBits, String name, int notAlliance,
                                         int shipHandle);  // 0x5961e0

// Starts the registered battle (switches the application into the tactical states).
void TacticalStart();  // 0x596820

// Results, read back after the battle (BattleResults, app/battle_flow.h). Each returns `out`.
CapitalShipResult* REB_CDECL TacticalGetCapitalShipResult(CapitalShipResult* out, int handle);  // 0x596870
FightSquadResult* REB_CDECL TacticalGetFightSquadResult(FightSquadResult* out, int handle);     // 0x5968e0
CharacterResult* REB_CDECL TacticalMakeCharacterResult(CharacterResult* out, int handle);       // 0x596930
DeathStarResult* REB_CDECL TacticalGetDeathStarResult(DeathStarResult* out);                    // 0x596980
BattleOutcome* REB_CDECL TacticalGetBattleOutcome(BattleOutcome* out, int unused);              // 0x5969d0

// The registry (DAT_006bc470), created on first use.
TacticalRegistry* GetTacticalRegistry();  // 0x596bd0

// ---- small forwarders to TacticalGame, the hub, the object manager and the registry ----
void REB_CDECL TacticalPostNotice(int a, int unused);          // 0x595b10
void REB_CDECL TacticalHubSync(int a);                         // 0x595b30
int TacticalHasDeathStar();                                  // 0x595b50
void REB_CDECL TacticalPostPair(int a, int b);                 // 0x595b70
int TacticalHubIsRunning();                                  // 0x595b90
void REB_CDECL TacticalCopyPlayerFlags8(uint32_t* out);        // 0x595ba0
void REB_CDECL TacticalCopyPlayerFlags4(uint32_t* out);        // 0x595bc0
void TacticalCallClientSlot1();                              // 0x595be0
int TacticalReturnMinusOne();                                // 0x595bf0
void REB_CDECL TacticalHubSelectA(int a);                      // 0x595c00
void REB_CDECL TacticalHubSelectB(int a);                      // 0x595c20
int TacticalGetPlayerSide();                                 // 0x595c40
int TacticalPlayerSideIsActive();                            // 0x595c50
int TacticalModeIsSix();                                     // 0x595c60
void REB_CDECL TacticalPostMessage1(int arg);                  // 0x595c80
void REB_CDECL TacticalPostMessage0(int arg);                  // 0x595ca0
void REB_CDECL TacticalPostMessage3IfSet(int arg);             // 0x595cc0
void REB_CDECL TacticalPostMessage2IfSet(int arg);             // 0x595ce0
void REB_CDECL TacticalForwardEvent(int a, int b, int c);      // 0x595d00
void REB_CDECL TacticalCopyManagerBlock(uint32_t* out, int index);  // 0x595d20: 4 dwords of the object manager at +0x960 + index * 16
void InitTacticalFacadeScale();                              // 0x595ed0
int REB_CDECL TacticalCanCommandShip(void* ship);              // 0x596830
int REB_CDECL TacticalCanCommandFighter(void* fighter);        // 0x596850
void TacticalPause();                                        // 0x596a10
void TacticalResume();                                       // 0x596a30
int TacticalIsPaused();                                      // 0x596a50
void TacticalPostNoticeIfPlayerSideCanAct();                 // 0x596a70
int TacticalPlayerSideCanAct();                              // 0x596a90
void TacticalCheckAnySideFlag();                             // 0x596ab0
void TacticalHubCheckSides();                                // 0x596ac0
int16_t TacticalGetBackdropId();                             // 0x596ad0
// The tactical stats of a unit (0 for none): through the registry entry named by the ship's +0x178, or directly.
uint32_t REB_CDECL TacticalGetCarrierStat68(void* ship);       // 0x596af0
uint32_t REB_CDECL TacticalGetUnitStat68(void* unit);          // 0x596b20
uint32_t REB_CDECL TacticalGetCarrierStat38(void* ship);       // 0x596b30
uint32_t REB_CDECL TacticalGetUnitStat38(void* unit);          // 0x596b60
void REB_CDECL TacticalNotifySync(int handle);                 // 0x596b70
void REB_CDECL TacticalNotifySyncQuiet(int handle);            // 0x596b90
int TacticalWorldCheck();                                    // 0x596bb0
void TacticalObjectManagerSlot7();                           // 0x596bc0
