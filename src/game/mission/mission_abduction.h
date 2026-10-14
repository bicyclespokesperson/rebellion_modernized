#pragma once
#include "mission/mission_leaf.h"

// MissionAbduction (family 0x62, ctor 0x576b40, VT 0x668fd0, secondary ObjectBase VT 0x668fcc, 0xa8 bytes, no
// fields): the Abduct mission against a character. Slots 5-9 and 37 are the shared forwarders.
class MissionAbduction : public MissionCharacterTargetBase
{
public:
  MissionAbduction(); // 0x576b40
  ~MissionAbduction() override; // 0x576b60 (deleting), 0x576b80

  uint32_t GetFamily() override; // 1, 0x576d50: 0x62
  REB_MISSION_SHARED_PERSISTENCE_DECLS // 5-9, 37
    int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x576ba0
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x576bc0
  int GetCheckFunctions(MissionCheckFunctions* out) override; // 111, 0x576c80
  GameObj* CreateViewObject() override; // 57, 0x576ca0
  int GetPersonSuccessChance(GameObj* person, int* out) override; // 157, 0x576e50
  int BeforeScoringResults(NotifyContext* ctx) override; // 158, stub 0x51ebb0
  int ApplyPersonResult(GameObj* person, int roll, int* out, NotifyContext* ctx) override; // 159, 0x576eb0
  int AfterScoringResults(NotifyContext* ctx) override; // 160, shared 0x576700

  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x576dd0
  static int BuildEventKinds(); // 0x576fd0
  static GameObjNotifier* Create(); // 0x576d60
};
