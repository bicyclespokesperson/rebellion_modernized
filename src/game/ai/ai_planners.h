#pragma once
#include "ai/ai_planner.h"

// The concrete mission planners, one class per (mission kind, subtype) that AiPlannerSet::CreatePlanner builds. The mission
// kind is the family byte of AiMissionPlanner::m_missionKey (named after the mission classes of src/game/mission).
//
// Free planners (derive from AiFreePlanner, 0x70 bytes unless noted) choose leaders (Prepare, slot 18), a target
// (ChooseTarget, slot 20) and gather the team there; the system planners (AiSystemPlanner) work for an AiSystemRecord.
// Slots 1 and 2 of every planner are forwarders to the base class Read/Write (0x4831d0/0x4820b0).

// 0x53 Research, subtype 1 (the shipyard research mission): VT 0x65a510
class AiResearchPlanner1 : public AiFreePlanner {
public:
    AiResearchPlanner1(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x47bba0
    virtual ~AiResearchPlanner1();                   // 0: 0x47bb80 (deleting), 0x47bc20
    virtual int GetSubtype() override { return 1; }  // 5: 0x40f340
    virtual int Virtual06() override;                // 6: 0x47bc30
    virtual int GetStallLimit() override { return 1000; }  // 9: 0x480df0
    virtual void Virtual12(int event) override { HandleEvent(event); }  // 12: 0x47c890
    virtual int Virtual13() override;                // 13: 0x47c110
    virtual int Virtual16() override;                // 16: 0x479ac0
    virtual int Virtual17() override { return 0; }  // 17: 0x6158b0
    virtual void Prepare() override;                 // 18: 0x47bcd0
    virtual void Virtual19() override { MarkMembersAssigned(); }  // 19: 0x481100
    virtual int ChooseTarget() override;             // 20: 0x47be50
};

// 0x53 Research, subtype 2: VT 0x65a568
class AiResearchPlanner2 : public AiFreePlanner {
public:
    AiResearchPlanner2(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x47c170
    virtual ~AiResearchPlanner2();                   // 0: 0x47c150 (deleting), 0x47c1f0
    virtual int GetSubtype() override { return 2; }  // 5: 0x43c1f0
    virtual int Virtual06() override;                // 6: 0x47c210
    virtual int GetStallLimit() override { return 1000; }  // 9: 0x480df0
    virtual void Virtual12(int event) override { HandleEvent(event); }  // 12: 0x47c890
    virtual int Virtual13() override;                // 13: 0x47c6f0
    virtual int Virtual16() override;                // 16: 0x47c200
    virtual int Virtual17() override { return 0; }  // 17: 0x6158b0
    virtual void Prepare() override;                 // 18: 0x47c2b0
    virtual void Virtual19() override { MarkMembersAssigned(); }  // 19: 0x481100
    virtual int ChooseTarget() override;             // 20: 0x47c430
};

// 0x53 Research, subtype 3: VT 0x65a5c0
class AiResearchPlanner3 : public AiFreePlanner {
public:
    AiResearchPlanner3(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x47c750
    virtual ~AiResearchPlanner3();                   // 0: 0x47c730 (deleting), 0x47c7d0
    virtual int GetSubtype() override { return 3; }  // 5: 0x526f00
    virtual int Virtual06() override;                // 6: 0x47c7f0
    virtual int GetStallLimit() override { return 1000; }  // 9: 0x480df0
    virtual void Virtual12(int event) override { HandleEvent(event); }  // 12: 0x47c890
    virtual int Virtual13() override;                // 13: 0x47cd40
    virtual int Virtual16() override;                // 16: 0x47c7e0
    virtual int Virtual17() override { return 0; }  // 17: 0x6158b0
    virtual void Prepare() override;                 // 18: 0x47c900
    virtual void Virtual19() override { MarkMembersAssigned(); }  // 19: 0x481100
    virtual int ChooseTarget() override;             // 20: 0x47ca80
};

// 0x57 Subdue: VT 0x65a618
class AiSubduePlanner : public AiFreePlanner {
public:
    AiSubduePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x47cda0
    virtual ~AiSubduePlanner();                      // 0: 0x47cd80 (deleting), 0x47ce20
    virtual int GetSubtype() override { return 0; }  // 5: 0x6158b0
    virtual int Virtual06() override;                // 6: 0x47ce40
    virtual int GetStallLimit() override { return 5; }  // 9: 0x404c20
    virtual void Virtual12(int event) override { HandleEvent(event); }  // 12: 0x47c890
    virtual int Virtual13() override;                // 13: 0x47d320
    virtual int Virtual16() override;                // 16: 0x47ce30
    virtual int Virtual17() override { return 0; }  // 17: 0x6158b0
    virtual void Prepare() override;                 // 18: 0x47cee0
    virtual void Virtual19() override { MarkMembersAssigned(); }  // 19: 0x481100
    virtual int ChooseTarget() override;             // 20: 0x47d060
};

// 0x55 Recruit: VT 0x65a6b8
class AiRecruitPlanner : public AiFreePlanner {
public:
    AiRecruitPlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x47d9a0
    virtual ~AiRecruitPlanner();                     // 0: 0x47d980 (deleting), 0x47da20
    virtual int GetSubtype() override { return 0; }  // 5: 0x6158b0
    virtual int Virtual06() override;                // 6: 0x47da30
    virtual int GetStallLimit() override { return 1000; }  // 9: 0x480df0
    virtual void Virtual12(int event) override;      // 12: 0x47dad0
    virtual int Virtual16() override { return 0; }   // 16: 0x47a470
    virtual int Virtual17() override { return 0; }   // 17: 0x6158b0
    virtual void Prepare() override;                 // 18: 0x47db40
    virtual void Virtual19() override { MarkMembersAssigned(); }  // 19: 0x481100
    virtual int ChooseTarget() override;             // 20: 0x47dcc0
};

// 0x56 Incite: VT 0x65a808
class AiIncitePlanner : public AiFreePlanner {
public:
    AiIncitePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x47fe50
    virtual ~AiIncitePlanner();                      // 0: 0x47fe30 (deleting), 0x47fed0
    virtual int GetSubtype() override { return 0; }  // 5: 0x6158b0
    virtual int Virtual06() override;                // 6: 0x47fef0
    virtual int GetStallLimit() override { return 10; }  // 9: 0x4820c0
    virtual void Virtual12(int event) override { HandleEvent(event); }  // 12: 0x47c890
    virtual int Virtual13() override;                // 13: 0x4806d0
    virtual int Virtual16() override { return 12; }  // 16: 0x47fee0
    virtual int Virtual17() override { return 4; }   // 17: 0x4047d0
    virtual void Prepare() override;                 // 18: 0x47ff90
    virtual void Virtual19() override;               // 19: 0x480110
    virtual int ChooseTarget() override;             // 20: 0x480370
};

// The planners that work for an AiSystemRecord (AiSystemPlanner, 0x7c bytes). Slot 16 chooses the leaders, slot 17 the members.

// 0x69 Sabotage: VT 0x65a670
class AiSabotagePlanner : public AiSystemPlanner {
public:
    AiSabotagePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x47d380
    virtual ~AiSabotagePlanner();                    // 0: 0x47d360 (deleting), 0x47d400
    virtual int GetSubtype() override { return 0; }  // 5: 0x6158b0
    virtual int Virtual06() override;                // 6: 0x47d430
    virtual int GetStallLimit() override { return 10; }  // 9: 0x4820c0
    virtual void Virtual12(int event) override { HandleQuietEvent(event); }  // 12: 0x47d4d0
    virtual int Virtual16() override { PickLeaders(); return 0; }  // 16: 0x47d520
    virtual int Virtual17() override { PickMembers(); return 0; }  // 17: 0x47d720

    void PickLeaders();  // 0x47d520
    void PickMembers();  // 0x47d720
};

// 0x54 (system planner): VT 0x65a768
class AiMission54Planner : public AiSystemPlanner {
public:
    AiMission54Planner(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x47f190
    virtual ~AiMission54Planner();                   // 0: 0x47f170 (deleting), 0x47f210
    virtual int GetSubtype() override { return 0; }  // 5: 0x6158b0
    virtual int Virtual06() override;                // 6: 0x47f220
    virtual int GetStallLimit() override { return 1000; }  // 9: 0x480df0
    virtual void Virtual12(int event) override;      // 12: 0x47f2c0
    virtual int Virtual16() override { PickLeaders(); return 0; }  // 16: 0x47f320
    virtual int Virtual17() override { AssignNoMembers(); return 0; }  // 17: 0x47f500

    void PickLeaders();  // 0x47f320
};

// 0x52 Espionage (system planner): VT 0x65a860
class AiEspionagePlanner : public AiSystemPlanner {
public:
    AiEspionagePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x480730
    virtual ~AiEspionagePlanner();                   // 0: 0x480710 (deleting), 0x4807b0
    virtual int GetSubtype() override { return 0; }  // 5: 0x6158b0
    virtual int Virtual06() override;                // 6: 0x4807c0
    virtual int GetStallLimit() override { return 10; }  // 9: 0x4820c0
    virtual void Virtual12(int event) override;      // 12: 0x480860
    virtual int Virtual16() override { PickLeaders(); return 0; }  // 16: 0x4808c0
    virtual int Virtual17() override { PickMembers(); return 0; }  // 17: 0x480aa0

    void PickLeaders();  // 0x4808c0
    void PickMembers();  // 0x480aa0
};

// 0x62 Abduction (Alliance) / 0x63 Assassination (Empire), the system planner: VT 0x65a9b0, 0x80 bytes
class AiAbductionPlanner : public AiSystemPlanner {
public:
    AiAbductionPlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x482b30
    virtual ~AiAbductionPlanner();                   // 0: 0x482b10 (deleting), 0x482bf0
    virtual void Read(istream* in) override;         // 1: 0x482c00
    virtual void Write(ostream* out) override;       // 2: 0x482c20
    virtual int GetSubtype() override { return 4; }  // 5: 0x4047d0
    virtual int Virtual06() override;                // 6: 0x482c40
    virtual int GetStallLimit() override { return 5; }  // 9: 0x404c20
    virtual void Virtual12(int event) override { HandleQuietEvent(event); }  // 12: 0x47d4d0
    virtual int Virtual16() override { PickLeaders(); return 0; }  // 16: 0x482ce0
    virtual int Virtual17() override { PickMembers(); return 0; }  // 17: 0x482ec0

    uint32_t m_capability;  // +0x7c the character capability the mission needs

    void PickLeaders();  // 0x482ce0
    void PickMembers();  // 0x482ec0
};

// 0x6a Death Star sabotage: VT 0x65a958
class AiDsSabotagePlanner : public AiFreePlanner {
public:
    AiDsSabotagePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x482020
    virtual ~AiDsSabotagePlanner();                  // 0: 0x482000 (deleting), 0x4820a0
    virtual int GetSubtype() override { return 0; }  // 5: 0x6158b0
    virtual int Virtual06() override;                // 6: 0x4820e0
    virtual int GetStallLimit() override { return 10; }  // 9: 0x4820c0
    virtual void Virtual12(int event) override { HandleQuietEvent(event); }  // 12: 0x47d4d0
    virtual int Virtual16() override { return 5; }   // 16: 0x4820d0
    virtual int Virtual17() override { return 4; }   // 17: 0x4047d0
    virtual void Prepare() override;                 // 18: 0x482180
    virtual void Virtual19() override;               // 19: 0x482330
    virtual int ChooseTarget() override;             // 20: 0x482560

    void FindTargetShip();                           // 0x482910
};

// 0x61 Rescue (free planner that hunts for a character): VT 0x65a710
class AiRescuePlanner : public AiFreePlanner {
public:
    AiRescuePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x47e130
    virtual ~AiRescuePlanner();                      // 0: 0x47e110 (deleting), 0x47e1b0
    virtual int GetSubtype() override { return 0; }  // 5: 0x6158b0
    virtual int Virtual06() override;                // 6: 0x47e1d0
    virtual int GetStallLimit() override { return 10; }  // 9: 0x4820c0
    virtual void Virtual12(int event) override { HandleQuietEvent(event); }  // 12: 0x47d4d0
    virtual int Virtual16() override { return 8; }   // 16: 0x47e1c0
    virtual int Virtual17() override { return 4; }   // 17: 0x4047d0
    virtual void Prepare() override;                 // 18: 0x47e270
    virtual void Virtual19() override;               // 19: 0x47e3f0
    virtual int ChooseTarget() override;             // 20: 0x47e620

    void PickCandidate();                            // 0x47ecf0
};

// 0x62 free planner (every subtype but 4, which AiAbductionPlanner handles): VT 0x65a9f8
class AiMission62Planner : public AiFreePlanner {
public:
    AiMission62Planner(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x483140
    virtual ~AiMission62Planner();                   // 0: 0x483120 (deleting), 0x4831c0
    virtual int GetSubtype() override { return 0; }  // 5: 0x6158b0
    virtual int Virtual06() override;                // 6: 0x4831f0
    virtual int GetStallLimit() override { return 10; }  // 9: 0x4820c0
    virtual void Virtual12(int event) override { HandleQuietEvent(event); }  // 12: 0x47d4d0
    virtual int Virtual16() override { return 6; }   // 16: 0x4831e0
    virtual int Virtual17() override { return 4; }   // 17: 0x4047d0
    virtual void Prepare() override;                 // 18: 0x483290
    virtual void Virtual19() override;               // 19: 0x483410
    virtual int ChooseTarget() override;             // 20: 0x483640

    void PickCandidate();                            // 0x483d10
};

// 0x51 Diplomacy: VT 0x65a900
class AiDiplomacyPlanner : public AiFreePlanner {
public:
    AiDiplomacyPlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x481460
    virtual ~AiDiplomacyPlanner();                   // 0: 0x481440 (deleting), 0x4814e0
    virtual int GetSubtype() override { return 0; }  // 5: 0x6158b0
    virtual int Virtual06() override;                // 6: 0x481500
    virtual int GetStallLimit() override { return 5; }  // 9: 0x404c20
    virtual void Virtual12(int event) override { HandleEvent(event); }  // 12: 0x47c890
    virtual int Virtual13() override;                // 13: 0x481eb0
    virtual void OnFinished() override;              // 14: 0x481fe0
    virtual int CanUse(const ObjectKey* character) override;  // 15: 0x481ef0
    virtual int Virtual16() override { return 0xb; } // 16: 0x4814f0
    virtual int Virtual17() override { return 0; }   // 17: 0x6158b0
    virtual void Prepare() override;                 // 18: 0x4815a0
    virtual void Virtual19() override { AssignNoMembers(); }  // 19: 0x47f500
    virtual int ChooseTarget() override;             // 20: 0x481720
};

// 0x54 system chooser, subtype 5 (a free planner that remembers the sector it works in): VT 0x65a7b0
class AiMission54State5Planner : public AiFreePlanner {
public:
    AiMission54State5Planner(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x47f550
    virtual ~AiMission54State5Planner();             // 0: 0x47f530 (deleting), 0x47f5e0
    virtual void Read(istream* in) override;         // 1: 0x47f630 (shared with AiEspionageState6Planner)
    virtual void Write(ostream* out) override;       // 2: 0x47f650 (shared with AiEspionageState6Planner)
    virtual int GetSubtype() override { return 5; }  // 5: 0x404c20
    virtual int Virtual06() override;                // 6: 0x47f680
    virtual int GetStallLimit() override { return 1000; }  // 9: 0x480df0
    virtual void Virtual12(int event) override;      // 12: 0x47f720
    virtual void OnFinished() override;              // 14: 0x47fe10
    virtual int Virtual16() override { return 9; }   // 16: 0x47f670
    virtual int Virtual17() override { return 0; }   // 17: 0x6158b0
    virtual void Prepare() override;                 // 18: 0x47f790
    virtual void Virtual19() override { MarkMembersAssigned(); }  // 19: 0x481100
    virtual int ChooseTarget() override;             // 20: 0x47f970

    ObjectKey m_sectorKey;                           // +0x70 the sector of m_x34 (flagged busy while the mission runs)
};

// 0x52 Espionage, subtype 6: VT 0x65a8a8
class AiEspionageState6Planner : public AiFreePlanner {
public:
    AiEspionageState6Planner(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x480d10
    virtual ~AiEspionageState6Planner();             // 0: 0x480cf0 (deleting), 0x480da0
    virtual void Read(istream* in) override;         // 1: 0x47f630 (the code AiMission54State5Planner::Read has)
    virtual void Write(ostream* out) override;       // 2: 0x47f650
    virtual int GetSubtype() override { return 6; }  // 5: 0x4953a0
    virtual int Virtual06() override;                // 6: 0x480e10
    virtual int GetStallLimit() override { return 1000; }  // 9: 0x480df0
    virtual void Virtual12(int event) override;      // 12: 0x480eb0
    virtual void OnFinished() override;              // 14: 0x481420
    virtual int Virtual16() override { return 10; }  // 16: 0x480e00
    virtual int Virtual17() override { return 0; }   // 17: 0x6158b0
    virtual void Prepare() override;                 // 18: 0x480f30
    virtual void Virtual19() override { MarkMembersAssigned(); }  // 19: 0x481100
    virtual int ChooseTarget() override;             // 20: 0x481110

    ObjectKey m_sectorKey;                           // +0x70
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(AiResearchPlanner1, 0x70);
REB_ASSERT_SIZE(AiResearchPlanner2, 0x70);
REB_ASSERT_SIZE(AiResearchPlanner3, 0x70);
REB_ASSERT_SIZE(AiSubduePlanner, 0x70);
REB_ASSERT_SIZE(AiRecruitPlanner, 0x70);
REB_ASSERT_SIZE(AiIncitePlanner, 0x70);
REB_ASSERT_SIZE(AiDsSabotagePlanner, 0x70);
REB_ASSERT_SIZE(AiRescuePlanner, 0x70);
REB_ASSERT_SIZE(AiMission62Planner, 0x70);
REB_ASSERT_SIZE(AiDiplomacyPlanner, 0x70);
REB_ASSERT_SIZE(AiMission54State5Planner, 0x74);
REB_ASSERT_SIZE(AiEspionageState6Planner, 0x74);
REB_ASSERT_SIZE(AiSabotagePlanner, 0x7c);
REB_ASSERT_SIZE(AiMission54Planner, 0x7c);
REB_ASSERT_SIZE(AiEspionagePlanner, 0x7c);
REB_ASSERT_SIZE(AiAbductionPlanner, 0x80);
#pragma clang diagnostic pop
