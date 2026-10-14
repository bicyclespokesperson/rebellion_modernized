#pragma once
#include "mission/mission_leaf.h"

// MissionSubdue (family 0x57, ctor 0x5698c0, VT 0x663b78, secondary ObjectBase VT 0x663b74, 0xa8 bytes, no fields): the
// Subdue Uprising mission against a system. Slots 5-9 and 37 are the shared forwarders (see mission_leaf.h).
class MissionSubdue : public MissionSystemTargetBase
{
public:
  MissionSubdue(); // 0x5698c0
  ~MissionSubdue() override; // 0x5698e0 (deleting), 0x569900

  uint32_t GetFamily() override; // 1, 0x569a90: 0x57
  REB_MISSION_SHARED_PERSISTENCE_DECLS // 5-9, 37
    int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x569920
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x569940
  int GetCheckFunctions(MissionCheckFunctions* out) override; // 111, 0x569a00
  GameObj* CreateViewObject() override; // 57, 0x569a20
  int GetPersonSuccessChance(GameObj* person, int* out) override; // 157, 0x569b90
  int BeforeScoringResults(NotifyContext* ctx) override; // 158, stub 0x51ebb0
  int ApplyPersonResult(GameObj* person, int roll, int* out, NotifyContext* ctx) override; // 159, 0x569c20
  int AfterScoringResults(NotifyContext* ctx) override; // 160, shared 0x576700

  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x569b10
  static int BuildEventKinds(); // 0x569d90
  static GameObjNotifier* Create(); // 0x569aa0
};
