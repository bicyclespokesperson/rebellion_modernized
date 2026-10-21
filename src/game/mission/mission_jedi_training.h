#pragma once
#include "mission/mission_extra_key.h"

// MissionJediTraining (family 0x58, ctor 0x5701d0, VT 0x6667d8, secondary ObjectBase VT 0x6667d4, 0xac bytes): a Jedi
// teacher trains the Force of the team's student. The extra key property Teacher (+0xa8, base class
// MissionExtraKeyBase) names the teacher character. Strings: "MissionJediTrainingTeacherKeyNotif", "Teacher".
class MissionJediTraining : public MissionExtraKeyBase
{
  public:
  MissionJediTraining();           // 0x5701d0
  ~MissionJediTraining() override; // 0x570230 (deleting), 0x570250

  uint32_t GetFamily() override;                                                           // 1, 0x5705a0: 0x58
  int ApplyAll(NotifyContext* ctx) override;                                               // 9, 0x570580
  int GetEventKinds(int unused, EventKindSet* out) override;                               // 15, 0x5702b0
  int CheckOrder(int a, int orderKind, StatusCode* out) override;                          // 22, 0x5702d0
  int GetCheckFunctions(MissionCheckFunctions* out) override;                              // 111, 0x570390
  GameObj* CreateViewObject() override;                                                    // 57, 0x5704f0
  int RemoveMember(ObjectKey* member, int decoy, NotifyContext* ctx) override;             // 116, 0x5705b0
  int OnPhaseChanged(int oldValue, int newValue, NotifyContext* ctx) override;             // 125, 0x571410
  int GetPersonSuccessChance(GameObj* person, int* out) override;                          // 157, 0x5710b0
  int BeforeScoringResults(NotifyContext* ctx) override;                                   // 158, stub 0x51ebb0
  int ApplyPersonResult(GameObj* person, int roll, int* out, NotifyContext* ctx) override; // 159, 0x5712b0
  int AfterScoringResults(NotifyContext* ctx) override;                                    // 160, shared 0x576700
  int OnExtraKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) override
  {
    return 1;
  } // 162, shared stub 0x524b10
  int OnExtraKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) override; // 163, 0x571590

  int SetTeacherKey(ObjectKey* value, NotifyContext* ctx); // 0x5703b0
  int ApplyExtraKey(NotifyContext* ctx);                   // 0x5711d0 (ApplyTeacherKey)
  int ApplyAllBody(NotifyContext* ctx);                    // 0x5711a0: slot 9 behind its side-view guard
  // Called by the character code (0x545a10 / 0x545b20): asks for phase 9 / 10 once the mission is far enough along.
  int RequestPhase9(NotifyContext* ctx);  // 0x570470
  int RequestPhase10(NotifyContext* ctx); // 0x5704b0

  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x570690
  static int BuildEventKinds();                                                  // 0x5715d0
  static GameObjNotifier* Create();                                              // 0x570620
};

// Iterator over the Jedi Training missions among the children of an object (family 0x58, VT 0x666a68, destructor only).
class MissionJediTrainingIterator : public ChildIterator
{
  public:
  explicit MissionJediTrainingIterator(GameObjNotifier* parent); // 0x571650 (state filter 1)
  ~MissionJediTrainingIterator() override;                       // 0x5716d0 (deleting), 0x5716f0
};
