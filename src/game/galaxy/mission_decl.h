#pragma once
#include <cstdint>

#include "core/layout.h"
#include "gameobj/game_obj.h"
#include "mission/mission.h"

// Minimal declaration of the base of the per-type mission classes (ctor 0x593730, VT 0x66b0e0, secondary 0x66b0d8,
// 0xa8 bytes), which is owned by the mission module (it derives in turn from Mission, mission/mission.h, ctor 0x5208b0).
// Only what the type-0x42 mission needs; drop this header when the mission module provides one.
class MissionTypeBase : public Mission {
public:
    MissionTypeBase();           // 0x593730
    virtual ~MissionTypeBase();  // 0x593750 (deleting), 0x593770

    // Fills *out with the mission base's event kinds (the starting point of every type's static set).
    static int InitEventKinds(int unused, EventKindSet* out);  // 0x593b20

    virtual int Virtual120() override;  // 120, 0x593870
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(MissionTypeBase, 0xa8);
#pragma clang diagnostic pop

// Bodies of the mission base's persistence slots, called by the leaf types' second-level forwarders.
int __thiscall FUN_00592c50(MissionTypeBase* self, istream* in);     // Read
int __thiscall FUN_00593ad0(MissionTypeBase* self, Crc32* crc);      // Checksum
int __thiscall FUN_00523c60(MissionTypeBase* self);                  // Validate
int __thiscall FUN_0056ec40(MissionTypeBase* self, ostream* out);    // Write (shared by several mission types)
int __thiscall FUN_0056ec50(MissionTypeBase* self, NotifyContext* ctx);  // ApplyAll body (shared)
int __thiscall FUN_00576250(MissionTypeBase* self, int controlKind, NotifyContext* ctx);  // CreateViews (shared)
