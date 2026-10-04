#pragma once
#include <cstdint>

#include "core/layout.h"
#include "data/sd_records.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/game_obj_data.h"
#include "gameobj/object_family.h"
#include "gameobj/object_key.h"
#include "gameobj/tracked_stat.h"

// Mission (ctor 0x5208b0, VT 0x65edf8, secondary ObjectBase VT 0x65edf4, 0xa8 bytes) is the base of every mission
// (families 0x40-0x7f; one family per mission type). It sits directly on GameObj. Class chain:
//
//   Mission
//     MissionTypeBase (0x593730, VT 0x66b0e0, no fields)                      mission/mission_type_base.h
//     MissionSystemBase (0x5923a0, VT 0x66abc0)                               mission_base_a.h
//       ... target-system/target-character bases 0x5866c0 / 0x586c20, 0x5933a0, 0x5941c0 ...
//
// Vtable: 157 slots. 0-110 are GameObj's, 111-156 are new. Slots 1 (GetFamily), 5-9, 37 and 57 stay pure here; the
// persistence slots have bodies below that the leaf classes call qualified (as with Role). Slot 120 is a pure virtual
// of the mission itself.
//
// The 18 properties, in field order (the same order as the Set/Apply functions and the OnXChanged/OnXNotified slots
// 121-138 / 139-156):
//   5 ints          UserId +0x58, UserId2 +0x5c, TaskStatus +0x60 (0..3), CompletionStatus +0x64 (-1..16), Phase +0x68 (0..11)
//   6 keys          OriginLocationKey +0x6c, ObjectiveKey +0x70, TargetKey +0x74, TargetLocationKey +0x78, LeaderKey +0x7c,
//                   LeaderSeedKey +0x80 (NullableKey)
//   4 key sets      Team +0x84, Decoys +0x8c, Captured +0x94, Set4 +0x9c (unnamed, never iterated)
//   3 flag bits     m_flagsA4: ReadyForNextPhase 0x1, ImpliedTeam 0x2, Mandatory 0x4
// Property pattern as in GameObj: SetX (master only, validates, stores, view1->ApplyX, view2->ApplyX, OnXChanged),
// ApplyX (copies master -> side view, raises OnXNotified).
//
// Strings: "Invalid TaskStatus/Phase value!" (the range checks), "Mission*Notif" (the OnXNotified events).

class Crc32;
class EventKindSet;
class MissionMemberIterator;

namespace mission_flag {
constexpr uint32_t kReadyForNextPhase = 0x1;
constexpr uint32_t kImpliedTeam = 0x2;
constexpr uint32_t kMandatory = 0x4;
}  // namespace mission_flag

namespace mission_phase {
constexpr int kMin = 0;
constexpr int kMax = 0xb;
}  // namespace mission_phase

// The four 4-bit values MissionData keeps in its status word (VT 0x66a098, 8 bytes, ctor 0x5834d0, deleting dtor
// 0x5834e0, dtor 0x583500, assignment 0x583510): one level per mission phase 2, 3, 7 and 9 (see Mission::GetPhaseLevel).
// Slots: 1 Read (0x583530: one packed word), 2 Write (0x583550: the four nibbles), 3 Checksum (shared TrackedStat-style).
class MissionPhaseLevels {
public:
    MissionPhaseLevels();                                 // 0x5834d0
    virtual ~MissionPhaseLevels();                        // 0x583500 (deleting 0x5834e0)
    virtual int Read(istream* in);                        // 0x583530
    virtual int Write(ostream* out);                      // shared 0x595510 (writes one packed word)
    virtual int Checksum(Crc32* crc);                     // 0x583550
    virtual int IsValid() { return 1; }                   // shared 0x40f340
    MissionPhaseLevels& operator=(const MissionPhaseLevels& other);  // 0x583510

    uint32_t m_levels;  // +0x04
};

// The data block of a mission master (VT 0x66a0b0, 0x38 bytes, ctor 0x5835b0, created by Mission::CreateData).
class MissionData : public GameObjData {
public:
    MissionData();                                    // 0x5835b0
    virtual ~MissionData();                           // 0x583620 (deleting), 0x583640
    virtual int Read(istream* in) override;           // 0x5836b0
    virtual int Write(ostream* out) override;         // 0x583720
    virtual int Checksum(Crc32* crc) override;        // 0x583790

    int m_unk18;                    // +0x18, starts at 1; Mission::UpdateTeamCapability stores "all team members can act" here
    int m_unk1c;                    // +0x1c, 0; a small level (>4 / >6 are tested by Mission::IsAdvancedPhaseLevel*)
    TrackedStat m_statA;            // +0x20
    TrackedStat m_statB;            // +0x28
    MissionPhaseLevels m_levels;    // +0x30
};

// Event-callback pair filled by Mission slot 111 (0x5215e0): the two static checkers 0x523670 / 0x523450.
struct MissionCheckFunctions {
    void* m_unk00;  // +0x00 = 0x523670
    void* m_unk04;  // +0x04 = 0x523450
};

class Mission : public GameObj {
public:
    Mission();            // 0x5208b0
    virtual ~Mission();   // 0x5209c0 (scalar deleting destructor 0x5209a0)

    // ---- overrides of GameObj slots ----
    virtual int GetOwnerKey(ObjectKey* out) override;                                 // 3, 0x5214e0: clears *out, returns 1
    virtual int GetEventKinds(int unused, EventKindSet* out) override;                // 15, 0x521500
    virtual int IsObservable() override { return 0; }                                 // 19
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;           // 22, 0x521520
    virtual GameObjData* CreateData() override;                                       // 58, 0x525690
    virtual int OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx) override;  // 61, 0x524a60
    virtual int OnDestroyedChanged(int newValue, NotifyContext* ctx) override;        // 71, 0x524a80

    // ---- persistence/validation: pure here with bodies the leaf classes call qualified ----
    virtual int Read(istream* in) override = 0;               // 5, body 0x5237c0
    virtual int Write(ostream* out) override = 0;             // 6, body 0x523910
    virtual int Checksum(Crc32* crc) override = 0;            // 7, body 0x523b20
    virtual int Validate() override = 0;                      // 8, body 0x523c60
    virtual int ApplyAll(NotifyContext* ctx) override = 0;    // 9, body 0x523a60

    // ---- new virtuals (slots 111-120); names provisional ----
    virtual int GetCheckFunctions(MissionCheckFunctions* out);                       // 111, 0x5215e0
    virtual int CheckActivity(const ActivityRequest* request, StatusCode* out);      // 112, 0x521600
    virtual int CheckActivityStatus(const ActivityRequest* request, StatusCode* out);  // 113, 0x521700: out = {1,-1}, then slot 112
    virtual int Virtual114() { return 0; }                                           // 114
    // Adds a person to the team (decoy false), decoys (decoy true) or captured list, after the membership checks.
    virtual int AddMember(ObjectKey* member, int decoy, NotifyContext* ctx);         // 115, 0x522b30
    virtual int RemoveMember(ObjectKey* member, int decoy, NotifyContext* ctx);      // 116, 0x522fc0
    virtual int Virtual117(int a, NotifyContext* ctx);                               // 117, 0x523310
    virtual int Virtual118(NotifyContext* ctx);                                      // 118, 0x5233a0: if Phase 8, SetPhase(2)... see body
    virtual int Virtual119(int a, NotifyContext* ctx);                               // 119, 0x5233d0
    virtual int Virtual120() = 0;                                                    // 120, pure

    // ---- OnXChanged events on the master (slots 121-138) ----
    virtual int OnUserIdChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }               // 121
    virtual int OnUserId2Changed(int oldValue, int newValue, NotifyContext* ctx) { return 1; }              // 122
    virtual int OnTaskStatusChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }           // 123
    virtual int OnCompletionStatusChanged(int oldValue, int newValue, NotifyContext* ctx);                  // 124, 0x524b20
    virtual int OnPhaseChanged(int oldValue, int newValue, NotifyContext* ctx);                             // 125, 0x524b70
    virtual int OnOriginLocationKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) { return 1; }  // 126
    virtual int OnObjectiveKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);            // 127, 0x524f00
    virtual int OnTargetKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) { return 1; }  // 128
    virtual int OnTargetLocationKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) { return 1; }  // 129
    virtual int OnLeaderKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) { return 1; }  // 130
    virtual int OnLeaderSeedKeyChanged(NullableKey* oldKey, NullableKey* newKey, NotifyContext* ctx) { return 1; }  // 131
    virtual int OnTeamChanged(ObjectKeyValueSet* oldSet, ObjectKeyValueSet* newSet, NotifyContext* ctx) { return 1; }  // 132
    virtual int OnDecoysChanged(ObjectKeyValueSet* oldSet, ObjectKeyValueSet* newSet, NotifyContext* ctx) { return 1; }  // 133
    virtual int OnCapturedChanged(ObjectKeyValueSet* oldSet, ObjectKeyValueSet* newSet, NotifyContext* ctx) { return 1; }  // 134
    virtual int OnSet4Changed(ObjectKeyValueSet* oldSet, ObjectKeyValueSet* newSet, NotifyContext* ctx) { return 1; }  // 135
    virtual int OnReadyForNextPhaseChanged(int newValue, NotifyContext* ctx);                               // 136, 0x524f80
    virtual int OnImpliedTeamChanged(int newValue, NotifyContext* ctx) { return 1; }                        // 137
    virtual int OnMandatoryChanged(int newValue, NotifyContext* ctx) { return 1; }                          // 138

    // ---- OnXNotified events on a side view (slots 139-156) ----
    virtual int OnUserIdNotified(int oldValue, int newValue, NotifyContext* ctx);                           // 139, 0x524fc0
    virtual int OnUserId2Notified(int oldValue, int newValue, NotifyContext* ctx);                          // 140, 0x525000
    virtual int OnTaskStatusNotified(int oldValue, int newValue, NotifyContext* ctx);                       // 141, 0x525040
    virtual int OnCompletionStatusNotified(int oldValue, int newValue, NotifyContext* ctx);                 // 142, 0x525070
    virtual int OnPhaseNotified(int oldValue, int newValue, NotifyContext* ctx) { return IsSideView(); }    // 143, shared 0x55b1f0
    virtual int OnOriginLocationKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);      // 144, 0x5250a0
    virtual int OnObjectiveKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);           // 145, 0x5250e0
    virtual int OnTargetKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);              // 146, 0x525120
    virtual int OnTargetLocationKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);      // 147, 0x525160
    virtual int OnLeaderKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);              // 148, 0x5251a0
    virtual int OnLeaderSeedKeyNotified(NullableKey* oldKey, NullableKey* newKey, NotifyContext* ctx);      // 149, 0x5251e0
    virtual int OnTeamNotified(ObjectKeyValueSet* oldSet, ObjectKeyValueSet* newSet, NotifyContext* ctx);   // 150, 0x525220
    virtual int OnDecoysNotified(ObjectKeyValueSet* oldSet, ObjectKeyValueSet* newSet, NotifyContext* ctx); // 151, 0x5252f0
    virtual int OnCapturedNotified(ObjectKeyValueSet* oldSet, ObjectKeyValueSet* newSet, NotifyContext* ctx);  // 152, 0x5253c0
    virtual int OnSet4Notified(ObjectKeyValueSet* oldSet, ObjectKeyValueSet* newSet, NotifyContext* ctx);   // 153, 0x525490
    virtual int OnReadyForNextPhaseNotified(int newValue, NotifyContext* ctx);                              // 154, 0x525560
    virtual int OnImpliedTeamNotified(int newValue, NotifyContext* ctx);                                    // 155, 0x5255a0
    virtual int OnMandatoryNotified(int newValue, NotifyContext* ctx);                                      // 156, 0x5255e0

    Mission* ViewAsMission(int view) { return static_cast<Mission*>(ViewAs(view)); }
    MissionSD* GetRecordSD() { return static_cast<MissionSD*>(m_record); }

    // ==== SECTION core (0x520aa0-0x521520): see mission_core.cpp; names provisional ====
    TrackedStat* GetStatA();             // 0x520aa0: &m_data->m_statA, null without data
    TrackedStat* GetStatB();             // 0x520ab0: &m_data->m_statB
    int GetDataField1c();                // 0x520ac0
    int IsDataField1cAbove4();           // 0x520ad0
    int IsDataField1cAbove6();           // 0x520ae0
    int CanAllTeamAct();                 // 0x520af0
    int IsPhase4();                      // 0x520b20
    int IsPhase8();                      // 0x520b30
    // Words of the MissionSD record that the mission logic tests (0 without a record).
    int GetRecordTimerMinDays();         // 0x520b40: +0x50
    int GetRecordTimerSpreadDays();      // 0x520b50: +0x54
    int GetRecordRepeats();              // 0x520b60: +0x58
    int GetRecordHidden();               // 0x520b70: +0x5c
    int GetRecordDetectionPhases();      // 0x520b80: +0x60
    int GetRecordCanResign();            // 0x520b90: +0x64
    int GetRecordField68();              // 0x520ba0: +0x68
    int TargetLocationDiffersFromOrigin();  // 0x520bb0
    int IsInTeam(ObjectKey* key);        // 0x520bd0
    int IsInDecoys(ObjectKey* key);      // 0x520bf0
    int IsInCaptured(ObjectKey* key);    // 0x520c10
    int IsMember(ObjectKey* key);        // 0x520c30 (static-looking: ECX is this, passes it on)
    int TargetKeyIs(ObjectKey* key);     // 0x520c70
    int TargetLocationKeyIs(ObjectKey* key);  // 0x520c90
    int ObjectiveKeyIs(ObjectKey* key);  // 0x520cb0
    int AverageMemberSkill();            // 0x520cd0
    int HasMembers();                    // 0x520d60
    int CountMembers();                  // 0x520dd0
    int HasCompletionStatus();           // 0x520e40: m_completionStatus != 0
    uint32_t GetPhaseLevel();            // 0x520e50
    int FillStatusFlags(IntPair* out);   // 0x520ec0
    int GetObjectivePerson(GameObj** out);          // 0x520f40
    int GetOriginLocationObject(GameObj** out);     // 0x521030
    int GetTargetLocationObject(GameObj** out);     // 0x521050
    int GetLeaderObject(GameObj** out);             // 0x521070
    int GetOriginInOwnerSide(GameObj** out);        // 0x521160
    int GetTargetInOwnerSide(GameObj** out);        // 0x5211c0
    int GetTransitionFlags(int* a, int* b, int* c);  // 0x521220

    // ==== SECTION setters (0x521780-0x522210): see mission_setters.cpp ====
    int SetUserId(int value, NotifyContext* ctx);                 // 0x521780
    int SetUserId2(int value, NotifyContext* ctx);                // 0x521800
    int SetTaskStatus(int value, NotifyContext* ctx);             // 0x521880
    int SetCompletionStatus(int value, NotifyContext* ctx);       // 0x521900
    int SetPhase(int value, NotifyContext* ctx);                  // 0x521980
    int SetOriginLocationKey(ObjectKey* key, NotifyContext* ctx);  // 0x521a00
    int SetObjectiveKey(ObjectKey* key, NotifyContext* ctx);      // 0x521ab0
    int SetTargetKey(ObjectKey* key, NotifyContext* ctx);         // 0x521b60
    int SetTargetLocationKey(ObjectKey* key, NotifyContext* ctx);  // 0x521c10
    int SetLeaderKey(ObjectKey* key, NotifyContext* ctx);         // 0x521cc0
    int SetLeaderSeedKey(NullableKey* key, NotifyContext* ctx);   // 0x521d70
    int SetTeam(ObjectKeyValueSet* set, NotifyContext* ctx);      // 0x521e30
    int SetDecoys(ObjectKeyValueSet* set, NotifyContext* ctx);    // 0x521ef0
    int SetCaptured(ObjectKeyValueSet* set, NotifyContext* ctx);  // 0x521fb0
    int SetSet4(ObjectKeyValueSet* set, NotifyContext* ctx);      // 0x522070
    int SetReadyForNextPhase(int value, NotifyContext* ctx);      // 0x522130
    int SetImpliedTeam(int value, NotifyContext* ctx);            // 0x5221a0
    int SetMandatory(int value, NotifyContext* ctx);              // 0x522210

    // ==== SECTION logic (0x522280-0x523310): see mission_logic.cpp; names provisional ====
    int UpdateTeamAndTarget(NotifyContext* ctx);     // 0x522280
    int UpdatePhaseForTeam(NotifyContext* ctx);      // 0x522480
    int AdvancePhase(NotifyContext* ctx);            // 0x5227d0
    int UpdateLeaderAndTeamKeys(NotifyContext* ctx); // 0x522870
    int ClearImpliedTeamIfPhase4Or8(NotifyContext* ctx);  // 0x522980
    int ScheduleTimerEvent(NotifyContext* ctx);      // 0x5229c0
    void ResetImpliedTeamIfPhase8();                 // 0x522a10
    int ResumeFromPhase11(NotifyContext* ctx);       // 0x522a30
    int AssignPhaseLevels(IntPair* levels);          // 0x522a60
    int ComputeAllTeamCanAct(uint32_t* out);         // 0x522a90

    // ==== SECTION apply (0x523d70-0x5249d0): see mission_apply.cpp ====
    int ApplyUserId(NotifyContext* ctx);              // 0x523d70
    int ApplyUserId2(NotifyContext* ctx);             // 0x523df0
    int ApplyTaskStatus(NotifyContext* ctx);          // 0x523e70
    int ApplyCompletionStatus(NotifyContext* ctx);    // 0x523ef0
    int ApplyPhase(NotifyContext* ctx);               // 0x523f70
    int ApplyOriginLocationKey(NotifyContext* ctx);   // 0x523ff0
    int ApplyObjectiveKey(NotifyContext* ctx);        // 0x5240d0
    int ApplyTargetKey(NotifyContext* ctx);           // 0x5241b0
    int ApplyTargetLocationKey(NotifyContext* ctx);   // 0x524290
    int ApplyLeaderKey(NotifyContext* ctx);           // 0x524370
    int ApplyLeaderSeedKey(NotifyContext* ctx);       // 0x524450
    int ApplyTeam(NotifyContext* ctx);                // 0x524530
    int ApplyDecoys(NotifyContext* ctx);              // 0x524610
    int ApplyCaptured(NotifyContext* ctx);            // 0x5246f0
    int ApplySet4(NotifyContext* ctx);                // 0x5247d0
    int ApplyReadyForNextPhase(NotifyContext* ctx);   // 0x5248b0
    int ApplyImpliedTeam(NotifyContext* ctx);         // 0x524940
    int ApplyMandatory(NotifyContext* ctx);           // 0x5249d0

    // Bodies of the persistence slots (leaf classes call these qualified), see mission_persist.cpp.
    int ReadImpl(istream* in);            // 0x5237c0
    int WriteImpl(ostream* out);          // 0x523910
    int ChecksumImpl(Crc32* crc);         // 0x523b20
    int ValidateImpl();                   // 0x523c60
    int ApplyAllImpl(NotifyContext* ctx); // 0x523a60

    // The static event kind set of the mission base: kind 0x250 (0x525620 builds it, 0x525670 copies it).
    static int BuildEventKinds();                                 // 0x525620
    static int GetBaseEventKinds(int unused, EventKindSet* out);  // 0x525670
    static void InitStatics();                                    // 0x525700

    int m_userId;              // +0x58
    int m_userId2;             // +0x5c
    int m_taskStatus;          // +0x60
    int m_completionStatus;    // +0x64
    int m_phase;               // +0x68
    ObjectKey m_originLocationKey;  // +0x6c
    ObjectKey m_objectiveKey;       // +0x70
    ObjectKey m_targetKey;          // +0x74
    ObjectKey m_targetLocationKey;  // +0x78
    ObjectKey m_leaderKey;          // +0x7c
    NullableKey m_leaderSeedKey;    // +0x80
    ObjectKeyValueSet m_team;       // +0x84
    ObjectKeyValueSet m_decoys;     // +0x8c
    ObjectKeyValueSet m_captured;   // +0x94
    ObjectKeyValueSet m_set4;       // +0x9c
    uint32_t m_flagsA4;             // +0xa4
};

// Iterator over the mission families 0x40-0x80 of a parent (VT 0x65f06c, ctor 0x525750, destructor only).
class MissionIterator : public ChildIterator {
public:
    explicit MissionIterator(GameObjNotifier* parent);  // 0x525750 (state filter 1)
    virtual ~MissionIterator();                                      // 0x5257f0 (deleting 0x5257d0)
};

// Walks the persons (team, decoys, captured: see the three selector flags) of a mission whose family is in a range
// (VT 0x65f070 base; ctor 0x525870, advance 0x5258f0, restart 0x525930, count 0x5259d0, any 0x525a00). The leaf
// iterator classes below fix range and selectors and have only a destructor (each with its own vtable):
//
//   class                              VT        range     team decoys captured   ctor
//   MissionPersonIterator              0x65f074  0x30-0x40  1    1     1          0x525bb0
//   MissionPersonTeamIterator          0x65f078  0x30-0x40  1    0     0          0x525c60
//   MissionPersonCapturedIterator      0x65f07c  0x30-0x40  0    0     1          0x525d10
//   MissionSpecialForcesIterator       0x65f080  0x3c-0x40  1    1     1          0x525dc0
//   MissionSpecialForcesTeamIterator   0x65f084  0x3c-0x40  1    0     0          0x525e70
//   MissionSpecialForcesDecoyIterator  0x65f088  0x3c-0x40  0    1     0          0x525f30
//   MissionCharacterIterator           0x65f08c  0x30-0x3c  1    1     1          0x525fe0
//   MissionCharacterTeamIterator       0x65f090  0x30-0x3c  1    0     0          0x526090
//   MissionCharacterDecoyIterator      0x65f094  0x30-0x3c  0    1     0          0x526140
//   MissionMinorTeamIterator           0x65f098  0x38-0x3c  1    0     0          0x5261f0
//   MissionMinorDecoyIterator          0x65f09c  0x38-0x3c  0    1     0          0x5262a0
//   MissionMajorTeamIterator           0x65f0a0  0x30-0x38  1    0     0          0x526350
//   MissionMajorDecoyIterator          0x65f0a4  0x30-0x38  0    1     0          0x526400
//
// Usage: `Iter it(mission); it.Begin(); while (it.Current()) { ...; it.Advance(); }` (Begin = 0x525930, Advance = 0x5258f0;
// the constructors do NOT position the iterator).
class MissionMemberIterator {
public:
    MissionMemberIterator(Mission* mission, const FamilyRange* range, int team, int decoys, int captured);  // 0x525870
    virtual ~MissionMemberIterator();  // 0x525820 (deleting 0x525800)

    MissionMemberIterator* Advance();  // 0x5258f0
    MissionMemberIterator* Begin();    // 0x525930
    int Count();                       // 0x5259d0
    int HasAny();                      // 0x525a00
    int Accepts();                     // 0x525a20: current object's family is inside the range
    GameObj* Current() const { return m_current; }

    Mission* m_mission;       // +0x04
    FamilyRange m_range;      // +0x08
    int m_wantTeam;           // +0x10
    int m_wantDecoys;         // +0x14
    int m_wantCaptured;       // +0x18
    int m_inTeam;             // +0x1c
    int m_inDecoys;           // +0x20
    int m_inCaptured;         // +0x24
    ObjectKey m_currentKey;   // +0x28
    GameObj* m_current;       // +0x2c, the person named by m_currentKey (null at the end)
};

#define REB_MISSION_ITERATOR_CLASS(NAME, CTOR_ADDR_COMMENT)                       \
    class NAME : public MissionMemberIterator {                                   \
    public:                                                                       \
        explicit NAME(Mission* mission);                                          \
        virtual ~NAME();                                                          \
    }
REB_MISSION_ITERATOR_CLASS(MissionPersonIterator, 0x525bb0);
REB_MISSION_ITERATOR_CLASS(MissionPersonTeamIterator, 0x525c60);
REB_MISSION_ITERATOR_CLASS(MissionPersonCapturedIterator, 0x525d10);
REB_MISSION_ITERATOR_CLASS(MissionSpecialForcesIterator, 0x525dc0);
REB_MISSION_ITERATOR_CLASS(MissionSpecialForcesTeamIterator, 0x525e70);
REB_MISSION_ITERATOR_CLASS(MissionSpecialForcesDecoyIterator, 0x525f30);
REB_MISSION_ITERATOR_CLASS(MissionCharacterIterator, 0x525fe0);
REB_MISSION_ITERATOR_CLASS(MissionCharacterTeamIterator, 0x526090);
REB_MISSION_ITERATOR_CLASS(MissionCharacterDecoyIterator, 0x526140);
REB_MISSION_ITERATOR_CLASS(MissionMinorTeamIterator, 0x5261f0);
REB_MISSION_ITERATOR_CLASS(MissionMinorDecoyIterator, 0x5262a0);
REB_MISSION_ITERATOR_CLASS(MissionMajorTeamIterator, 0x526350);
REB_MISSION_ITERATOR_CLASS(MissionMajorDecoyIterator, 0x526400);
#undef REB_MISSION_ITERATOR_CLASS

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(MissionPhaseLevels, 0x8);
REB_ASSERT_SIZE(MissionData, 0x38);
REB_ASSERT_SIZE(Mission, 0xa8);
REB_ASSERT_OFFSET(Mission, m_userId, 0x58);
REB_ASSERT_OFFSET(Mission, m_phase, 0x68);
REB_ASSERT_OFFSET(Mission, m_leaderSeedKey, 0x80);
REB_ASSERT_OFFSET(Mission, m_team, 0x84);
REB_ASSERT_OFFSET(Mission, m_set4, 0x9c);
REB_ASSERT_OFFSET(Mission, m_flagsA4, 0xa4);
REB_ASSERT_SIZE(MissionIterator, 0x20);
REB_ASSERT_SIZE(MissionMemberIterator, 0x30);
#pragma clang diagnostic pop
