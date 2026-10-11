#include "tactical/tactical_facade.h"
#include "core/icf_stubs.h"

#include <new>

#include "core/checked_new.h"
#include "tactical/tactical_externals.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_ship.h"
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

void TacticalBeginBattle(int humanIsNotAlliance, int opponentIsAi, uint32_t systemField44, std::string systemName,
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

int TacticalRegisterCapitalShip(
    uint32_t family, uint32_t id, int notAlliance, uint32_t typeBits, std::string name, float hull, float currentHull,
    float damageControl, int freeHangar, float tractorPower, float tractorRange, int gravityWell, float interdiction,
    float field64, float field68, float field60, float hyperdriveCount, float gridTotal, float weaponRecharge,
    float shieldRecharge, int grid02, int grid12, int grid22, int grid32, int grid01, int grid11, int grid21,
    int grid31, int grid00, int grid10, int grid20, int grid30, int speedCH, int shieldCH, int weaponCH,
    int hyperdriveCH, int tractorCH, float a42, float a41, float a40) {
    const uint32_t typeIndex = GetTacticalGame()->FindShipTypeIndex(typeBits);
    void* memory = ::operator new(sizeof(TacticalCapitalShip));
    TacticalCapitalShip* ship = nullptr;
    if (memory) {
        ship = new (memory) TacticalCapitalShip(family, id, notAlliance, typeIndex, name, hull, currentHull,
                                                damageControl, freeHangar, tractorPower, tractorRange, gravityWell,
                                                interdiction, field64, field68, field60, hyperdriveCount,
                                                weaponRecharge, shieldRecharge, grid02, grid12, grid22, grid32, grid01,
                                                grid11, grid21, grid31, grid00, grid10, grid20, grid30, speedCH,
                                                shieldCH, weaponCH, hyperdriveCH, tractorCH, a42, a41, a40, 100);
    }
#ifndef REB_NO_TACTICAL_3D
    if (GetTacticalGame()->m_abstract == 0) {
        GetObjectManager()->Virtual24(ship);
        return GetObjectManager()->AddShipToWorld(ship);
    }
#endif
    GetHeadlessSimulator()->RegisterCapitalShip(ship);
    return GetHeadlessSimulator()->ActivateUnit(ship);
}

int TacticalRegisterDeathStar(uint32_t family, uint32_t id, int notCompleted, int hasFacility, float hull,
                                      float currentHull, int hangarFree) {
#ifdef REB_NO_TACTICAL_3D
    return GetHeadlessSimulator()->AddDeathStar(family, id, notCompleted, hasFacility, hull, currentHull, hangarFree);
#else
    if (GetTacticalGame()->m_abstract != 0) {
        return GetHeadlessSimulator()->AddDeathStar(family, id, notCompleted, hasFacility, hull, currentHull,
                                                    hangarFree);
    }
    return GetTacticalGame()->AddDeathStar(family, id, notCompleted, hasFacility, hull, currentHull, hangarFree);
#endif
}

int TacticalRegisterSpecialForce(uint32_t family, uint32_t id, uint32_t typeBits, std::string name, int notAlliance,
                                         int shipHandle) {
    const uint16_t spriteId = GetTacticalGame()->LookupTypeSprite(static_cast<int16_t>(typeBits));
    void* memory = ::operator new(sizeof(TacticalSpecialForceUnit));
    TacticalSpecialForceUnit* unit = nullptr;
    if (memory) unit = new (memory) TacticalSpecialForceUnit(family, id, spriteId, name, notAlliance, shipHandle);
    TacticalUnit* base = unit;
#ifndef REB_NO_TACTICAL_3D
    if (GetTacticalGame()->m_abstract == 0) {
        GetObjectManager()->Virtual21(base);
        return GetObjectManager()->AddSpecialForceToWorld(base);
    }
#endif
    GetHeadlessSimulator()->AssignCharacterHandle(base);
    return GetHeadlessSimulator()->AddSpecialForce(base);
}

int TacticalRegisterCharacter(uint32_t family, uint32_t id, std::string name, int notAlliance, int isMajor,
                                      int isFirstCommander, int isSecondCommander, float scaleA,
                                      float scaleB, int shipHandle, int injured, uint32_t typeBits) {
    const uint16_t spriteId = GetTacticalGame()->LookupTypeSprite(static_cast<int16_t>(typeBits));
    void* memory = ::operator new(sizeof(TacticalCharacterUnit));
    TacticalCharacterUnit* unit = nullptr;
    if (memory) {
        unit = new (memory) TacticalCharacterUnit(family, id, spriteId, name, notAlliance, isMajor, isFirstCommander,
                                                  isSecondCommander, scaleA, scaleB, shipHandle, injured);
    }
    TacticalUnit* base = unit;
#ifndef REB_NO_TACTICAL_3D
    if (GetTacticalGame()->m_abstract == 0) {
        GetObjectManager()->Virtual22(base);
        return GetObjectManager()->AddCharacterToWorld(base);
    }
#endif
    GetHeadlessSimulator()->AssignCharacterHandle(base);
    return GetHeadlessSimulator()->AddCharacter(base);
}

int TacticalRegisterFightSquad(
    uint32_t family, uint32_t id, int notAlliance, uint32_t typeBits, std::string name, int carrierHandle,
    int carrierIsCapitalShip, float field64, float field68, float field60, int squadSize, int currentSize, int zero,
    float hyperdriveCount, int grid02, int grid12, int grid22, int grid32, int grid01, int grid11, int grid21,
    int grid31, int grid00, int grid10, int grid20, int grid30, float a42, float a41, float a40, int totalTorpedoes,
    float torpedoRange) {
    const uint32_t typeIndex = GetTacticalGame()->FindShipTypeIndex(typeBits);
    void* memory = ::operator new(sizeof(TacticalFighter));
    TacticalFighter* fighter = nullptr;
    if (memory) {
        fighter = new (memory) TacticalFighter(family, id, notAlliance, typeIndex, name, carrierHandle, field64,
                                               field68, field60, squadSize, currentSize, zero, hyperdriveCount, grid02,
                                               grid12, grid22, grid32, grid01, grid11, grid21, grid31, grid00, grid10,
                                               grid20, grid30, a42, a41, a40, totalTorpedoes, torpedoRange);
    }
#ifndef REB_NO_TACTICAL_3D
    if (GetTacticalGame()->m_abstract == 0) {
        GetObjectManager()->Virtual25(fighter);
    } else {
        GetHeadlessSimulator()->RegisterFighter(fighter);
    }
#else
    GetHeadlessSimulator()->RegisterFighter(fighter);
#endif
    if (carrierIsCapitalShip == kAttachedToCapitalShip) {
        TacticalHangar* carrier = GetTacticalRegistry()->FindLaunchInterface(carrierHandle);
        if (carrier) {
            if (fighter->UpdateHyperdriveReady() == 0) {
                fighter->SetState1();
                carrier->Launch(fighter);
#ifdef REB_NO_TACTICAL_3D
                return GetHeadlessSimulator()->AddLaunchedFighter(fighter);
#else
                if (GetTacticalGame()->m_abstract != 0) return GetHeadlessSimulator()->AddLaunchedFighter(fighter);
                return GetObjectManager()->AddLaunchedFighterToWorld(fighter);
#endif
            }
            fighter->SetState2();
#ifdef REB_NO_TACTICAL_3D
            return GetHeadlessSimulator()->ActivateUnit(fighter);
#else
            if (GetTacticalGame()->m_abstract != 0) return GetHeadlessSimulator()->ActivateUnit(fighter);
            return GetObjectManager()->AddShipToWorld(fighter);
#endif
        }
    }
    fighter->SetState2();
#ifdef REB_NO_TACTICAL_3D
    return GetHeadlessSimulator()->ActivateUnit(fighter);
#else
    if (GetTacticalGame()->m_abstract != 0) return GetHeadlessSimulator()->ActivateUnit(fighter);
    return GetObjectManager()->AddShipToWorld(fighter);
#endif
}

// ---- results -------------------------------------------------------------------------------------------------------

CapitalShipResult* TacticalGetCapitalShipResult(CapitalShipResult* out, int handle) {
    CapitalShipResult result;
    TacticalCapitalShip* ship = static_cast<TacticalCapitalShip*>(GetTacticalRegistry()->FindShip(handle));
    if (ship) {
        result = *ship->BuildResult(&result);
    } else {
        TacticalUnit* deathStar = GetTacticalRegistry()->GetDeathStar();
        if (deathStar) result = *static_cast<TacticalDeathStar*>(deathStar)->BuildCapitalShipResult(&result);
    }
    *out = result;
    return out;
}

FightSquadResult* TacticalGetFightSquadResult(FightSquadResult* out, int handle) {
    FightSquadResult result;
    TacticalFighter* fighter = static_cast<TacticalFighter*>(GetTacticalRegistry()->FindShip(handle));
    if (fighter) result = *fighter->BuildResult(&result);
    *out = result;
    return out;
}

CharacterResult* TacticalMakeCharacterResult(CharacterResult* out, int handle) {
    CharacterResult result = {handle, {0, 0, 0, 0, 0, 0, 0}};
    *out = result;
    return out;
}

DeathStarResult* TacticalGetDeathStarResult(DeathStarResult* out) {
    DeathStarResult result;
    TacticalUnit* deathStar = GetTacticalRegistry()->GetDeathStar();
    if (deathStar) result = *static_cast<TacticalDeathStar*>(deathStar)->BuildResult(&result);
    *out = result;
    return out;
}

BattleOutcome* TacticalGetBattleOutcome(BattleOutcome* out, int unused) {
    BattleOutcome outcome;
    GetTacticalGame()->GetOutcome(&outcome);
    *out = outcome;
    return out;
}

// ---- registry and simulator -----------------------------------------------------------------------------------------

TacticalRegistry* GetTacticalRegistry() {
    TacticalGame* game = GetTacticalGame();
    return game ? game->GetOrCreateRegistry() : nullptr;
}

// ---- small forwarders ----------------------------------------------------------------------------------------------

void TacticalPostNotice(int a, int unused) { GetTacticalGame()->PostHubNotice(a, unused); }

void TacticalHubSync(int a) { FUN_005a5fd0(FUN_0059ed70(), a); }

int TacticalHasDeathStar() { return GetTacticalRegistry()->GetDeathStar() != nullptr; }

void TacticalPostPair(int a, int b) { GetTacticalGame()->PostHubPair(a, b); }

int TacticalHubIsRunning() { return GetTacticalGame()->HubIsRunning(); }

void TacticalCopyPlayerFlags8(uint32_t* out) { GetTacticalGame()->CopyHubPlayerFlags8(out); }

void TacticalCopyPlayerFlags4(uint32_t* out) { GetTacticalGame()->CopyHubPlayerFlags4(out); }

void TacticalCallClientSlot1() { GetTacticalGame()->CallClientSlot1(); }

int TacticalReturnMinusOne() { return -1; }

void TacticalHubSelectA(int a) { FUN_0059eec0(FUN_0059ed70(), a); }

void TacticalHubSelectB(int a) { FUN_0059eef0(FUN_0059ed70(), a); }

int TacticalGetPlayerSide() { return GetTacticalGame()->GetPlayerSide(); }

int TacticalPlayerSideIsActive() {
#ifdef REB_NO_TACTICAL_3D
    return 0;
#else
    TacticalObjectManager* manager = GetObjectManager();
    return manager ? manager->PlayerSideIsActive() : 0;
#endif
}

int TacticalModeIsSix() {
#ifdef REB_NO_TACTICAL_3D
    return 0;
#else
    return GetObjectManager()->m_unk8b0 == 6;
#endif
}

void TacticalPostMessage1(int arg) { GetTacticalGame()->PostHubMessage1(arg); }

void TacticalPostMessage0(int arg) { GetTacticalGame()->PostHubMessage0(arg); }

void TacticalPostMessage3IfSet(int arg) {
    if (arg != 0) GetTacticalGame()->PostHubMessage3(arg);
}

void TacticalPostMessage2IfSet(int arg) {
    if (arg != 0) GetTacticalGame()->PostHubMessage2(arg);
}

void TacticalForwardEvent(int a, int b, int c) { GetTacticalGame()->ForwardToObjectManager(a, b, c); }

void TacticalCopyManagerBlock(uint32_t* out, int index) {
#ifdef REB_NO_TACTICAL_3D
    (void)index;
    out[0] = out[1] = out[2] = out[3] = 0;
#else
    const uint32_t* source =
        reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(GetObjectManager()) + (index + 0x96) * 16);
    out[0] = source[0];
    out[1] = source[1];
    out[2] = source[2];
    out[3] = source[3];
#endif
}

void InitTacticalFacadeScale() { g_tacticalFacadeScaleSquared = g_tacticalFacadeScaleBase * g_tacticalFacadeScaleBase; }

int TacticalCanCommandShip(void* ship) {
#ifdef REB_NO_TACTICAL_3D
    (void)ship;
    return 0;
#else
    TacticalObjectManager* manager = GetObjectManager();
    return manager ? manager->CanPlayerCommandShip(ship) : 0;
#endif
}

int TacticalCanCommandFighter(void* fighter) {
#ifdef REB_NO_TACTICAL_3D
    (void)fighter;
    return 0;
#else
    TacticalObjectManager* manager = GetObjectManager();
    return manager ? manager->CanPlayerCommandFighter(fighter) : 0;
#endif
}

void TacticalPause() {
#ifdef REB_NO_TACTICAL_3D
    return;
#else
    GetTacticalGame();
    GetObjectManager()->Virtual03();
#endif
}

void TacticalResume() {
#ifdef REB_NO_TACTICAL_3D
    return;
#else
    GetTacticalGame();
    GetObjectManager()->Virtual02();
#endif
}

int TacticalIsPaused() {
#ifdef REB_NO_TACTICAL_3D
    return 0;
#else
    GetTacticalGame();
    return GetObjectManager()->m_pauseCount != 0;
#endif
}

void TacticalPostNoticeIfPlayerSideCanAct() {
    if (TacticalPlayerSideCanAct() != 0) GetTacticalGame()->PostHubNotice(0x7a, 0);
}

int TacticalPlayerSideCanAct() {
#ifdef REB_NO_TACTICAL_3D
    return 0;
#else
    const int side = GetObjectManager()->m_playerSide;
    return GetObjectManager()->PlayerSideCanAct(side);
#endif
}

void TacticalCheckAnySideFlag() {
#ifndef REB_NO_TACTICAL_3D
    if (TacticalObjectManager* manager = GetObjectManager()) manager->AnySideHasFlag();
#endif
}

void TacticalHubCheckSides() {
    FUN_0059ed70();
    FUN_005a0240();
}

int16_t TacticalGetBackdropId() {
#ifdef REB_NO_TACTICAL_3D
    return 0;
#else
    return static_cast<int16_t>(GetObjectManager()->m_systemField44 + 0x157c);
#endif
}

uint32_t TacticalGetCarrierStat68(void* ship) {
    if (ship) {
        const int carrierHandle = *reinterpret_cast<int*>(static_cast<char*>(ship) + 0x178);
        if (carrierHandle) {
            TacticalUnit* carrier = reinterpret_cast<TacticalUnit*>(GetTacticalRegistry()->TacticalRegistry::FindShipGroup(carrierHandle));
            if (carrier) return *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(carrier) + 0x68);
        }
    }
    return 0;
}

uint32_t TacticalGetUnitStat68(void* unit) {
    return unit ? *reinterpret_cast<uint32_t*>(static_cast<char*>(unit) + 0x68) : 0;
}

uint32_t TacticalGetCarrierStat38(void* ship) {
    if (ship) {
        const int carrierHandle = *reinterpret_cast<int*>(static_cast<char*>(ship) + 0x178);
        if (carrierHandle) {
            TacticalUnit* carrier = reinterpret_cast<TacticalUnit*>(GetTacticalRegistry()->TacticalRegistry::FindShipGroup(carrierHandle));
            if (carrier) return *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(carrier) + 0x38);
        }
    }
    return 0;
}

uint32_t TacticalGetUnitStat38(void* unit) {
    return unit ? *reinterpret_cast<uint32_t*>(static_cast<char*>(unit) + 0x38) : 0;
}

void TacticalNotifySync(int handle) { FUN_005bbd90(FUN_005bbe70(handle), handle); }

void TacticalNotifySyncQuiet(int handle) { IgnoreOneArgument(FUN_005bbe70(handle), handle); }

int TacticalWorldCheck() {
#ifdef REB_NO_TACTICAL_3D
    return 0;
#else
    return GetObjectManager()->WorldCheck();
#endif
}

void TacticalObjectManagerSlot7() {
#ifndef REB_NO_TACTICAL_3D
    GetObjectManager()->Virtual07();
#endif
}

float g_tacticalFacadeScaleBase = 3.0f;
float g_tacticalFacadeScaleSquared{};
