#pragma once
#include "mission/mission_leaf.h"

// MissionDiplomacy (family 0x51, ctor 0x573c90, VT 0x667a18, secondary ObjectBase VT 0x667a14, 0xa8 bytes, no fields):
// the Diplomacy mission against a system. Slots 5-9 and 37 are the shared forwarders (see mission_leaf.h).
class MissionDiplomacy : public MissionSystemTargetBase
{
public:
  MissionDiplomacy(); // 0x573c90
  ~MissionDiplomacy() override; // 0x573cb0 (deleting), 0x573cd0

  uint32_t GetFamily() override; // 1, 0x573e60: 0x51
  REB_MISSION_SHARED_PERSISTENCE_DECLS // 5-9, 37
    int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x573cf0
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x573d10
  int GetCheckFunctions(MissionCheckFunctions* out) override; // 111, 0x573dd0
  GameObj* CreateViewObject() override; // 57, 0x573df0
  int Virtual157(GameObj* person, int* out) override; // 157, 0x573ff0
  int Virtual158(NotifyContext* ctx) override; // 158, stub 0x51ebb0
  int Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) override; // 159, 0x5740a0
  int Virtual160(NotifyContext* ctx) override; // 160, 0x574120

  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x573ee0
  static int BuildEventKinds(); // 0x5741d0
  static GameObjNotifier* Create(); // 0x573e70
};
