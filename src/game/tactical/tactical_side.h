#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "tactical/tactical_groups.h"
#include "tactical/tactical_lists.h"
#include "tactical/tactical_units.h"

// A battle side of the tactical engine (tactical AI module 0x5c5b30-0x5d2360). The base class (VT 0x66cc48) keeps the
// side's fleet, hangars, groups and target priorities; the headless (auto-resolve) simulator owns two sides of a class
// that adds the unit-by-unit AI (TacticalAiSide, whose two leaf classes, one per side index, are built inline by the
// simulator's constructor 0x5af5f0), the 3D battle's object manager uses other subclasses.

class HeadlessSimulator;

// ---- the two scorers a side owns ----------------------------------------------------------------------------------

// Rates the targets for the units of a side (VT 0x66cc70; the concrete class is AttackEvaluator).
class AttackEvaluatorBase {
public:
    virtual ~AttackEvaluatorBase();  // 0x5d1e00 (scalar deleting destructor 0x5c9720)
    virtual void Virtual01();        // 0x5ed170
    virtual float Evaluate(int a, SimUnit* unit) = 0;  // slot 2
    virtual int Virtual03(ShipGroup* group) = 0;       // slot 3
    virtual int Virtual04(SimUnit* unit) = 0;          // slot 4
};

class AttackEvaluator : public AttackEvaluatorBase {  // VT 0x66d100
public:
    AttackEvaluator() {}
    virtual float Evaluate(int a, SimUnit* unit) override;  // 0x5edc10
    virtual int Virtual03(ShipGroup* group) override;       // 0x5eda50
    virtual int Virtual04(SimUnit* unit) override;          // 0x5eda20
};

class TacticalSideBase;
class TargetPriorityTree;

// Assigns the groups of a side to targets (VT 0x66cc88; the concrete class is FormationEvaluator).
class FormationEvaluatorBase {
public:
    virtual ~FormationEvaluatorBase();  // 0x5d1e10 (scalar deleting destructor 0x5c9760)
    virtual void Assign(TacticalSideBase* side, GroupRefTree* groups, TargetPriorityTree* targets, float limit);  // slot 1
};

class FormationEvaluator : public FormationEvaluatorBase {  // VT 0x66d118
public:
    FormationEvaluator() {}
    virtual void Assign(TacticalSideBase* side, GroupRefTree* groups, TargetPriorityTree* targets, float limit) override;  // 0x5ecb90
};

// A tree of orders the side issues (VT 0x66cc28); the nodes are 0x30 bytes (AvlNode, a float key, a position record).
class SideOrderTree : public AvlTree {
public:
    SideOrderTree() {}
    virtual ~SideOrderTree();                              // 0x5c9560 (scalar deleting destructor 0x5c9650)
    virtual int IsLess(AvlNode* a, AvlNode* b) override;   // 0x5c9570
};

// ---- the side ------------------------------------------------------------------------------------------------------

class TacticalSideBase {
public:
    TacticalSideBase();           // 0x5c7cb0
    virtual ~TacticalSideBase();  // 0x5c7a90 (scalar deleting destructor 0x5c7bc0)
    virtual int SideIndex() = 0;  // slot 1: 0 or 1
    virtual TargetPriorityTree* BuildTargetList();  // 0x5c8e10 slot 2
    virtual int HasNoShipsAtAll();                  // 0x5c7e80
    virtual int ResetEvaluators();                  // 0x5c8d20
    virtual int Refresh(int a);                     // 0x5c7f70 slot 5
    virtual int Slot06();                           // 0x5c8300
    virtual int SplitIntoGroups(TargetPriorityTree* targets, int least, int most);  // 0x5c83c0
    virtual int Slot08();                           // 0x5c8660

    int HasDisabledUnit();                     // 0x5c7e40 (a fleet unit with +0x150 set)
    void RecordFleetPriorities();              // 0x5c81d0 notes the strength and sorts the fleet into the target lists
    float EvaluateAttack(int a, SimUnit* unit);  // 0x5c9190
    int EvaluateUnit4(SimUnit* unit);          // 0x5c91c0
    int EvaluateUnit3(ShipGroup* group);       // 0x5c91e0
    void ReprioritiseTargets(float step);      // 0x5c9200

    TurnSyncChannelA m_channelA;       // +0x04
    TurnSyncChannelB m_channelB;       // +0x0c
    int m_unk14;                       // +0x14 1
    int m_unk18;                       // +0x18
    int m_unk1c;                       // +0x1c
    int m_unk20;                       // +0x20
    float m_unk24;                     // +0x24 0.33
    int m_attackEvaluatorKind;         // +0x28
    AttackEvaluatorBase* m_attackEvaluator;        // +0x2c
    int m_formationEvaluatorKind;      // +0x30
    FormationEvaluatorBase* m_formationEvaluator;  // +0x34
    float m_previousStrength;          // +0x38
    float m_strength;                  // +0x3c
    int m_unk40;                       // +0x40
    int m_unitCount;                   // +0x44
    int m_unk48;                       // +0x48
    int m_unk4c;                       // +0x4c
    int m_unk50;                       // +0x50
    float m_unk54;                     // +0x54 -1
    UnitRefTree m_fleet;               // +0x58 the side's units on the field
    UnitRefTree m_hangars;             // +0x64 the side's units still in hangars
    GroupRefTree m_groups;             // +0x70
    int m_unk7c;                       // +0x7c
    int m_unk80;                       // +0x80
    int m_state;                       // +0x84 5 while the side is withdrawing
    SideOrderTree m_orders;            // +0x88
    TargetPriorityTree m_targets[4];   // +0x90 (+0x90 the primary list, then three by unit kind)
    int m_unkb0;                       // +0xb0
};

// The side of the headless simulator: slots 2-11 are the AI (the 3D sides keep the base implementations).
class TacticalAiSide : public TacticalSideBase {
public:
    virtual TargetPriorityTree* BuildTargetList() override;   // 0x5d1a70
    virtual int HasNoShipsAtAll() override;                   // 0x5d0f50
    virtual int ResetEvaluators() override;                   // 0x5d1920
    virtual int Refresh(int a) override;                      // 0x5d0ce0
    virtual int Slot06() override;                            // 0x5d1410
    virtual int SplitIntoGroups(TargetPriorityTree* targets, int least, int most) override;  // 0x5d1240
    virtual int Slot08() override;                            // 0x5d1080
    virtual int RecordFleet(int unused);                      // 0x5d14c0 slot 9
    virtual int StepSide(float step);                         // 0x5d14d0 slot 10
    virtual int Virtual11();                                  // 0x5d19a0
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(SideOrderTree, 0x08);
REB_ASSERT_OFFSET(TacticalSideBase, m_channelB, 0x0c);
REB_ASSERT_OFFSET(TacticalSideBase, m_unk24, 0x24);
REB_ASSERT_OFFSET(TacticalSideBase, m_attackEvaluator, 0x2c);
REB_ASSERT_OFFSET(TacticalSideBase, m_formationEvaluator, 0x34);
REB_ASSERT_OFFSET(TacticalSideBase, m_strength, 0x3c);
REB_ASSERT_OFFSET(TacticalSideBase, m_fleet, 0x58);
REB_ASSERT_OFFSET(TacticalSideBase, m_hangars, 0x64);
REB_ASSERT_OFFSET(TacticalSideBase, m_groups, 0x70);
REB_ASSERT_OFFSET(TacticalSideBase, m_state, 0x84);
REB_ASSERT_OFFSET(TacticalSideBase, m_orders, 0x88);
REB_ASSERT_OFFSET(TacticalSideBase, m_targets, 0x90);
REB_ASSERT_OFFSET(TacticalSideBase, m_unkb0, 0xb0);
REB_ASSERT_SIZE(TacticalSideBase, 0xb4);
#pragma clang diagnostic pop
