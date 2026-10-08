#include "tactical/tactical_facade.h"

#include <new>

#include "core/checked_new.h"
#include "tactical/tactical_externals.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_simulator.h"
#include "tactical/tactical_units.h"

extern float g_tacticalFacadeScaleBase;     // 0x66bab8
extern float g_tacticalFacadeScaleSquared;  // 0x6bc240

namespace {

constexpr float kStrengthScale = 0.4f;
constexpr int kAttachedToCapitalShip = 1;
constexpr int kStateActive = 2;

// What the facade does with a freshly built unit: hand it to the simulator or the object manager.
template <class Unit>
Unit* constructOrNull(unsigned size, ...);

TacticalUnit* asUnit(void* memory) { return static_cast<TacticalUnit*>(memory); }

}  // namespace

// ---- start of a battle ------------------------------------------------------------------------------------------

void __cdecl TacticalBeginBattle(int humanIsNotAlliance, int opponentIsAi, uint32_t systemField44, String systemName,
                                 float sideAValue, float sideBValue, int sideAC, int sideBC, GameTime timeA,
                                 GameTime timeB, int battleKindNotAlliance, int systemFlag8, int controlIsEmpire,
                                 int controlIsAlliance, int abstractFlag) {
    if (opponentIsAi == 1) {
        TacticalGameSetupBattle(GetTacticalGame(), humanIsNotAlliance, systemField44, systemName, sideAValue * kStrengthScale,
                                       sideBValue * kStrengthScale, sideAC, sideBC, timeA, timeB,
                                       battleKindNotAlliance, systemFlag8, controlIsEmpire, controlIsAlliance,
                                       abstractFlag);
    } else {
        TacticalGameSetupBattleLocal(GetTacticalGame(), humanIsNotAlliance, systemField44, systemName,
                                            sideAValue * kStrengthScale, sideBValue * kStrengthScale, sideAC, sideBC,
                                            battleKindNotAlliance, systemFlag8, controlIsEmpire, controlIsAlliance,
                                            abstractFlag);
    }
}

void TacticalStart() { GetTacticalGame()->BeginBattle(10); }

// ---- unit registration --------------------------------------------------------------------------------------------

int __cdecl TacticalRegisterCapitalShip(
    uint32_t family, uint32_t id, int notAlliance, uint32_t typeBits, String name, float hull, float currentHull,
    float damageControl, int freeHangar, float tractorPower, float tractorRange, int gravityWell, float interdiction,
    float field64, float field68, float field60, float hyperdriveCount, float gridTotal, float weaponRecharge,
    float shieldRecharge, int grid02, int grid12, int grid22, int grid32, int grid01, int grid11, int grid21,
    int grid31, int grid00, int grid10, int grid20, int grid30, int speedCH, int shieldCH, int weaponCH,
    int hyperdriveCH, int tractorCH, float a42, float a41, float a40) {
    const uint32_t typeIndex = GetTacticalGame()->FindShipTypeIndex(typeBits);
    void* memory = ::operator new(0x448);
    TacticalCapitalShip* ship = nullptr;
    if (memory) {
        ship = FUN_005b05c0(memory, family, id, notAlliance, typeIndex, name, hull, currentHull, damageControl,
                            freeHangar, tractorPower, tractorRange, gravityWell, interdiction, field64, field68,
                            field60, hyperdriveCount, weaponRecharge, shieldRecharge, grid02, grid12,
                            grid22, grid32, grid01, grid11, grid21, grid31, grid00, grid10, grid20, grid30, speedCH,
                            shieldCH, weaponCH, hyperdriveCH, tractorCH, a42, a41, a40, 100);
    }
    TacticalUnit* unit = reinterpret_cast<TacticalUnit*>(ship);
    if (GetTacticalGame()->m_abstract == 0) {
        GetObjectManager()->Virtual24(unit);
        return GetObjectManager()->AddShipToWorld(unit);
    }
    FUN_005afd70(GetHeadlessSimulator(), unit);
    return FUN_005af5c0(GetHeadlessSimulator(), unit);
}

int __cdecl TacticalRegisterDeathStar(uint32_t family, uint32_t id, int notCompleted, int hasFacility, float hull,
                                      float currentHull, int hangarFree) {
    if (GetTacticalGame()->m_abstract != 0) {
        return FUN_005af4e0(GetHeadlessSimulator(), family, id, notCompleted, hasFacility, hull, currentHull,
                            hangarFree);
    }
    return GetTacticalGame()->AddDeathStar(family, id, notCompleted, hasFacility, hull, currentHull, hangarFree);
}

int __cdecl TacticalRegisterSpecialForce(uint32_t family, uint32_t id, uint32_t typeBits, String name, int notAlliance,
                                         int shipHandle) {
    const uint16_t spriteId = GetTacticalGame()->LookupTypeSprite(static_cast<int16_t>(typeBits));
    void* memory = ::operator new(0x4c);
    TacticalSpecialForceUnit* unit = nullptr;
    if (memory) unit = new (memory) TacticalSpecialForceUnit(family, id, spriteId, name, notAlliance, shipHandle);
    TacticalUnit* base = unit;
    if (GetTacticalGame()->m_abstract == 0) {
        GetObjectManager()->Virtual21(base);
        return GetObjectManager()->AddSpecialForceToWorld(base);
    }
    FUN_005afdb0(GetHeadlessSimulator(), base);
    return FUN_005af490(GetHeadlessSimulator(), base);
}

int __cdecl TacticalRegisterCharacter(uint32_t family, uint32_t id, String name, int notAlliance, int isMajor,
                                      int isFirstCommander, int isSecondCommander, float scaleA,
                                      float scaleB, int shipHandle, int injured, uint32_t typeBits) {
    const uint16_t spriteId = GetTacticalGame()->LookupTypeSprite(static_cast<int16_t>(typeBits));
    void* memory = ::operator new(0x64);
    TacticalCharacterUnit* unit = nullptr;
    if (memory) {
        unit = new (memory) TacticalCharacterUnit(family, id, spriteId, name, notAlliance, isMajor, isFirstCommander,
                                                  isSecondCommander, scaleA, scaleB, shipHandle, injured);
    }
    TacticalUnit* base = unit;
    if (GetTacticalGame()->m_abstract == 0) {
        GetObjectManager()->Virtual22(base);
        return GetObjectManager()->AddCharacterToWorld(base);
    }
    FUN_005afdb0(GetHeadlessSimulator(), base);
    return FUN_005af440(GetHeadlessSimulator(), base);
}

int __cdecl TacticalRegisterFightSquad(
    uint32_t family, uint32_t id, int notAlliance, uint32_t typeBits, String name, int carrierHandle,
    int carrierIsCapitalShip, float field64, float field68, float field60, int squadSize, int currentSize, int zero,
    float hyperdriveCount, int grid02, int grid12, int grid22, int grid32, int grid01, int grid11, int grid21,
    int grid31, int grid00, int grid10, int grid20, int grid30, float a42, float a41, float a40, int totalTorpedoes,
    float torpedoRange) {
    const uint32_t typeIndex = GetTacticalGame()->FindShipTypeIndex(typeBits);
    void* memory = ::operator new(0x418);
    TacticalFighter* fighter = nullptr;
    if (memory) {
        fighter = FUN_005b9c60(memory, family, id, notAlliance, typeIndex, name, carrierHandle, field64, field68,
                               field60, squadSize, currentSize, zero, hyperdriveCount, grid02, grid12, grid22, grid32,
                               grid01, grid11, grid21, grid31, grid00, grid10, grid20, grid30, a42, a41, a40,
                               totalTorpedoes, torpedoRange);
    }
    TacticalUnit* unit = reinterpret_cast<TacticalUnit*>(fighter);
    if (GetTacticalGame()->m_abstract == 0) {
        GetObjectManager()->Virtual25(unit);
    } else {
        FUN_005afd40(GetHeadlessSimulator(), unit);
    }
    if (carrierIsCapitalShip == kAttachedToCapitalShip) {
        TacticalUnit* carrier = FUN_005a6360(GetTacticalRegistry(), carrierHandle);
        if (carrier) {
            if (FUN_005b1b70(unit) == 0) {
                FUN_005b69d0(unit);
                FUN_005b8570(carrier, unit);
                if (GetTacticalGame()->m_abstract != 0) return FUN_005af4b0(GetHeadlessSimulator(), unit);
                return GetObjectManager()->AddLaunchedFighterToWorld(unit);
            }
            FUN_005b69b0(unit);
            if (GetTacticalGame()->m_abstract != 0) return FUN_005af5c0(GetHeadlessSimulator(), unit);
            return GetObjectManager()->AddShipToWorld(unit);
        }
    }
    FUN_005b69b0(unit);
    if (GetTacticalGame()->m_abstract != 0) return FUN_005af5c0(GetHeadlessSimulator(), unit);
    return GetObjectManager()->AddShipToWorld(unit);
}

// ---- results -------------------------------------------------------------------------------------------------------

CapitalShipResult* __cdecl TacticalGetCapitalShipResult(CapitalShipResult* out, int handle) {
    CapitalShipResult result;
    TacticalUnit* ship = FUN_005a6340(GetTacticalRegistry(), handle);
    if (ship) {
        result = *FUN_005b0280(ship, &result);
    } else {
        TacticalUnit* deathStar = FUN_005a66a0(GetTacticalRegistry());
        if (deathStar) result = *FUN_005ba670(deathStar, &result);
    }
    *out = result;
    return out;
}

FightSquadResult* __cdecl TacticalGetFightSquadResult(FightSquadResult* out, int handle) {
    FightSquadResult result;
    TacticalUnit* fighter = FUN_005a6340(GetTacticalRegistry(), handle);
    if (fighter) result = *FUN_005b8e00(fighter, &result);
    *out = result;
    return out;
}

CharacterResult* __cdecl TacticalMakeCharacterResult(CharacterResult* out, int handle) {
    CharacterResult result = {handle, {0, 0, 0, 0, 0, 0, 0}};
    *out = result;
    return out;
}

DeathStarResult* __cdecl TacticalGetDeathStarResult(DeathStarResult* out) {
    DeathStarResult result;
    TacticalUnit* deathStar = FUN_005a66a0(GetTacticalRegistry());
    if (deathStar) result = *FUN_005ba620(deathStar, &result);
    *out = result;
    return out;
}

BattleOutcome* __cdecl TacticalGetBattleOutcome(BattleOutcome* out, int unused) {
    BattleOutcome outcome;
    GetTacticalGame()->GetOutcome(&outcome);
    *out = outcome;
    return out;
}

// ---- registry and simulator -----------------------------------------------------------------------------------------

TacticalRegistry* GetTacticalRegistry() {
    if (!g_tacticalRegistry) {
        void* memory = ::operator new(0x48);
        g_tacticalRegistry = memory ? FUN_005a6290(memory) : nullptr;
    }
    return g_tacticalRegistry;
}

// ---- small forwarders ----------------------------------------------------------------------------------------------

void __cdecl TacticalPostNotice(int a, int unused) { GetTacticalGame()->PostHubNotice(a, unused); }

void __cdecl TacticalHubSync(int a) { FUN_005a5fd0(FUN_0059ed70(), a); }

int TacticalHasDeathStar() { return FUN_005a66a0(GetTacticalRegistry()) != nullptr; }

void __cdecl TacticalPostPair(int a, int b) { GetTacticalGame()->PostHubPair(a, b); }

int TacticalHubIsRunning() { return GetTacticalGame()->HubIsRunning(); }

void __cdecl TacticalCopyPlayerFlags8(uint32_t* out) { GetTacticalGame()->CopyHubPlayerFlags8(out); }

void __cdecl TacticalCopyPlayerFlags4(uint32_t* out) { GetTacticalGame()->CopyHubPlayerFlags4(out); }

void TacticalCallClientSlot1() { GetTacticalGame()->CallClientSlot1(); }

int TacticalReturnMinusOne() { return -1; }

void __cdecl TacticalHubSelectA(int a) { FUN_0059eec0(FUN_0059ed70(), a); }

void __cdecl TacticalHubSelectB(int a) { FUN_0059eef0(FUN_0059ed70(), a); }

int TacticalGetPlayerSide() { return GetTacticalGame()->GetPlayerSide(); }

int TacticalPlayerSideIsActive() { return GetObjectManager()->PlayerSideIsActive(); }

int TacticalModeIsSix() { return GetObjectManager()->m_unk8b0 == 6; }

void __cdecl TacticalPostMessage1(int arg) { GetTacticalGame()->PostHubMessage1(arg); }

void __cdecl TacticalPostMessage0(int arg) { GetTacticalGame()->PostHubMessage0(arg); }

void __cdecl TacticalPostMessage3IfSet(int arg) {
    if (arg != 0) GetTacticalGame()->PostHubMessage3(arg);
}

void __cdecl TacticalPostMessage2IfSet(int arg) {
    if (arg != 0) GetTacticalGame()->PostHubMessage2(arg);
}

void __cdecl TacticalForwardEvent(int a, int b, int c) { GetTacticalGame()->ForwardToObjectManager(a, b, c); }

void __cdecl TacticalCopyManagerBlock(uint32_t* out, int index) {
    const uint32_t* source =
        reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(GetObjectManager()) + (index + 0x96) * 16);
    out[0] = source[0];
    out[1] = source[1];
    out[2] = source[2];
    out[3] = source[3];
}

void InitTacticalFacadeScale() { g_tacticalFacadeScaleSquared = g_tacticalFacadeScaleBase * g_tacticalFacadeScaleBase; }

int __cdecl TacticalCanCommandShip(void* ship) { return GetObjectManager()->CanPlayerCommandShip(ship); }

int __cdecl TacticalCanCommandFighter(void* fighter) { return GetObjectManager()->CanPlayerCommandFighter(fighter); }

void TacticalPause() {
    GetTacticalGame();
    GetObjectManager()->Virtual03();
}

void TacticalResume() {
    GetTacticalGame();
    GetObjectManager()->Virtual02();
}

int TacticalIsPaused() {
    GetTacticalGame();
    return GetObjectManager()->m_pauseCount != 0;
}

void TacticalPostNoticeIfPlayerSideCanAct() {
    if (TacticalPlayerSideCanAct() != 0) GetTacticalGame()->PostHubNotice(0x7a, 0);
}

int TacticalPlayerSideCanAct() {
    const int side = GetObjectManager()->m_playerSide;
    return GetObjectManager()->PlayerSideCanAct(side);
}

void TacticalCheckAnySideFlag() { GetObjectManager()->AnySideHasFlag(); }

void TacticalHubCheckSides() {
    FUN_0059ed70();
    FUN_005a0240();
}

int16_t TacticalGetBackdropId() { return static_cast<int16_t>(GetObjectManager()->m_systemField44 + 0x157c); }

uint32_t __cdecl TacticalGetCarrierStat68(void* ship) {
    if (ship) {
        const int carrierHandle = *reinterpret_cast<int*>(static_cast<char*>(ship) + 0x178);
        if (carrierHandle) {
            TacticalUnit* carrier = FUN_005a6480(GetTacticalRegistry(), carrierHandle);
            if (carrier) return *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(carrier) + 0x68);
        }
    }
    return 0;
}

uint32_t __cdecl TacticalGetUnitStat68(void* unit) {
    return unit ? *reinterpret_cast<uint32_t*>(static_cast<char*>(unit) + 0x68) : 0;
}

uint32_t __cdecl TacticalGetCarrierStat38(void* ship) {
    if (ship) {
        const int carrierHandle = *reinterpret_cast<int*>(static_cast<char*>(ship) + 0x178);
        if (carrierHandle) {
            TacticalUnit* carrier = FUN_005a6480(GetTacticalRegistry(), carrierHandle);
            if (carrier) return *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(carrier) + 0x38);
        }
    }
    return 0;
}

uint32_t __cdecl TacticalGetUnitStat38(void* unit) {
    return unit ? *reinterpret_cast<uint32_t*>(static_cast<char*>(unit) + 0x38) : 0;
}

void __cdecl TacticalNotifySync(int handle) { FUN_005bbd90(FUN_005bbe70(handle), handle); }

void __cdecl TacticalNotifySyncQuiet(int handle) { FUN_005ae1e0(FUN_005bbe70(handle), handle); }

int TacticalWorldCheck() { return GetObjectManager()->WorldCheck(); }

void TacticalObjectManagerSlot7() { GetObjectManager()->Virtual07(); }
