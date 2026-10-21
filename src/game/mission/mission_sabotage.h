#pragma once
#include "mission/mission_leaf.h"

// MissionSabotage (family 0x69, ctor 0x569ec0, VT 0x663e08, secondary ObjectBase VT 0x663e04, 0xa8 bytes, no fields):
// the Sabotage mission against a game object (MissionObjectTargetBase). Slots 5-9 and 37 are the shared forwarders.
class MissionSabotage : public MissionObjectTargetBase
{
  public:
  MissionSabotage();           // 0x569ec0
  ~MissionSabotage() override; // 0x569ee0 (deleting), 0x569f00

  uint32_t GetFamily() override;                                                           // 1, 0x56a090: 0x69
  REB_MISSION_SHARED_PERSISTENCE_DECLS                                                     // 5-9, 37 (0x574420)
      int GetEventKinds(int unused, EventKindSet* out) override;                           // 15, 0x569f20
  int CheckOrder(int a, int orderKind, StatusCode* out) override;                          // 22, 0x569f40
  int GetCheckFunctions(MissionCheckFunctions* out) override;                              // 111, 0x56a000
  GameObj* CreateViewObject() override;                                                    // 57, 0x56a020
  int GetPersonSuccessChance(GameObj* person, int* out) override;                          // 157, 0x56a2d0
  int BeforeScoringResults(NotifyContext* ctx) override;                                   // 158, stub 0x51ebb0
  int ApplyPersonResult(GameObj* person, int roll, int* out, NotifyContext* ctx) override; // 159, 0x56a300
  int AfterScoringResults(NotifyContext* ctx) override;                                    // 160, shared 0x5746e0

  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x56a110
  static int BuildEventKinds();                                                  // 0x56a3b0
  static GameObjNotifier* Create();                                              // 0x56a0a0
};
