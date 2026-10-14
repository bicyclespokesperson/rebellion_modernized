#pragma once
#include <cstdint>

#include "core/entry_identity.h"
#include "tactical/tactical_formation.h"
#include "tactical/tactical_lists.h"
#include "tactical/tactical_units.h"

// Ship groups of the tactical engine (ship-group module 0x59cd60-0x5a7500, tactical AI and formations 0x5c5b30-0x5d2360):
// the battle sides organise their capital ships (kind 4) and fighter squadrons (kind 3) into groups, give each group an
// order (a mode of the group's state machine) and a formation. Groups are registered in the TacticalRegistry.

class TacticalSideBase;

// The formation object of a group (tactical_formation.h).

// The embedded link a behavior object keeps (VT 0x66c678, 0x18 bytes: an AvlNode with three virtual functions).
class BehaviorLink : public AvlNode
{
public:
  BehaviorLink()
  {
  }
  ~BehaviorLink() override; // 0x5b8240 (scalar deleting destructor 0x5b8220)
  // The embedded AVL behavior-link hooks are supplied by the missing 3D behavior module.
  virtual void UnreconstructedBehaviorLinkSlot01(int a); // 0x5b7dc0
  virtual void UnreconstructedBehaviorLinkSlot02(); // 0x5b7e30
  virtual void UnreconstructedBehaviorLinkSlot03(); // 0x5b7e40
};

// What a group is currently doing about its target (0x3c bytes). The base class is abstract (VT 0x66ccf0); the one a
// ship group creates is ShipGroupBehavior (VT 0x66ccd0).
class ShipGroup;
class GroupBehavior
{
public:
  explicit GroupBehavior(ShipGroup* group); // 0x5cbd20
  virtual ~GroupBehavior(); // 0x5cbd90 (scalar deleting destructor 0x5cbd70)
  // The original 3D group-behavior implementation is absent; these hooks have no reconstructed caller or role.
  virtual void Unreconstructed3DBehaviorSlot01(int a) = 0;
  virtual void Unreconstructed3DBehaviorSlot02(int a) = 0;
  virtual void Unreconstructed3DBehaviorSlot03() = 0;
  virtual void Unreconstructed3DBehaviorSlot04(int a) = 0;
  virtual void Unreconstructed3DBehaviorSlot05(int a); // 0x5edd50
  virtual void Unreconstructed3DBehaviorSlot06(int a, int b); // 0x5eddf0
  virtual void Unreconstructed3DBehaviorSlot07(int a); // 0x5ede90

  int m_unk04; // +0x04
  ShipGroup* m_group; // +0x08
  float m_range; // +0x0c 5.0 by default
  int m_unk10; // +0x10
  int m_unk14; // +0x14
  BehaviorLink m_link; // +0x18
  int m_unk30; // +0x30
  float m_rangeNow; // +0x34 starts as m_range
  int m_unk38; // +0x38
};

class ShipGroupBehavior : public GroupBehavior
{
public:
  explicit ShipGroupBehavior(ShipGroup* group);
  ~ShipGroupBehavior() override;
  void Unreconstructed3DBehaviorSlot01(int a) override; // 0x5edf80
  void Unreconstructed3DBehaviorSlot02(int a) override; // 0x5ee250
  void Unreconstructed3DBehaviorSlot03() override; // 0x5ee280
  void Unreconstructed3DBehaviorSlot04(int a) override; // 0x5ee290
};

// ---- orders -------------------------------------------------------------------------------------------------------

// What a group is ordered to do (0x24 bytes). A group's mode (ShipGroup::SetMode) selects the class.
class GroupOrder
{
public:
  explicit GroupOrder(ShipGroup* group);
  virtual ~GroupOrder(); // 0x5cbd10 (scalar deleting destructor 0x5cbcf0)
  // 3D group-order implementations are absent. Slot 2 is called when a mode-3 order starts; the other slots have no
  // reconstructed callers or semantic role and preserve the original vtable positions.
  virtual void Unreconstructed3DOrderSlot01() = 0;
  virtual void BeginOrder() = 0;
  virtual int Unreconstructed3DOrderSlot03()
  {
    return 0;
  }
  virtual int Unreconstructed3DOrderSlot04()
  {
    return 0;
  }
  virtual int Unreconstructed3DOrderSlot05()
  {
    return 0;
  }
  virtual int Unreconstructed3DOrderSlot06()
  {
    return 0;
  }
  virtual int Unreconstructed3DOrderSlot07()
  {
    return 0;
  }
  virtual int Unreconstructed3DOrderSlot08()
  {
    return 0;
  }
  virtual int Unreconstructed3DOrderSlot09()
  {
    return 0;
  }
  virtual int Unreconstructed3DOrderSlot10()
  {
    return 0;
  }
  virtual int Unreconstructed3DOrderSlot11()
  {
    return 0;
  }

  int m_unk04; // +0x04
  ShipGroup* m_group; // +0x08
  int m_unk0c; // +0x0c
  int m_unk10; // +0x10
  int m_unk14; // +0x14
  TargetOrderTree m_targets; // +0x18
};

#define REB_DECLARE_GROUP_ORDER(Name) \
  class Name : public GroupOrder      \
  {                                   \
  public:                             \
    explicit Name(ShipGroup* group);  \
    virtual void Unreconstructed3DOrderSlot01() override \
    {                                 \
    }                                 \
    virtual void BeginOrder() override \
    {                                 \
    }                                 \
  }

REB_DECLARE_GROUP_ORDER(GroupOrderMode1);
REB_DECLARE_GROUP_ORDER(GroupOrderMode2);
REB_DECLARE_GROUP_ORDER(GroupOrderMode3);
REB_DECLARE_GROUP_ORDER(GroupOrderMode4);
REB_DECLARE_GROUP_ORDER(GroupOrderMode5);
REB_DECLARE_GROUP_ORDER(GroupOrderMode7);
REB_DECLARE_GROUP_ORDER(GroupOrderMode8);
REB_DECLARE_GROUP_ORDER(GroupOrderMode9);
REB_DECLARE_GROUP_ORDER(GroupOrderMode10);
REB_DECLARE_GROUP_ORDER(GroupOrderMode11);
#undef REB_DECLARE_GROUP_ORDER

// The order of mode 6 (0x84 bytes, constructor 0x5d0820) is not reconstructed.
GroupOrder* FUN_005d0820(void* memory, ShipGroup* group);

// ---- groups -------------------------------------------------------------------------------------------------------

// A group of ships of one side. It is a TacticalUnit (id = the group's handle) and, as a second base at +0x1c, the tree
// of the units that belong to it.
class ShipGroup : public TacticalUnit, public UnitRefTree
{
public:
  explicit ShipGroup(int seedHandle); // 0x5c9db0
  ~ShipGroup() override; // 0x5ca1e0 (scalar deleting destructor 0x5c9f90)
  virtual void SetSeedHandle(int handle); // 0x5ca1a0 slot 2
  // These hooks belong to the omitted 3D group/order implementation. Slot 21 is called immediately before member
  // removal in both fighter launch and auto-resolve group splitting.
  virtual void Unreconstructed3DGroupSlot03(int a); // 0x5c9bd0
  virtual void Unreconstructed3DGroupSlot04(int a); // 0x5c65e0
  virtual void Unreconstructed3DGroupSlot05(int a) = 0; // capital ships 0x5c6610, fighters 0x5cc300
  virtual void Unreconstructed3DGroupSlot06(int a, int b); // 0x5cb050
  virtual void Unreconstructed3DGroupSlot07(int a); // 0x5cb600
  virtual void Unreconstructed3DGroupSlot08(int a); // 0x5cb430
  virtual void Unreconstructed3DGroupSlot09(int a); // 0x5cb490
  virtual void AddMember(int handle); // 0x5cb3e0 slot 10
  virtual void Unreconstructed3DGroupSlot11(int a); // 0x5cb460
  virtual void RemoveMember(int handle); // 0x5cb510 slot 12
  virtual void Unreconstructed3DGroupSlot13(int a); // 0x5cb4d0
  virtual void Unreconstructed3DGroupSlot14(int a); // 0x5cb5b0
  virtual void Unreconstructed3DGroupSlot15(int a); // 0x5cb8e0
  virtual TacticalVector* GetPosition(TacticalVector* out); // 0x5cafa0 slot 16
  virtual void Unreconstructed3DGroupSlot17(); // 0x5cb990
  virtual void SetMode(int mode); // 0x5ca6d0 slot 18
  virtual void Unreconstructed3DGroupSlot19(int a); // 0x5cb770
  virtual void Unreconstructed3DGroupSlot20(int a); // 0x5cb7b0
  virtual void BeforeMemberRemoved(int handle); // 0x5cb870
  virtual void Unreconstructed3DGroupSlot22(int a); // 0x5cb840
  virtual void Unreconstructed3DGroupSlot23(int a); // 0x5cb7e0
  virtual void Unreconstructed3DGroupSlot24(int a); // 0x5cb810
  virtual void Unreconstructed3DGroupSlot25(int a); // 0x5ca530
  virtual void Unreconstructed3DGroupSlot26(int a); // 0x5c9940
  virtual void Unreconstructed3DGroupSlot27(int a); // 0x5c9780
  virtual void Unreconstructed3DGroupSlot28(int a); // 0x5c9ae0

  float MemberStrengthAgainst(TacticalShipBase* target); // 0x5cb650

  ShipGroup(int seedHandle, int id); // 0x5c9fb0 the variant that takes the group's id

  TurnSyncChannelA m_channelA; // +0x28
  TurnSyncChannelB m_channelB; // +0x30
  int m_unk38; // +0x38
  int m_unk3c; // +0x3c
  GroupBehavior* m_behavior; // +0x40
  UnitRefTree m_founders; // +0x44 the unit the group was created for
  int m_side; // +0x50 the side of the seed unit
  FormationBase* m_formation; // +0x54
  int m_unk58; // +0x58
  int m_unk5c; // +0x5c
  GroupOrder* m_order; // +0x60
  int m_mode; // +0x64 see SetMode
  int m_unk68; // +0x68
  int m_firstMember; // +0x6c handle of the first member
  int m_seedHandle; // +0x70
  TargetOrderTree m_targetsA; // +0x74
  TargetOrderTree m_targetsB; // +0x80
  FormationOrderTree m_formationA; // +0x8c
  FormationOrderTree m_formationB; // +0x98
  FormationOrderTree m_formationC; // +0xa4
  int m_unkb0; // +0xb0
  int m_unkb4; // +0xb4
  int m_unkb8; // +0xb8
};

class CapitalShipGroup : public ShipGroup
{
public:
  explicit CapitalShipGroup(int seedHandle); // 0x5c62b0
  ~CapitalShipGroup() override; // scalar deleting destructor 0x5a6710
  int GetUnitKind() override; // 0x5a6700 returns 4
  void Unreconstructed3DGroupSlot05(int a) override; // 0x5c6610
  void Unreconstructed3DGroupSlot06(int a, int b) override; // 0x5c7060
  void SetMode(int mode) override; // 0x5c7130
  void Unreconstructed3DGroupSlot26(int a) override; // 0x5c6270
  void Unreconstructed3DGroupSlot27(int a) override; // 0x5c6230

  void SetRole(int role); // 0x5c7140

  int m_role; // +0xbc 4 until the simulator names one
  int m_unkc0; // +0xc0
};

class FighterGroup : public ShipGroup
{
public:
  explicit FighterGroup(int seedHandle); // 0x5cbf50
  ~FighterGroup() override; // scalar deleting destructor 0x5a6740
  int GetUnitKind() override; // 0x5a6730 returns 3
  void Unreconstructed3DGroupSlot05(int a) override; // 0x5cc300
};

// The group an id-tree node of the registry stands for (a group starts with its TacticalUnit, which is the node).
inline ShipGroup* ShipGroupOf(IdNode* node)
{
  return static_cast<ShipGroup*>(static_cast<TacticalUnit*>(node));
}

// Gives a new group a handle for its side and registers it (0x5afde0 a fighter group, 0x5afe10 a capital-ship group).
void RegisterFighterGroup(FighterGroup* group);
void RegisterCapitalShipGroup(CapitalShipGroup* group);

// Removes a unit from its group (a null group is ignored); 0x5caed0.
void RemoveMemberFromGroup(ShipGroup* group, int handle);
