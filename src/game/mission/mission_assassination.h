#pragma once
#include "mission/mission_leaf.h"

// MissionAssassination (family 0x63, ctor 0x5762f0, VT 0x668ac8, secondary ObjectBase VT 0x668ac4, 0xa8 bytes, no
// fields): the Assassinate mission against a character. Slots 5-9 and 37 are the shared forwarders.
class MissionAssassination : public MissionCharacterTargetBase
{
  public:
  MissionAssassination();           // 0x5762f0
  ~MissionAssassination() override; // 0x576310 (deleting), 0x576330

  uint32_t GetFamily() override;                                                           // 1, 0x5764c0: 0x63
  REB_MISSION_SHARED_PERSISTENCE_DECLS                                                     // 5-9, 37
      int GetEventKinds(int unused, EventKindSet* out) override;                           // 15, 0x576350
  int CheckOrder(int a, int orderKind, StatusCode* out) override;                          // 22, 0x576370
  int GetCheckFunctions(MissionCheckFunctions* out) override;                              // 111, 0x576430
  GameObj* CreateViewObject() override;                                                    // 57, 0x576450
  int GetPersonSuccessChance(GameObj* person, int* out) override;                          // 157, 0x5765c0
  int BeforeScoringResults(NotifyContext* ctx) override;                                   // 158, stub 0x51ebb0
  int ApplyPersonResult(GameObj* person, int roll, int* out, NotifyContext* ctx) override; // 159, 0x576620
  int AfterScoringResults(NotifyContext* ctx) override;                                    // 160, shared 0x576700

  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x576540
  static int BuildEventKinds();                                                  // 0x576730
  static GameObjNotifier* Create();                                              // 0x5764d0
};
