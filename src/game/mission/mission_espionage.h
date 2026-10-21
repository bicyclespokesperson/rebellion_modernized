#pragma once
#include "mission/mission_extra_key.h"

// MissionEspionage (family 0x52, ctor 0x572c50, VT 0x667390, secondary ObjectBase VT 0x66738c, 0xac bytes): the
// Espionage mission against a system. Its extra key property ExtraSystem (+0xa8, base class MissionExtraKeyBase) names
// a second system the mission has revealed: when the mission succeeds, PickExtraSystems chooses systems of the target's
// side and stores them one after the other; OnExtraKeyChanged synchronizes the chosen system's view for the event.
// Strings: "MissionEspionageExtraSystemKeyNotif", "ExtraSystem".
class MissionEspionage : public MissionExtraKeyBase
{
  public:
  MissionEspionage();           // 0x572c50
  ~MissionEspionage() override; // 0x572cb0 (deleting), 0x572cd0

  uint32_t GetFamily() override;                                                             // 1, 0x572f90: 0x52
  int ApplyAll(NotifyContext* ctx) override;                                                 // 9, 0x572f60
  int GetEventKinds(int unused, EventKindSet* out) override;                                 // 15, 0x572d30
  int CheckOrder(int a, int orderKind, StatusCode* out) override;                            // 22, 0x572d50
  int GetCheckFunctions(MissionCheckFunctions* out) override;                                // 111, 0x572e10
  GameObj* CreateViewObject() override;                                                      // 57, 0x572ef0
  int GetPersonSuccessChance(GameObj* person, int* out) override;                            // 157, 0x573090
  int BeforeScoringResults(NotifyContext* ctx) override;                                     // 158, stub 0x51ebb0
  int ApplyPersonResult(GameObj* person, int roll, int* out, NotifyContext* ctx) override;   // 159, 0x573540
  int AfterScoringResults(NotifyContext* ctx) override;                                      // 160, 0x573610
  int OnExtraKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) override;  // 162, 0x573890
  int OnExtraKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) override; // 163, 0x5738f0

  int SetExtraSystemKey(ObjectKey* value, NotifyContext* ctx); // 0x572e30
  int ApplyExtraKey(NotifyContext* ctx);                       // 0x573460 (ApplyExtraSystemKey)
  int ApplyAllBody(NotifyContext* ctx);                        // 0x573110: slot 9 behind its side-view guard
  int PickExtraSystems(GameObj* target, int count, int includeCore, int includeOuter, NotifyContext* ctx); // 0x573170

  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x573010
  static int BuildEventKinds();                                                  // 0x573940
  static GameObjNotifier* Create();                                              // 0x572fa0
};
