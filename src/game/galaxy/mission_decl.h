#pragma once
#include <cstdint>

#include "gameobj/game_obj.h"
#include "mission/mission.h"

// Minimal declaration of the base of the per-type mission classes (ctor 0x593730, VT 0x66b0e0, secondary 0x66b0d8,
// 0xa8 bytes), which is owned by the mission module (it derives in turn from Mission, mission/mission.h, ctor
// 0x5208b0). Only what the type-0x42 mission needs; drop this header when the mission module provides one.
class MissionTypeBase : public Mission
{
  public:
  MissionTypeBase();           // 0x593730
  ~MissionTypeBase() override; // 0x593750 (deleting), 0x593770

  // Fills *out with the mission base's event kinds (the starting point of every type's static set).
  static int InitEventKinds(int unused, EventKindSet* out); // 0x593b20

  int GetEventKinds(int unused, EventKindSet* out) override;      // 15, 0x593790
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x5937b0
  int AddInitialMembers(ObjectKeyValueSet* team, ObjectKeyValueSet* decoys,
                        NotifyContext* ctx) override; // 120, 0x593870

  // Second-level forwarders of the leaf types' slot 37 / 9 / 7 bodies.
  int CreateViewsBody(int controlKind, NotifyContext* ctx); // 0x593a80
  int ApplyAllBody(NotifyContext* ctx);                     // 0x593aa0
  int ChecksumBody(Crc32* crc);                             // 0x593ad0

  static int BuildEventKinds(); // 0x593af0
};

// Bodies of the mission base's persistence slots, called by the leaf types' second-level forwarders.
int MissionTypeWriteForward(MissionTypeBase* self, ostream* out); // 0x56ec40: Write (shared by several mission types)
int MissionTypeApplyAllForward(MissionTypeBase* self, NotifyContext* ctx); // 0x56ec50: ApplyAll body (shared)
