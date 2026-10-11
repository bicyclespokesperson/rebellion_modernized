#pragma once
#include "mission/mission_bases.h"
#include "mission/mission_leaf.h"
#include "story/story_characters.h"

class ObjectKeySet;
class ObjectRefList;

// The five story missions. They sit on the story rule bases: Pickup (family 0x73), Vacation (0x72) and the Dagobah
// training (0x71) on MissionStoryBase, whose target is a named system, and the Palace (0x64) and Bounty (0x65) missions on
// MissionCharacterTargetBase. None but the Dagobah mission has a field; slots 5-9 and 37 are the shared forwarders (see
// mission_leaf.h), slot 157 of Pickup, Vacation, Dagobah and Bounty is the shared "value 100" stub (0x575060) and slot 158
// the shared stub 0x51ebb0.

// Pickup (ctor 0x56d260, VT 0x665058, secondary ObjectBase VT 0x665054, 0xa8 bytes): the Empire's pickup of the captured
// Emperor stand-in (family 0x35) on the story's target system; completing it frees every captured character of the
// objective system.
class MissionPickup : public MissionStoryBase
{
public:
  MissionPickup(); // 0x56d260
  ~MissionPickup() override; // 0x56d280 (deleting), 0x56d2a0

  uint32_t GetFamily() override; // 1, 0x56d470: 0x73
  int Read(istream* in) override; // 5, 0x56d430
  int Write(ostream* out) override; // 6, 0x56d440
  int Checksum(Crc32* crc) override; // 7, 0x56d450
  int Validate() override; // 8, shared 0x56d460
  int ApplyAll(NotifyContext* ctx) override; // 9, shared 0x576d30
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, shared 0x576d10
  int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x56d2c0
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x56d2e0
  int GetCheckFunctions(MissionCheckFunctions* out) override; // 111, 0x56d3a0
  GameObj* CreateViewObject() override; // 57, 0x56d3c0
  int Virtual157(GameObj* person, int* out) override; // 157, shared 0x575060
  int Virtual158(NotifyContext* ctx) override; // 158, stub 0x51ebb0
  int Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) override; // 159, 0x56da80
  int Virtual160(NotifyContext* ctx) override; // 160, shared 0x576700

  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x56d4f0
  static int CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out); // 0x56d6d0
  static int CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out); // 0x56d910
  static int BuildEventKinds(); // 0x56dc70, g_missionPickupEventKinds at 0x6bb998
  static GameObjNotifier* Create(); // 0x56d480
};

// Palace (ctor 0x56dcf0, VT 0x6652e8, secondary ObjectBase VT 0x6652e0, 0xa8 bytes): the mission against a story
// character (Luke, Leia or the family 0x38 character 0x343 may be sent) that turns on the objective character.
class MissionPalace : public MissionCharacterTargetBase
{
public:
  MissionPalace(); // 0x56dcf0
  ~MissionPalace() override; // 0x56dd10 (deleting), 0x56dd30

  uint32_t GetFamily() override; // 1, 0x56def0: 0x64
  int Read(istream* in) override; // 5, 0x56dec0
  int Write(ostream* out) override; // 6, 0x56ded0
  int Checksum(Crc32* crc) override; // 7, 0x56dee0
  int Validate() override; // 8, shared 0x56ad20
  int ApplyAll(NotifyContext* ctx) override; // 9, shared 0x576d30
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, shared 0x576d10
  int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x56dd50
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x56dd70
  int GetCheckFunctions(MissionCheckFunctions* out) override; // 111, 0x56de30
  GameObj* CreateViewObject() override; // 57, 0x56de50
  int Virtual157(GameObj* person, int* out) override; // 157, 0x56e620
  int Virtual158(NotifyContext* ctx) override; // 158, stub 0x51ebb0
  int Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) override; // 159, 0x56e650
  int Virtual160(NotifyContext* ctx) override; // 160, 0x56e870

  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x56df70
  static int CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out); // 0x56e1f0
  static int CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out); // 0x56e4b0
  static int BuildEventKinds(); // 0x56e980, g_missionPalaceEventKinds at 0x6bb9a8
  static GameObjNotifier* Create(); // 0x56df00
};

// Vacation (ctor 0x571cd0, VT 0x666d00, secondary ObjectBase VT 0x666cfc, 0xa8 bytes): Luke's (family 0x32) journey to
// the story system 0x98000442.
class MissionVacation : public MissionStoryBase
{
public:
  MissionVacation(); // 0x571cd0
  ~MissionVacation() override; // 0x571cf0 (deleting), 0x571d10

  uint32_t GetFamily() override; // 1, 0x571ea0: 0x72
  int Read(istream* in) override; // 5, shared 0x56d430
  int Write(ostream* out) override; // 6, shared 0x56d440
  int Checksum(Crc32* crc) override; // 7, shared 0x56d450
  int Validate() override; // 8, shared 0x56d460
  int ApplyAll(NotifyContext* ctx) override; // 9, shared 0x576d30
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, shared 0x576d10
  int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x571d30
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x571d50
  int GetCheckFunctions(MissionCheckFunctions* out) override; // 111, 0x571e10
  GameObj* CreateViewObject() override; // 57, 0x571e30
  int Virtual157(GameObj* person, int* out) override; // 157, shared 0x575060
  int Virtual158(NotifyContext* ctx) override; // 158, stub 0x51ebb0
  int Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) override; // 159, 0x572470
  int Virtual160(NotifyContext* ctx) override; // 160, 0x572490

  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x571f20
  static int CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out); // 0x572100
  static int CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out); // 0x572340
  static int BuildEventKinds(); // 0x5724b0, g_missionVacationEventKinds at 0x6bba40
  static GameObjNotifier* Create(); // 0x571eb0
};

// The Dagobah training (family 0x71, ctor 0x5747f0, VT 0x667f38, secondary ObjectBase VT 0x667f30, 0xac bytes): Luke is
// trained on Dagobah. The extra property FirstTrainingDay (+0xa8, the game day phase 8 began) decides at phase 11 how much
// Force experience Luke gets, and completes the Dagobah story flag. Virtual slots 162 (changed, shared stub 0x524b10) and
// 163 (notified) belong to the property.
class DagobahMission : public MissionStoryBase
{
public:
  DagobahMission(); // 0x5747f0
  ~DagobahMission() override; // 0x574820 (deleting), 0x574840

  uint32_t GetFamily() override; // 1, 0x574a90: 0x71
  int Read(istream* in) override; // 5, 0x574a40
  int Write(ostream* out) override; // 6, 0x574a50
  int Checksum(Crc32* crc) override; // 7, 0x574a80
  int Validate() override; // 8, shared 0x56d460
  int ApplyAll(NotifyContext* ctx) override; // 9, 0x574a60
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, shared 0x576d10
  int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x574860
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x574880
  int GetCheckFunctions(MissionCheckFunctions* out) override; // 111, 0x574940
  GameObj* CreateViewObject() override; // 57, 0x5749d0
  int OnPhaseChanged(int oldValue, int newValue, NotifyContext* ctx) override; // 125, 0x5751d0
  int Virtual157(GameObj* person, int* out) override; // 157, shared 0x575060
  int Virtual158(NotifyContext* ctx) override; // 158, stub 0x51ebb0
  int Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) override; // 159, shared 0x572470
  int Virtual160(NotifyContext* ctx) override; // 160, shared 0x572490
  virtual int OnFirstTrainingDayChanged(int oldValue, int newValue, NotifyContext* ctx)
  {
    return 1;
  } // 162, shared stub 0x524b10
  virtual int OnFirstTrainingDayNotified(int oldValue, int newValue, NotifyContext* ctx); // 163, 0x575320

  int SetFirstTrainingDay(int value, NotifyContext* ctx); // 0x574960
  int ApplyFirstTrainingDay(NotifyContext* ctx); // 0x575140
  int ApplyAllBody(NotifyContext* ctx); // 0x5750e0
  int ReadBody(istream* in); // 0x575080
  int WriteBody(ostream* out); // 0x5750b0
  int ChecksumBody(Crc32* crc); // 0x575110

  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x574b10
  static int CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out); // 0x574cf0
  static int CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out); // 0x574f30
  static int BuildEventKinds(); // 0x575360, g_dagobahMissionEventKinds at 0x6bbab8
  static GameObjNotifier* Create(); // 0x574aa0

  int m_firstTrainingDay; // +0xa8
};

// Bounty (family 0x65, ctor 0x5756b0, VT 0x6685c0, secondary ObjectBase VT 0x6685bc, 0xa8 bytes): bounty hunters (the
// special forces unit 0x3c000009) go after Han. Its slot 157 is the shared stub, 160 the shared stub 0x51ebb0.
class MissionBounty : public MissionCharacterTargetBase
{
public:
  MissionBounty(); // 0x5756b0
  ~MissionBounty() override; // 0x5756d0 (deleting), 0x5756f0

  uint32_t GetFamily() override; // 1, 0x575880: 0x65
  int Read(istream* in) override; // 5, shared 0x56dec0
  int Write(ostream* out) override; // 6, shared 0x56ded0
  int Checksum(Crc32* crc) override; // 7, shared 0x56dee0
  int Validate() override; // 8, shared 0x56ad20
  int ApplyAll(NotifyContext* ctx) override; // 9, shared 0x576d30
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, shared 0x576d10
  int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x575710
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x575730
  int GetCheckFunctions(MissionCheckFunctions* out) override; // 111, 0x5757f0
  GameObj* CreateViewObject() override; // 57, 0x575810
  int Virtual157(GameObj* person, int* out) override; // 157, shared 0x575060
  int Virtual158(NotifyContext* ctx) override; // 158, stub 0x51ebb0
  int Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) override; // 159, 0x575de0
  int Virtual160(NotifyContext* ctx) override; // 160, stub 0x51ebb0

  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x575900
  static int CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out); // 0x575ae0
  static int CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out); // 0x575cc0
  static int BuildEventKinds(); // 0x575f80, g_missionBountyEventKinds at 0x6bbad8
  static GameObjNotifier* Create(); // 0x575890
};

// The second-level forwarders of the slots 5-7 and 9 that the story missions' first-level forwarders call: they run the
// rule-base bodies on the same object.
int MissionStoryReadForward(Mission* self, istream* in); // 0x56da40
int MissionStoryWriteForward(Mission* self, ostream* out); // 0x56da50
int MissionStoryApplyAllForward(Mission* self, NotifyContext* ctx); // 0x56da60
int MissionStoryChecksumForward(Mission* self, Crc32* crc); // 0x56da70
