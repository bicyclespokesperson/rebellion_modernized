#pragma once
#include <cstdint>

#include "app/player_controller_decl.h"
#include "core/containers.h"
#include "core/game_string.h"
#include "core/layout.h"
#include "core/utility_classes.h"
#include "tactical/tactical_ship_decl.h"
#include "tactical/tactical_types.h"
#include "tactical/tactical_units.h"

class TacticalShipBase;
class TacticalDeathStar;

// The headless battle simulator (DAT_006bc4a8, 0x274 bytes): the auto-resolve engine. It runs the same capital-ship and
// fighter unit objects as the 3D battle, one fixed step per frame, without any rendering (TacticalGame::Update with the
// abstract flag set steps it with 1.0). Its constructor (0x5af5f0) and most unit handling belong to the capital-ship
// module; the part reconstructed here is the step and the end-of-battle bookkeeping (0x5ae9c0-0x5af4b0).

// A battle unit (capital ship, fighter, Death Star...): only the interface the simulator uses. The classes live in the
// capital-ship and fighter modules.
class SimUnit {
public:
    virtual void Slot00();
    virtual void Slot01();
    virtual void Slot02();
    virtual void Slot03();
    virtual void Slot04();
    virtual void Slot05();
    virtual void Slot06();
    virtual void Slot07();
    virtual int GetUnitClass();  // slot 8 (+0x20): 0 for a capital ship, 1 for a fighter squadron
    virtual int Slot09();        // slot 9 (+0x24): the unit's type index

    uint8_t m_pad04[0x168 - 4];
    int m_side;                  // +0x168 0 or 1
    uint8_t m_pad16c[4];
    int m_state;                 // +0x170 see SimUnitState
};

enum SimUnitState {
    kUnitDockedOrActive = 1,
    kUnitActive = 2,
    kUnitWithdrawing = 5,
    kUnitDeathStarPresent = 9,
};

// A node of a unit list (the lists are the ship-group module's id-ordered containers).
class SimUnitNode {
public:
    virtual void Slot00();
    virtual void Slot01();
    virtual SimUnitNode* Next();  // slot 2 (+0x08)
    virtual void Slot03();
    virtual SimUnit* Value();     // slot 4 (+0x10)
};

using SimUnitList = TacticalUnitTree;

inline SimUnitNode* FirstNodeOf(SimUnitList& list) { return reinterpret_cast<SimUnitNode*>(list.FirstNode()); }

// One side's state inside the simulator (the object at +0x24c / +0x250; class and vtable live in the capital-ship module).
class SimulatorSide {
public:
    SimulatorSide();                 // 0x5c7cb0 (AI module)
    virtual ~SimulatorSide();        // slot 0: 0x5c7a90
    virtual void Slot01();
    virtual void Slot02();
    virtual int HasNoShipsAtAll();   // slot 3 (+0x0c)
    virtual void Slot04();
    virtual void Refresh(int a);     // slot 5 (+0x14)
    virtual void Slot06();
    virtual void Slot07();
    virtual void Slot08();
    virtual void Slot09();
    virtual int StepSide(float step);  // slot 10 (+0x28)
    virtual void Slot11();

    uint8_t m_pad04[0x20];
    GameTime m_battleTime;           // +0x24 vote time of this side's controller
    uint8_t m_pad28[0x3c - 0x28];
    float m_strength;                // +0x3c
    uint8_t m_pad40[8];
    int m_unk48;                     // +0x48
    int m_unk4c;                     // +0x4c
    float m_sign50;                  // +0x50 1.0 for the first side, -1.0 for the second
    float m_unk54;                   // +0x54 -1.0
    SimUnitList m_fleet;             // +0x58 the side's units on the field
    SimUnitList m_hangars;           // +0x64 the side's units still in hangars
    SimUnitList m_retired;           // +0x70 the side's units that are out of the battle
    float m_value;                   // +0x7c the side's fleet value (set by the facade)
    int m_flag80;                    // +0x80
    int m_unk84;                     // +0x84 5 while the side is withdrawing
    uint8_t m_pad88[0xb0 - 0x88];
    int m_unkB0;                     // +0xb0
};

// The two concrete sides (VT 0x66c428 / 0x66c3f8); their vtables differ only in a few slots of the AI module.
class SimulatorSideA : public SimulatorSide {
public:
    SimulatorSideA() {
        m_unkB0 = 0;
        m_unk48 = 0;
        m_unk4c = 0;
        m_sign50 = 1.0f;
        m_unk54 = -1.0f;
    }
};

class SimulatorSideB : public SimulatorSide {
public:
    SimulatorSideB() {
        m_unkB0 = 0;
        m_unk48 = 0;
        m_unk4c = 0;
        m_sign50 = -1.0f;
        m_unk54 = -1.0f;
    }
};

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
    SimUnitList m_units;        // +0x4c every unit in the battle
    SimUnitList m_launched;     // +0x58 units whose carrier was found (launched fighters)
    SimUnitList m_active;       // +0x64 units in state 2
    SimUnitList m_retired;      // +0x70 units that left the battle (state 0xb)
    String m_strings[0x26];     // +0x7c
    uint8_t m_pad244[8];
    SimulatorSide* m_sides[2];  // +0x24c
    int m_secondCommander[2];   // +0x254 handle of each side's second-best commander
    int m_firstCommander[2];    // +0x25c handle of each side's best commander
    float m_bestStrength[2];    // +0x264 the lowest strength each side has reached
    float m_stagnantTime[2];    // +0x26c time since each side's strength last dropped

    int Step(float step);                     // 0x5ae9c0
    int RandomBetween(int low, int high);     // 0x5af420
    int AddCharacter(TacticalUnit* character);       // 0x5af440
    int AddSpecialForce(TacticalUnit* force);        // 0x5af490
    int AddLaunchedFighter(TacticalUnit* fighter);   // 0x5af4b0
    void RecordSurvivors();                   // 0x5aee90

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
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(SimulatorSide, 0xb4);
REB_ASSERT_SIZE(HeadlessSimulator, 0x274);
REB_ASSERT_OFFSET(HeadlessSimulator, m_strings, 0x7c);
REB_ASSERT_OFFSET(HeadlessSimulator, m_outcome, 0x10);
REB_ASSERT_OFFSET(HeadlessSimulator, m_battleKind, 0x44);
REB_ASSERT_OFFSET(HeadlessSimulator, m_characters, 0x2c);
REB_ASSERT_OFFSET(HeadlessSimulator, m_specialForces, 0x38);
REB_ASSERT_OFFSET(HeadlessSimulator, m_units, 0x4c);
REB_ASSERT_OFFSET(HeadlessSimulator, m_launched, 0x58);
REB_ASSERT_OFFSET(HeadlessSimulator, m_secondCommander, 0x254);
REB_ASSERT_OFFSET(HeadlessSimulator, m_firstCommander, 0x25c);
REB_ASSERT_OFFSET(HeadlessSimulator, m_sides, 0x24c);
REB_ASSERT_OFFSET(HeadlessSimulator, m_bestStrength, 0x264);
REB_ASSERT_OFFSET(HeadlessSimulator, m_stagnantTime, 0x26c);
REB_ASSERT_OFFSET(SimulatorSide, m_battleTime, 0x24);
REB_ASSERT_OFFSET(SimulatorSide, m_strength, 0x3c);
REB_ASSERT_OFFSET(SimulatorSide, m_fleet, 0x58);
REB_ASSERT_OFFSET(SimulatorSide, m_hangars, 0x64);
REB_ASSERT_OFFSET(SimulatorSide, m_unk84, 0x84);
REB_ASSERT_OFFSET(SimulatorSide, m_unkB0, 0xb0);
REB_ASSERT_OFFSET(SimUnit, m_side, 0x168);
REB_ASSERT_OFFSET(SimUnit, m_state, 0x170);
#pragma clang diagnostic pop
