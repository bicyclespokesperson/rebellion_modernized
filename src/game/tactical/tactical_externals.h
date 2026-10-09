#pragma once
// Functions and objects of the tactical engine that live outside the battle-flow / facade / TacticalGame / object-manager
// modules (ship groups and orders 0x59cd60, capital ship 0x5af4e0, fighter 0x5b8540, Death Star 0x5ba3a0, turn sync
// 0x5bbf10, 3D view 0x5c14d0). They are declared here by their original addresses; the owning module names them and
// drops the declarations from here. Everything is called in the original image (the link step resolves FUN_ names).
#include <cstdint>

#include "core/game_string.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "tactical/tactical_math.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_types.h"
#include "tactical/tactical_units.h"

class TacticalObjectManager;
class TacticalSideData;
class TacticalHub;
struct HeadlessSimulator;
class TacticalCapitalShip;
class TacticalShipBase;
class TacticalFighter;
class TacticalShipBase;
class TacticalCharacterUnit;
class ShipGroup;
class TacticalSoundManager;

// ---- Ship-group module: the hub (DAT_006bc468, 0x188 bytes) and the registry (DAT_006bc470, 0x48 bytes) ----
TacticalHub* FUN_0059ed70();                                    // creates the hub on first use
void DestroyTacticalHub();                                      // 0x59ede0 destroys the hub
void __thiscall FUN_0059ee10(TacticalHub* hub);                 // the hub's destructor body
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

// ---- Capital ship / fighter / Death Star modules ----
void FUN_005ba920();

// ---- Headless simulator (DAT_006bc4a8, 0x274 bytes) ----

// ---- Turn sync / 3D view ----
void FUN_005be0f0();
int __thiscall FUN_005c7e40(TacticalSideData* side);
void FUN_005c0f80(int paused);
int __thiscall FUN_005ae1d0(TacticalObjectManager* manager, int side);
void __thiscall FUN_0059e840(TacticalHub* hub);
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

// ---- Object manager (3D battle): commander skill, random numbers, control and registration of units ----
double __thiscall FUN_005ad750(TacticalObjectManager* manager, int side);
double __thiscall FUN_005ad7e0(TacticalObjectManager* manager, int side);
// The object manager's random number between two bounds (rand() % (high - low + 1) + low).
int __thiscall FUN_005a8a70(TacticalObjectManager* manager, int low, int high);
// Whether the unit is controlled on this machine (RET 4).
int __thiscall FUN_005ad870(TacticalObjectManager* manager, TacticalShipBase* ship);
void __thiscall FUN_005a90c0(TacticalObjectManager* manager, TacticalShipBase* ship);
void __thiscall FUN_005a9130(TacticalObjectManager* manager, TacticalShipBase* ship);
void __thiscall FUN_005a9070(TacticalObjectManager* manager, TacticalShipBase* ship);
void __thiscall FUN_005a9170(TacticalObjectManager* manager, TacticalShipBase* ship);
// The Death Star the 3D battle knows (the registry's); `manager` is only passed through.
TacticalUnit* __thiscall FUN_005a9330(TacticalObjectManager* manager);
// The character with this handle in the object manager's tree (RET 4).
TacticalCharacterUnit* __thiscall FUN_005a9430(TacticalObjectManager* manager, int handle);
TacticalVector* __thiscall FUN_005a7490(TacticalObjectManager* manager, TacticalVector* out, int side);
// Builds a queued event object (0x220 bytes) in `memory`: time, kind and three arguments.
void* __thiscall FUN_005ce500(void* memory, float time, int kind, int a, int b, int c);

// ---- Ship groups and the hub ----
int __thiscall FUN_005cb980(ShipGroup* group);
int __thiscall FUN_005c7150(ShipGroup* group);
// Removes a member from its group and unregisters the group when it is empty (cdecl).
void FUN_005caef0(ShipGroup* group, int handle);
void __thiscall FUN_0059f4d0(TacticalHub* hub, TacticalShipBase* ship);
int __thiscall FUN_0059f640(TacticalHub* hub, TacticalShipBase* ship);
void __thiscall FUN_005a5cf0(TacticalHub* hub, int attacker, int target);
extern float g_shipMinTurnStep;  // 0x6bc4b0

// ---- 3D presentation hooks (effects, camera, sound): called as the original does ----
void FUN_005c12d0(int target, int source, int kind, float volume, int level);
void FUN_005c13d0(int unit, int kind, int sound);
void FUN_005c1320(int unit, int on);
void FUN_005c1360(int unit, int on);
void FUN_005c1200(int unit);
// The tactical sound manager (presentation): created on first use.
TacticalSoundManager* FUN_005ba8b0();
void __thiscall FUN_005ba980(TacticalSoundManager* manager, int sound, int flag);
