#pragma once
#include <cstdint>

#include "app/player_controller_decl.h"
#include "core/containers.h"
#include "core/game_string.h"
#include "core/utility_classes.h"
#include "tactical/tactical_lists.h"
#include "tactical/tactical_side.h"
#include "tactical/tactical_types.h"
#include "tactical/tactical_units.h"

class TacticalShipBase;
class TacticalDeathStar;

// The headless battle simulator (DAT_006bc4a8, 0x274 bytes): the auto-resolve engine. It runs the same capital-ship and
// fighter unit objects as the 3D battle, one fixed step per frame, without any rendering (TacticalGame::Update with the
// abstract flag set steps it with 1.0). Its constructor (0x5af5f0) and most unit handling belong to the capital-ship
// module; the part reconstructed here is the step and the end-of-battle bookkeeping (0x5ae9c0-0x5af4b0).

enum SimUnitState {
    kUnitDockedOrActive = 1,
    kUnitActive = 2,
    kUnitWithdrawing = 5,
    kUnitDeathStarPresent = 9,
};

// The two concrete sides of the simulator (VT 0x66c428 / 0x66c3f8): the AI side with its index (slot 1 is an ICF-shared
// `return 0` / `return 1`, 0x5a5b60 / 0x5a66f0).
class SimulatorSideA : public TacticalAiSide {
public:
    virtual ~SimulatorSideA();  // 0x5d14a0 (scalar deleting destructor 0x5affd0)
    SimulatorSideA() {
        m_unkb0 = 0;
        m_unk48 = 0;
        m_unk4c = 0;
        m_unk50 = 1.0f;
        m_unk54 = -1.0f;
    }
    virtual int SideIndex() override { return 0; }
};

class SimulatorSideB : public TacticalAiSide {
public:
    virtual ~SimulatorSideB();  // 0x5d14b0 (scalar deleting destructor 0x5afff0)
    SimulatorSideB() {
        m_unkb0 = 0;
        m_unk48 = 0;
        m_unk4c = 0;
        m_unk50 = -1.0f;
        m_unk54 = -1.0f;
    }
    virtual int SideIndex() override { return 1; }
};

class CapitalShipGroup;

struct HeadlessSimulator {
    HeadlessSimulator();        // 0x5af5f0
    ~HeadlessSimulator();       // 0x5afa50 (called as a body: the object is freed by the caller)

    float m_elapsed;            // +0x00 simulated time
    RandomGenerator m_random;   // +0x04
    int m_unk0c;                // +0x0c 1
    BattleOutcome m_outcome;    // +0x10
    TacticalCharacterTree m_characters;        // +0x2c characters by handle
    TacticalSpecialForceTree m_specialForces;  // +0x38 special forces and troops by handle
    int m_battleKind;           // +0x44
    uint8_t m_pad48[4];
    UnitRefTree m_units;        // +0x4c every unit in the battle
    UnitRefTree m_launched;     // +0x58 units whose carrier was found (launched fighters)
    UnitRefTree m_active;       // +0x64 units in state 2
    UnitRefTree m_retired;      // +0x70 units that left the battle (state 0xb)
    String m_strings[0x26];     // +0x7c
    uint8_t m_pad244[8];
    TacticalAiSide* m_sides[2];  // +0x24c
    int m_secondCommander[2];   // +0x254 handle of each side's second-best commander
    int m_firstCommander[2];    // +0x25c handle of each side's best commander
    float m_bestStrength[2];    // +0x264 the lowest strength each side has reached
    float m_stagnantTime[2];    // +0x26c time since each side's strength last dropped

    int Step(float step);                     // 0x5ae9c0
    int RandomBetween(int low, int high);     // 0x5af420
    int AddCharacter(TacticalUnit* character);       // 0x5af440
    int AddSpecialForce(TacticalUnit* force);        // 0x5af490
    int AddLaunchedFighter(TacticalShipBase* fighter);   // 0x5af4b0
    void RecordSurvivors();                   // 0x5aee90
    void DeathStarAttackRun(ShipGroup* group);                    // 0x5aefc0
    void AssignTarget(TacticalShipBase* target, ShipGroup* group);  // 0x5afa00: the group's target becomes `target`

    // ---- units (capital-ship module) -----------------------------------------------------------------------
    int ActivateUnit(TacticalShipBase* ship);                 // 0x5af5c0
    void RetireUnit(TacticalShipBase* unit);                  // 0x5af7a0
    void DockAtDeathStar(TacticalShipBase* unit);             // 0x5af910
    double OpponentStrength(int side);                        // 0x5afa30
    void SetSideFlag(int side, int value);                    // 0x5afb30
    void SetSideValue(int side, float value);                 // 0x5afb50
    double CommanderSkillA(int side);                         // 0x5afb70 first commander's leadership
    double CommanderSkillB(int side);                         // 0x5afc00 second commander's combat skill
    int IsActiveUnit(TacticalShipBase* ship);                 // 0x5afc80
    void RegisterFighter(TacticalShipBase* unit);             // 0x5afd40
    void RegisterCapitalShip(TacticalShipBase* unit);         // 0x5afd70
    void AssignCharacterHandle(TacticalUnit* unit);           // 0x5afdb0
    int AddDeathStar(int family, int id, int notCompleted, int hasFacility, float hull, float hullNow,
                     int hangarFree);                         // 0x5af4e0
    void RegisterDeathStar(TacticalDeathStar* deathStar);     // 0x5afdc0
    int SideZeroHasMoreFighters();                            // 0x5afcc0
    int DockInHangar(TacticalShipBase* unit);                 // 0x5afe40
    // Battle-start grouping (tactical_battle_groups.cpp): sorts the units into the sides' lists and forms capital-ship
    // groups by type, then splits the fighters into groups.
    CapitalShipGroup* FormCapitalGroup(UnitRefTree* units, int unitType, int role);  // 0x5ae330
    void FormBattleGroups(int unused);                                               // 0x5ae460
};
