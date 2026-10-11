#pragma once
#include <cstdint>

#include "ai/ai_selector.h"
#include "core/entry_identity.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// The AI's mission planners (0x47aae0-0x484180, 0x4bce50-0x4be580). A planner gathers characters (leaders and members, both
// sets of character keys) for one kind of mission (the family byte of m_missionKey: 0x51 diplomacy ... 0x6a), moves them to the
// place the mission needs and finally orders the mission (order 0x240). AiPlannerSet creates them (CreatePlanner).
//
//   AiMissionPlanner  VT 0x65a4d0, 0x70 bytes: the state every planner has (teams, flags, selector); slots 0-15
//   AiFreePlanner     VT 0x65c3b0, 0x70: planners that choose their own destination; Step has 8 states
//   AiSystemPlanner   VT 0x65c408, 0x7c: planners that work for an AiSystemRecord (the record's id, its sector, its tier)
//
// m_flags: 0x1 leaders assigned, 0x2 members assigned, 0x4 destination chosen, 0x8 someone to move, 0x10 destination reached,
// 0x40 moving, 0x80 stalled, 0x100 teams usable, 0x10000000 finished, 0x20000000 abort, 0x40000000 abort requested,
// 0x80000000 mission object changed.

class AiSideController;
class AiCharacterRecordSet;
class AiSystemEval;
class OrderBase;

class AiMissionPlanner : public IdNode
{
public:
  AiMissionPlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x47aae0
  ~AiMissionPlanner() override; // 0: 0x47aac0 (deleting), 0x47abd0
  virtual void Read(istream* in); // 1: 0x47acb0
  virtual void Write(ostream* out); // 2: 0x47ad50
  virtual void Virtual03(int event); // 3: 0x47b0d0: events 3-10 reset the mission mark
  virtual int Virtual04() = 0; // 4: the planner's role (1 works for an AiSystemRecord, 2 free)
  virtual int GetSubtype() = 0; // 5: the mission subtype
  virtual int Virtual06() = 0; // 6: priority
  virtual void SetTier(int tier) = 0; // 7: the tier of the AiSystemRecord the planner works for
  virtual int GetTier() = 0; // 8
  virtual int GetStallLimit() = 0; // 9: steps without progress before the planner is marked stalled
  virtual int IsActive() = 0; // 10
  virtual OrderBase* Step(int* done) = 0; // 11: one step of the planner's state machine
  virtual void Virtual12(int argument) = 0; // 12
  virtual int Virtual13(); // 13: 0x6158b0 returns 0
  virtual void OnFinished(); // 14: 0x619730 (empty)
  virtual int CanUse(const ObjectKey* character); // 15: 0x51ebb0 returns 1
  virtual int Virtual16() = 0; // 16: the kind index of free planners (CountPlanner), a step of system planners
  virtual int Virtual17() = 0; // 17

  void SetMissionObject(const ObjectKey* key); // 0x47adf0
  int GetKindIndex()
  {
    return Virtual16();
  }
  int AdvanceStall(); // 0x47ae20: 1 once GetStallLimit() steps passed without progress
  void ReleaseUnfit(); // 0x47ae50: drops the characters that cannot be used any more
  void Finish(); // 0x47aff0
  void RefreshTeams(); // 0x47b110
  OrderBase* AbortMission(); // 0x47b250: order 0x250 for the running mission
  OrderBase* StartMission(); // 0x47b360: order 0x240 with the teams
  int AddLeader(const ObjectKey* character); // 0x47b610
  int AddMember(const ObjectKey* character); // 0x47b730
  int RemoveLeader(const ObjectKey* character); // 0x47b870
  int RemoveMember(const ObjectKey* character); // 0x47b8e0
  void ResetMissionMark(); // 0x47b950
  void RemoveFirstMember(); // 0x47b9e0
  int IsUsable(const ObjectKey* character); // 0x47ba40
  int IsReachable(const ObjectKey* character); // 0x47ba90
  // Virtual12 of most free planners: 0 marks the mission object changed, 2 and 3 are progress notices (0x47c890).
  void HandleEvent(int event);
  void MarkMembersAssigned(); // 0x481100: flag 2
  void HandleQuietEvent(int event); // 0x47d4d0: like HandleEvent without the abort flags
  void AssignNoMembers(); // 0x47f500: flag 2, drops a member

  int m_state; // +0x1c
  PersistentInt m_flags; // +0x20
  int m_view; // +0x24
  ObjectKey m_missionObject; // +0x28 the mission being carried out
  // The mission kind (+0x2c): the family byte is the mission kind (0x51-0x6a). The base constructor leaves it alone (a union
  // keeps the compiler from running ObjectKey's constructor); every concrete planner sets it.
  union
  {
    ObjectKey m_missionKey;
  };
  ObjectKey m_target; // +0x30 where the mission takes place
  ObjectKey m_x34; // +0x34
  ObjectKey m_destination; // +0x38 the system the teams gather at
  ObjectKeySet m_leaders; // +0x3c
  ObjectKeySet m_members; // +0x44
  ObjectKeySet m_finished; // +0x4c missions already handled
  AiRankedSelector m_selector; // +0x54
  AiCharacterRecordSet* m_characters; // +0x64
  AiSideController* m_owner; // +0x68
  int m_stall; // +0x6c
};

class AiFreePlanner : public AiMissionPlanner
{
public:
  AiFreePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x4bce70
  ~AiFreePlanner() override; // 0: 0x4bce50 (deleting), 0x4bcea0
  void Read(istream* in) override; // 1: 0x4bceb0 (forwarder)
  void Write(ostream* out) override; // 2: 0x4bcec0 (forwarder)
  int Virtual04() override
  {
    return 2;
  } // 4: 0x4e8740
  void SetTier(int) override
  {
  } // 7: 0x45d3f0
  int GetTier() override
  {
    return 0;
  } // 8: 0x5a5b60
  int IsActive() override; // 10: 0x4bda40
  OrderBase* Step(int* done) override; // 11: 0x4bced0
  virtual void Prepare() = 0; // 18: chooses the leaders
  virtual void Virtual19() = 0; // 19: usually MarkMembersAssigned
  virtual int ChooseTarget() = 0; // 20: chooses the target of the mission

  int CountMembersAtDestination(); // 0x4bd000
  int ChooseDestination(); // 0x4bd0a0
  OrderBase* MoveTeams(); // 0x4bd570
};

class AiSystemPlanner : public AiMissionPlanner
{
public:
  AiSystemPlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x4bd810
  ~AiSystemPlanner() override; // 0: 0x4bd7f0 (deleting), 0x4bd870
  void Read(istream* in) override; // 1: 0x4bd8c0
  void Write(ostream* out) override; // 2: 0x4bd900
  int Virtual04() override
  {
    return 1;
  } // 4: 0x5a66f0
  void SetTier(int tier) override; // 7: 0x4bd940
  int GetTier() override
  {
    return m_tier;
  } // 8: 0x47d970
  int IsActive() override; // 10: 0x4bda40
  OrderBase* Step(int* done) override; // 11: 0x4bda60

  int FetchOrderTargets(); // 0x4bdbc0
  int ChooseSystem(); // 0x4bdcd0
  OrderBase* MoveTeams(); // 0x4be230
  void Disband(); // 0x4be4c0

  int m_systemRecordId; // +0x70 id of the AiSystemRecord the planner works for
  ObjectKey m_sectorKey; // +0x74
  int m_tier; // +0x78
};
