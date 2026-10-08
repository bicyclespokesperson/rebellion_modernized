#pragma once
// Functions and objects of the tactical engine that live outside the battle-flow / facade / TacticalGame / object-manager
// modules (ship groups and orders 0x59cd60, capital ship 0x5af4e0, fighter 0x5b8540, Death Star 0x5ba3a0, turn sync
// 0x5bbf10, 3D view 0x5c14d0). They are declared here by their original addresses; the owning module names them and
// drops the declarations from here. Everything is called in the original image (the link step resolves FUN_ names).
#include <cstdint>

#include "core/game_string.h"
#include "tactical/tactical_types.h"
#include "tactical/tactical_units.h"

class TacticalObjectManager;
class TacticalHub;
class TacticalRegistry {
public:
    virtual ~TacticalRegistry();
};
struct HeadlessSimulator;
class TacticalCapitalShip;
class TacticalFighter;

// ---- Ship-group module: the hub (DAT_006bc468, 0x188 bytes) and the registry (DAT_006bc470, 0x48 bytes) ----
TacticalHub* FUN_0059ed70();                                    // creates the hub on first use
void FUN_0059ede0();                                            // destroys the hub
void __thiscall FUN_0059eec0(TacticalHub* hub, int a);
void __thiscall FUN_0059eef0(TacticalHub* hub, int a);
int __thiscall FUN_0059f6e0(TacticalHub* hub);
void __thiscall FUN_005a0a50(TacticalHub* hub, int kind, int arg);
void __thiscall FUN_005a0ad0(TacticalHub* hub, int arg);
void __thiscall FUN_005a0b50(TacticalHub* hub, int a, int b);
void __thiscall FUN_005a0bf0(TacticalHub* hub, uint32_t* out);
void __thiscall FUN_005a0c20(TacticalHub* hub, uint32_t* out);
void __thiscall FUN_005a5fd0(TacticalHub* hub, int arg);
void FUN_005a0240();

TacticalRegistry* __thiscall FUN_005a6290(void* memory);                          // registry constructor
TacticalUnit* __thiscall FUN_005a6340(TacticalRegistry* registry, int handle);   // capital ship or fighter by handle
TacticalUnit* __thiscall FUN_005a6360(TacticalRegistry* registry, int handle);   // a carrier's launch interface
TacticalUnit* __thiscall FUN_005a6480(TacticalRegistry* registry, int handle);
TacticalUnit* __thiscall FUN_005a66a0(TacticalRegistry* registry);               // the Death Star, if any

// ---- Capital ship / fighter / Death Star modules ----
TacticalCapitalShip* __thiscall FUN_005b05c0(void* memory, unsigned family, unsigned id, int notAlliance, uint32_t typeIndex,
                                             String name, float hull, float currentHull, float damageControl,
                                             int freeHangar, float tractorPower, float tractorRange, int gravityWell,
                                             float interdiction, float field64, float field68, float field60,
                                             float hyperdriveCount, float weaponRecharge,
                                             float shieldRecharge, int grid02, int grid12, int grid22, int grid32,
                                             int grid01, int grid11, int grid21, int grid31, int grid00, int grid10,
                                             int grid20, int grid30, int speedCH, int shieldCH, int weaponCH,
                                             int hyperdriveCH, int tractorCH, float a42, float a41, float a40, int hundred);
TacticalFighter* __thiscall FUN_005b9c60(void* memory, unsigned family, unsigned id, int notAlliance, uint32_t typeIndex,
                                         String name, int carrierHandle, float field64, float field68, float field60,
                                         int squadSize, int currentSize, int zero, float hyperdriveCount, int grid02,
                                         int grid12, int grid22, int grid32, int grid01, int grid11, int grid21,
                                         int grid31, int grid00, int grid10, int grid20, int grid30, float a42,
                                         float a41, float a40, int totalTorpedoes, float torpedoRange);
CapitalShipResult* __thiscall FUN_005b0280(TacticalUnit* ship, CapitalShipResult* out);
FightSquadResult* __thiscall FUN_005b8e00(TacticalUnit* fighter, FightSquadResult* out);
DeathStarResult* __thiscall FUN_005ba620(TacticalUnit* deathStar, DeathStarResult* out);
CapitalShipResult* __thiscall FUN_005ba670(TacticalUnit* deathStar, CapitalShipResult* out);
int __thiscall FUN_005b1b70(TacticalUnit* unit);
void __thiscall FUN_005b69b0(TacticalUnit* unit);
void __thiscall FUN_005b69d0(TacticalUnit* unit);
int __thiscall FUN_005b8570(TacticalUnit* carrier, TacticalUnit* fighter);
void FUN_005ba920();

// ---- Headless simulator (DAT_006bc4a8, 0x274 bytes) ----
HeadlessSimulator* __thiscall FUN_005af5f0(void* memory);                     // constructor
void __thiscall FUN_005afa50(HeadlessSimulator* simulator);                    // destructor body
int __thiscall FUN_005ae9c0(HeadlessSimulator* simulator, float step);         // one step, 1 when the battle is over
void __thiscall FUN_005ae460(HeadlessSimulator* simulator, int a);
void __thiscall FUN_005afb30(HeadlessSimulator* simulator, int side, int value);
void __thiscall FUN_005afb50(HeadlessSimulator* simulator, int side, float value);
void __thiscall FUN_005afd40(HeadlessSimulator* simulator, TacticalUnit* unit);
void __thiscall FUN_005afd70(HeadlessSimulator* simulator, TacticalUnit* unit);
void __thiscall FUN_005afdb0(HeadlessSimulator* simulator, TacticalUnit* unit);
int __thiscall FUN_005af5c0(HeadlessSimulator* simulator, TacticalUnit* ship);
int __thiscall FUN_005af440(HeadlessSimulator* simulator, TacticalUnit* unit);
int __thiscall FUN_005af490(HeadlessSimulator* simulator, TacticalUnit* unit);
int __thiscall FUN_005af4b0(HeadlessSimulator* simulator, TacticalUnit* fighter);
int __thiscall FUN_005af4e0(HeadlessSimulator* simulator, int family, int id, int notCompleted, int hasFacility, float hull,
                            float hullNow, int hangarFree);

// ---- Turn sync / 3D view ----
void FUN_005be0f0();
void FUN_005bf320();
void FUN_005c1020(int a);
void FUN_005bbee0();
void* FUN_005bbe70(int handle);
void __thiscall FUN_005bbd90(void* syncObject, int handle);
void __thiscall FUN_005ae1e0(void* self, int argument);  // shared empty stub (pops one argument)
void FUN_005c1470(float seconds);
int FUN_005c1490();

// The 3D view's client object (DAT_006bc464); only its second virtual is called from here.
class TacticalClient {
public:
    virtual void Virtual00();
    virtual void Virtual01();
};
extern TacticalClient* g_tacticalClient;                // 0x6bc464
extern TacticalObjectManager* g_tacticalObjectManager;  // 0x6bc4a0
extern TacticalHub* g_tacticalHub;                      // 0x6bc468
extern TacticalRegistry* g_tacticalRegistry;            // 0x6bc470
extern HeadlessSimulator* g_headlessSimulator;          // 0x6bc4a8
