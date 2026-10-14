#pragma once
#include <cstdint>
#include <memory>
#include <set>

#include "core/entry_identity.h"
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
class AttackEvaluatorBase
{
public:
  virtual ~AttackEvaluatorBase(); // 0x5d1e00 (scalar deleting destructor 0x5c9720)
  // Estimated time for the group to reach the unit (-1 when it cannot); adds the group's strength to *total and stores
  // the unit's speed in *speed.
  virtual float EstimateInterceptTime(TacticalShipBase* unit, ShipGroup* group, float* total, float* speed); // 0x5ed170
  virtual float Evaluate(int a, TacticalShipBase* unit) = 0; // slot 2
  virtual int CanGroupAttack(ShipGroup* group) = 0; // slot 3
  virtual int CanUnitAttack(TacticalShipBase* unit) = 0; // slot 4
};

class AttackEvaluator : public AttackEvaluatorBase
{ // VT 0x66d100
public:
  AttackEvaluator()
  {
  }
  float Evaluate(int a, TacticalShipBase* unit) override; // 0x5edc10
  int CanGroupAttack(ShipGroup* group) override; // 0x5eda50
  int CanUnitAttack(TacticalShipBase* unit) override; // 0x5eda20
};

class TacticalSideBase;
class TargetPriorityTree;

// Assigns the groups of a side to targets (VT 0x66cc88; the concrete class is FormationEvaluator).
class FormationEvaluatorBase
{
public:
  virtual ~FormationEvaluatorBase(); // 0x5d1e10 (scalar deleting destructor 0x5c9760)
  virtual int Assign(TacticalSideBase* side, GroupRefTree* groups, TargetPriorityTree* targets, float limit); // slot 1
};

class FormationEvaluator : public FormationEvaluatorBase
{ // VT 0x66d118
public:
  FormationEvaluator()
  {
  }
  int Assign(TacticalSideBase* side,
             GroupRefTree* groups,
             TargetPriorityTree* targets,
             float limit) override; // 0x5ecb90
};

// A pointer to a group compared by the group's id (0x5f2e40 / 0x5f2e70).
struct GroupRefSlot
{
  ShipGroup* m_group;
  int SameGroup(GroupRefSlot* other);
  int GroupBefore(GroupRefSlot* other);
};

// A group with the time it needs to reach a target (VT 0x66da5c, 0x20 bytes); lower time sorts first.
class ScoredGroupNode : public AvlNode
{
public:
  ScoredGroupNode(float priority, ShipGroup* group);
  ~ScoredGroupNode() override; // 0x5ed160 (scalar deleting destructor 0x5ed140)

  float m_priority; // +0x18
  GroupRefSlot m_group; // +0x1c
};

// ScoredGroupNodes (VT 0x66da60), ordered by score and then by group handle.
class ScoredGroupTree
{
public:
  ScoredGroupTree()
  {
  }
  ~ScoredGroupTree();
  void Insert(ScoredGroupNode* node);
  ScoredGroupNode* First() const;
  ScoredGroupNode* Last() const;
  const auto& Entries() const
  {
    return m_nodes;
  }

private:
  struct NodeOrder
  {
    bool operator()(const std::unique_ptr<ScoredGroupNode>& a, const std::unique_ptr<ScoredGroupNode>& b) const;
  };
  std::multiset<std::unique_ptr<ScoredGroupNode>, NodeOrder> m_nodes;
  bool m_clearing = false;
};

// A node of the side's order tree (0x30 bytes): the keys the tree sorts by, and the weight the evaluator reads.
struct SideOrderNode : AvlNode
{
  float m_primary;
  uint32_t m_unitHandle;
  uint32_t m_second;
  float m_weight;
  float m_secondary;
};

// A tree of orders the side issues (VT 0x66cc28); the nodes are SideOrderNodes.
class SideOrderTree
{
public:
  SideOrderTree()
  {
  }
  ~SideOrderTree();
  void Insert(SideOrderNode* node);
  const auto& Entries() const
  {
    return m_nodes;
  }

private:
  struct NodeOrder
  {
    bool operator()(const std::unique_ptr<SideOrderNode>& a, const std::unique_ptr<SideOrderNode>& b) const;
  };
  std::multiset<std::unique_ptr<SideOrderNode>, NodeOrder> m_nodes;
  bool m_clearing = false;
};

// ---- the side ------------------------------------------------------------------------------------------------------

class TacticalSideBase
{
public:
  TacticalSideBase(); // 0x5c7cb0
  virtual ~TacticalSideBase(); // 0x5c7a90 (scalar deleting destructor 0x5c7bc0)
  virtual int SideIndex() = 0; // slot 1: 0 or 1
  virtual TargetPriorityTree* BuildTargetList(); // 0x5c8e10 slot 2
  virtual int HasNoShipsAtAll(); // 0x5c7e80
  virtual int ResetEvaluators(); // 0x5c8d20
  virtual int Refresh(int a); // 0x5c7f70 slot 5
  virtual int UnreconstructedTacticalSideSlot06(); // 0x5c8300
  virtual int SplitIntoGroups(TargetPriorityTree* targets, int least, int most); // 0x5c83c0
  virtual int UnreconstructedTacticalSideSlot08(int a, int b, int c); // 0x5c8660

  int HasDisabledUnit(); // 0x5c7e40 (a fleet unit with +0x150 set)
  void RecordFleetPriorities(); // 0x5c81d0 notes the strength and sorts the fleet into the target lists
  float EvaluateAttack(int a, TacticalShipBase* unit); // 0x5c9190
  float InterceptTime(TacticalShipBase* unit, ShipGroup* group, float* total, float* speed); // 0x5c9160
  int EvaluateUnit4(TacticalShipBase* unit); // 0x5c91c0
  int EvaluateUnit3(ShipGroup* group); // 0x5c91e0
  void ReprioritiseTargets(float step); // 0x5c9200

  TurnSyncChannelA m_channelA; // +0x04
  TurnSyncChannelB m_channelB; // +0x0c
  int m_unk14; // +0x14 1
  int m_unk18; // +0x18
  int m_unk1c; // +0x1c
  int m_unk20; // +0x20
  float m_unk24; // +0x24 0.33
  int m_attackEvaluatorKind; // +0x28
  AttackEvaluatorBase* m_attackEvaluator; // +0x2c
  int m_formationEvaluatorKind; // +0x30
  FormationEvaluatorBase* m_formationEvaluator; // +0x34
  float m_previousStrength; // +0x38
  float m_strength; // +0x3c
  int m_unk40; // +0x40
  int m_unitCount; // +0x44
  int m_unk48; // +0x48
  int m_unk4c; // +0x4c
  float m_unk50; // +0x50, 1 / -1 in the simulator's two sides
  float m_unk54; // +0x54 -1
  UnitRefTree m_fleet; // +0x58 the side's units on the field
  UnitRefTree m_hangars; // +0x64 the side's units still in hangars
  GroupRefTree m_groups; // +0x70
  float m_unk7c; // +0x7c the side's fleet value (set by the facade)
  int m_unk80; // +0x80
  int m_state; // +0x84 5 while the side is withdrawing
  SideOrderTree m_orders; // +0x88
  TargetPriorityTree m_targets[4]; // +0x90 (+0x90 the primary list, then three by unit kind)
  int m_unkb0; // +0xb0
};

// The side of the headless simulator: slots 2-11 are the AI (the 3D sides keep the base implementations).
class TacticalAiSide : public TacticalSideBase
{
public:
  TargetPriorityTree* BuildTargetList() override; // 0x5d1a70
  int HasNoShipsAtAll() override; // 0x5d0f50
  int ResetEvaluators() override; // 0x5d1920
  int Refresh(int a) override; // 0x5d0ce0
  int UnreconstructedTacticalSideSlot06() override; // 0x5d1410
  int SplitIntoGroups(TargetPriorityTree* targets, int least, int most) override; // 0x5d1240
  int UnreconstructedTacticalSideSlot08(int a, int b, int c) override; // 0x5d1080
  virtual int RecordFleet(UnitRefTree* unused); // 0x5d14c0 slot 9
  virtual int StepSide(float step); // 0x5d14d0 slot 10
  virtual int ShouldWithdraw(); // 0x5d19a0
};
