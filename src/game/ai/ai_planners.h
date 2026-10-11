#pragma once
#include "ai/ai_planner.h"

// The concrete mission planners, one class per (mission kind, subtype) that AiPlannerSet::CreatePlanner builds. The mission
// kind is the family byte of AiMissionPlanner::m_missionKey (named after the mission classes of src/game/mission).
//
// Free planners (derive from AiFreePlanner, 0x70 bytes unless noted) choose leaders (Prepare, slot 18), a target
// (ChooseTarget, slot 20) and gather the team there; the system planners (AiSystemPlanner) work for an AiSystemRecord.
// Slots 1 and 2 of every planner are forwarders to the base class Read/Write (0x4831d0/0x4820b0).

// 0x53 Research, subtype 1 (the shipyard research mission): VT 0x65a510
class AiResearchPlanner1 : public AiFreePlanner
{
public:
  AiResearchPlanner1(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x47bba0
  ~AiResearchPlanner1() override; // 0: 0x47bb80 (deleting), 0x47bc20
  int GetSubtype() override
  {
    return 1;
  } // 5: 0x40f340
  int Virtual06() override; // 6: 0x47bc30
  int GetStallLimit() override
  {
    return 1000;
  } // 9: 0x480df0
  void Virtual12(int event) override
  {
    HandleEvent(event);
  } // 12: 0x47c890
  int Virtual13() override; // 13: 0x47c110
  int Virtual16() override; // 16: 0x479ac0
  int Virtual17() override
  {
    return 0;
  } // 17: 0x6158b0
  void Prepare() override; // 18: 0x47bcd0
  void Virtual19() override
  {
    MarkMembersAssigned();
  } // 19: 0x481100
  int ChooseTarget() override; // 20: 0x47be50
};

// 0x53 Research, subtype 2: VT 0x65a568
class AiResearchPlanner2 : public AiFreePlanner
{
public:
  AiResearchPlanner2(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x47c170
  ~AiResearchPlanner2() override; // 0: 0x47c150 (deleting), 0x47c1f0
  int GetSubtype() override
  {
    return 2;
  } // 5: 0x43c1f0
  int Virtual06() override; // 6: 0x47c210
  int GetStallLimit() override
  {
    return 1000;
  } // 9: 0x480df0
  void Virtual12(int event) override
  {
    HandleEvent(event);
  } // 12: 0x47c890
  int Virtual13() override; // 13: 0x47c6f0
  int Virtual16() override; // 16: 0x47c200
  int Virtual17() override
  {
    return 0;
  } // 17: 0x6158b0
  void Prepare() override; // 18: 0x47c2b0
  void Virtual19() override
  {
    MarkMembersAssigned();
  } // 19: 0x481100
  int ChooseTarget() override; // 20: 0x47c430
};

// 0x53 Research, subtype 3: VT 0x65a5c0
class AiResearchPlanner3 : public AiFreePlanner
{
public:
  AiResearchPlanner3(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x47c750
  ~AiResearchPlanner3() override; // 0: 0x47c730 (deleting), 0x47c7d0
  int GetSubtype() override
  {
    return 3;
  } // 5: 0x526f00
  int Virtual06() override; // 6: 0x47c7f0
  int GetStallLimit() override
  {
    return 1000;
  } // 9: 0x480df0
  void Virtual12(int event) override
  {
    HandleEvent(event);
  } // 12: 0x47c890
  int Virtual13() override; // 13: 0x47cd40
  int Virtual16() override; // 16: 0x47c7e0
  int Virtual17() override
  {
    return 0;
  } // 17: 0x6158b0
  void Prepare() override; // 18: 0x47c900
  void Virtual19() override
  {
    MarkMembersAssigned();
  } // 19: 0x481100
  int ChooseTarget() override; // 20: 0x47ca80
};

// 0x57 Subdue: VT 0x65a618
class AiSubduePlanner : public AiFreePlanner
{
public:
  AiSubduePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x47cda0
  ~AiSubduePlanner() override; // 0: 0x47cd80 (deleting), 0x47ce20
  int GetSubtype() override
  {
    return 0;
  } // 5: 0x6158b0
  int Virtual06() override; // 6: 0x47ce40
  int GetStallLimit() override
  {
    return 5;
  } // 9: 0x404c20
  void Virtual12(int event) override
  {
    HandleEvent(event);
  } // 12: 0x47c890
  int Virtual13() override; // 13: 0x47d320
  int Virtual16() override; // 16: 0x47ce30
  int Virtual17() override
  {
    return 0;
  } // 17: 0x6158b0
  void Prepare() override; // 18: 0x47cee0
  void Virtual19() override
  {
    MarkMembersAssigned();
  } // 19: 0x481100
  int ChooseTarget() override; // 20: 0x47d060
};

// 0x55 Recruit: VT 0x65a6b8
class AiRecruitPlanner : public AiFreePlanner
{
public:
  AiRecruitPlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x47d9a0
  ~AiRecruitPlanner() override; // 0: 0x47d980 (deleting), 0x47da20
  int GetSubtype() override
  {
    return 0;
  } // 5: 0x6158b0
  int Virtual06() override; // 6: 0x47da30
  int GetStallLimit() override
  {
    return 1000;
  } // 9: 0x480df0
  void Virtual12(int event) override; // 12: 0x47dad0
  int Virtual16() override
  {
    return 0;
  } // 16: 0x47a470
  int Virtual17() override
  {
    return 0;
  } // 17: 0x6158b0
  void Prepare() override; // 18: 0x47db40
  void Virtual19() override
  {
    MarkMembersAssigned();
  } // 19: 0x481100
  int ChooseTarget() override; // 20: 0x47dcc0
};

// 0x56 Incite: VT 0x65a808
class AiIncitePlanner : public AiFreePlanner
{
public:
  AiIncitePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x47fe50
  ~AiIncitePlanner() override; // 0: 0x47fe30 (deleting), 0x47fed0
  int GetSubtype() override
  {
    return 0;
  } // 5: 0x6158b0
  int Virtual06() override; // 6: 0x47fef0
  int GetStallLimit() override
  {
    return 10;
  } // 9: 0x4820c0
  void Virtual12(int event) override
  {
    HandleEvent(event);
  } // 12: 0x47c890
  int Virtual13() override; // 13: 0x4806d0
  int Virtual16() override
  {
    return 12;
  } // 16: 0x47fee0
  int Virtual17() override
  {
    return 4;
  } // 17: 0x4047d0
  void Prepare() override; // 18: 0x47ff90
  void Virtual19() override; // 19: 0x480110
  int ChooseTarget() override; // 20: 0x480370
};

// The planners that work for an AiSystemRecord (AiSystemPlanner, 0x7c bytes). Slot 16 chooses the leaders, slot 17 the members.

// 0x69 Sabotage: VT 0x65a670
class AiSabotagePlanner : public AiSystemPlanner
{
public:
  AiSabotagePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x47d380
  ~AiSabotagePlanner() override; // 0: 0x47d360 (deleting), 0x47d400
  int GetSubtype() override
  {
    return 0;
  } // 5: 0x6158b0
  int Virtual06() override; // 6: 0x47d430
  int GetStallLimit() override
  {
    return 10;
  } // 9: 0x4820c0
  void Virtual12(int event) override
  {
    HandleQuietEvent(event);
  } // 12: 0x47d4d0
  int Virtual16() override
  {
    PickLeaders();
    return 0;
  } // 16: 0x47d520
  int Virtual17() override
  {
    PickMembers();
    return 0;
  } // 17: 0x47d720

  void PickLeaders(); // 0x47d520
  void PickMembers(); // 0x47d720
};

// 0x54 (system planner): VT 0x65a768
class AiMission54Planner : public AiSystemPlanner
{
public:
  AiMission54Planner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x47f190
  ~AiMission54Planner() override; // 0: 0x47f170 (deleting), 0x47f210
  int GetSubtype() override
  {
    return 0;
  } // 5: 0x6158b0
  int Virtual06() override; // 6: 0x47f220
  int GetStallLimit() override
  {
    return 1000;
  } // 9: 0x480df0
  void Virtual12(int event) override; // 12: 0x47f2c0
  int Virtual16() override
  {
    PickLeaders();
    return 0;
  } // 16: 0x47f320
  int Virtual17() override
  {
    AssignNoMembers();
    return 0;
  } // 17: 0x47f500

  void PickLeaders(); // 0x47f320
};

// 0x52 Espionage (system planner): VT 0x65a860
class AiEspionagePlanner : public AiSystemPlanner
{
public:
  AiEspionagePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x480730
  ~AiEspionagePlanner() override; // 0: 0x480710 (deleting), 0x4807b0
  int GetSubtype() override
  {
    return 0;
  } // 5: 0x6158b0
  int Virtual06() override; // 6: 0x4807c0
  int GetStallLimit() override
  {
    return 10;
  } // 9: 0x4820c0
  void Virtual12(int event) override; // 12: 0x480860
  int Virtual16() override
  {
    PickLeaders();
    return 0;
  } // 16: 0x4808c0
  int Virtual17() override
  {
    PickMembers();
    return 0;
  } // 17: 0x480aa0

  void PickLeaders(); // 0x4808c0
  void PickMembers(); // 0x480aa0
};

// 0x62 Abduction (Alliance) / 0x63 Assassination (Empire), the system planner: VT 0x65a9b0, 0x80 bytes
class AiAbductionPlanner : public AiSystemPlanner
{
public:
  AiAbductionPlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x482b30
  ~AiAbductionPlanner() override; // 0: 0x482b10 (deleting), 0x482bf0
  void Read(istream* in) override; // 1: 0x482c00
  void Write(ostream* out) override; // 2: 0x482c20
  int GetSubtype() override
  {
    return 4;
  } // 5: 0x4047d0
  int Virtual06() override; // 6: 0x482c40
  int GetStallLimit() override
  {
    return 5;
  } // 9: 0x404c20
  void Virtual12(int event) override
  {
    HandleQuietEvent(event);
  } // 12: 0x47d4d0
  int Virtual16() override
  {
    PickLeaders();
    return 0;
  } // 16: 0x482ce0
  int Virtual17() override
  {
    PickMembers();
    return 0;
  } // 17: 0x482ec0

  uint32_t m_capability; // +0x7c the character capability the mission needs

  void PickLeaders(); // 0x482ce0
  void PickMembers(); // 0x482ec0
};

// 0x6a Death Star sabotage: VT 0x65a958
class AiDsSabotagePlanner : public AiFreePlanner
{
public:
  AiDsSabotagePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x482020
  ~AiDsSabotagePlanner() override; // 0: 0x482000 (deleting), 0x4820a0
  int GetSubtype() override
  {
    return 0;
  } // 5: 0x6158b0
  int Virtual06() override; // 6: 0x4820e0
  int GetStallLimit() override
  {
    return 10;
  } // 9: 0x4820c0
  void Virtual12(int event) override
  {
    HandleQuietEvent(event);
  } // 12: 0x47d4d0
  int Virtual16() override
  {
    return 5;
  } // 16: 0x4820d0
  int Virtual17() override
  {
    return 4;
  } // 17: 0x4047d0
  void Prepare() override; // 18: 0x482180
  void Virtual19() override; // 19: 0x482330
  int ChooseTarget() override; // 20: 0x482560

  void FindTargetShip(); // 0x482910
};

// 0x61 Rescue (free planner that hunts for a character): VT 0x65a710
class AiRescuePlanner : public AiFreePlanner
{
public:
  AiRescuePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x47e130
  ~AiRescuePlanner() override; // 0: 0x47e110 (deleting), 0x47e1b0
  int GetSubtype() override
  {
    return 0;
  } // 5: 0x6158b0
  int Virtual06() override; // 6: 0x47e1d0
  int GetStallLimit() override
  {
    return 10;
  } // 9: 0x4820c0
  void Virtual12(int event) override
  {
    HandleQuietEvent(event);
  } // 12: 0x47d4d0
  int Virtual16() override
  {
    return 8;
  } // 16: 0x47e1c0
  int Virtual17() override
  {
    return 4;
  } // 17: 0x4047d0
  void Prepare() override; // 18: 0x47e270
  void Virtual19() override; // 19: 0x47e3f0
  int ChooseTarget() override; // 20: 0x47e620

  void PickCandidate(); // 0x47ecf0
};

// 0x62 free planner (every subtype but 4, which AiAbductionPlanner handles): VT 0x65a9f8
class AiMission62Planner : public AiFreePlanner
{
public:
  AiMission62Planner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x483140
  ~AiMission62Planner() override; // 0: 0x483120 (deleting), 0x4831c0
  int GetSubtype() override
  {
    return 0;
  } // 5: 0x6158b0
  int Virtual06() override; // 6: 0x4831f0
  int GetStallLimit() override
  {
    return 10;
  } // 9: 0x4820c0
  void Virtual12(int event) override
  {
    HandleQuietEvent(event);
  } // 12: 0x47d4d0
  int Virtual16() override
  {
    return 6;
  } // 16: 0x4831e0
  int Virtual17() override
  {
    return 4;
  } // 17: 0x4047d0
  void Prepare() override; // 18: 0x483290
  void Virtual19() override; // 19: 0x483410
  int ChooseTarget() override; // 20: 0x483640

  void PickCandidate(); // 0x483d10
};

// 0x51 Diplomacy: VT 0x65a900
class AiDiplomacyPlanner : public AiFreePlanner
{
public:
  AiDiplomacyPlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x481460
  ~AiDiplomacyPlanner() override; // 0: 0x481440 (deleting), 0x4814e0
  int GetSubtype() override
  {
    return 0;
  } // 5: 0x6158b0
  int Virtual06() override; // 6: 0x481500
  int GetStallLimit() override
  {
    return 5;
  } // 9: 0x404c20
  void Virtual12(int event) override
  {
    HandleEvent(event);
  } // 12: 0x47c890
  int Virtual13() override; // 13: 0x481eb0
  void OnFinished() override; // 14: 0x481fe0
  int CanUse(const ObjectKey* character) override; // 15: 0x481ef0
  int Virtual16() override
  {
    return 0xb;
  } // 16: 0x4814f0
  int Virtual17() override
  {
    return 0;
  } // 17: 0x6158b0
  void Prepare() override; // 18: 0x4815a0
  void Virtual19() override
  {
    AssignNoMembers();
  } // 19: 0x47f500
  int ChooseTarget() override; // 20: 0x481720
};

// 0x54 system chooser, subtype 5 (a free planner that remembers the sector it works in): VT 0x65a7b0
class AiMission54State5Planner : public AiFreePlanner
{
public:
  AiMission54State5Planner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x47f550
  ~AiMission54State5Planner() override; // 0: 0x47f530 (deleting), 0x47f5e0
  void Read(istream* in) override; // 1: 0x47f630 (shared with AiEspionageState6Planner)
  void Write(ostream* out) override; // 2: 0x47f650 (shared with AiEspionageState6Planner)
  int GetSubtype() override
  {
    return 5;
  } // 5: 0x404c20
  int Virtual06() override; // 6: 0x47f680
  int GetStallLimit() override
  {
    return 1000;
  } // 9: 0x480df0
  void Virtual12(int event) override; // 12: 0x47f720
  void OnFinished() override; // 14: 0x47fe10
  int Virtual16() override
  {
    return 9;
  } // 16: 0x47f670
  int Virtual17() override
  {
    return 0;
  } // 17: 0x6158b0
  void Prepare() override; // 18: 0x47f790
  void Virtual19() override
  {
    MarkMembersAssigned();
  } // 19: 0x481100
  int ChooseTarget() override; // 20: 0x47f970

  ObjectKey m_sectorKey; // +0x70 the sector of m_x34 (flagged busy while the mission runs)
};

// 0x52 Espionage, subtype 6: VT 0x65a8a8
class AiEspionageState6Planner : public AiFreePlanner
{
public:
  AiEspionageState6Planner(int view, AiSideController* owner, AiCharacterRecordSet* characters); // 0x480d10
  ~AiEspionageState6Planner() override; // 0: 0x480cf0 (deleting), 0x480da0
  void Read(istream* in) override; // 1: 0x47f630 (the code AiMission54State5Planner::Read has)
  void Write(ostream* out) override; // 2: 0x47f650
  int GetSubtype() override
  {
    return 6;
  } // 5: 0x4953a0
  int Virtual06() override; // 6: 0x480e10
  int GetStallLimit() override
  {
    return 1000;
  } // 9: 0x480df0
  void Virtual12(int event) override; // 12: 0x480eb0
  void OnFinished() override; // 14: 0x481420
  int Virtual16() override
  {
    return 10;
  } // 16: 0x480e00
  int Virtual17() override
  {
    return 0;
  } // 17: 0x6158b0
  void Prepare() override; // 18: 0x480f30
  void Virtual19() override
  {
    MarkMembersAssigned();
  } // 19: 0x481100
  int ChooseTarget() override; // 20: 0x481110

  ObjectKey m_sectorKey; // +0x70
};
