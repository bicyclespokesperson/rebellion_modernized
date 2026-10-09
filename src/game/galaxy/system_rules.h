#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "galaxy/system.h"
#include "gameobj/child_iterator.h"
#include "gameobj/object_family.h"

// Helpers shared by the System rules sources (system_rules*.cpp).

namespace system_rules {

// Facility families that the rules iterate over by their sub-ranges.
constexpr FamilyRange kFacilities20To22(0x20, 0x22);
constexpr FamilyRange kFacilities22To23(0x22, 0x23);
constexpr FamilyRange kFacilities28To30(0x28, 0x30);
constexpr FamilyRange kFacilities2cTo2d(0x2c, 0x2d);
constexpr FamilyRange kFacilities2cTo30(0x2c, 0x30);
constexpr FamilyRange kEveryFamily(1, 0xff);

constexpr uint32_t kStateUsable = 1;
constexpr uint32_t kStateExisting = 3;
constexpr uint32_t kStateAny = 4;

// 1 for the Alliance, 2 for the Empire, 0 for neither (the side numbering of the unit-count helpers).
inline int PlayerSide(uint32_t controlSide) { return controlSide == 1 ? 1 : (controlSide == 2 ? 2 : 0); }

}  // namespace system_rules

// The key of the object that holds `object` (virtual GetOwnerKey, returned through *out).
ObjectKey* REB_THISCALL GetOwnerKeyValue(GameObjNotifier* object, ObjectKey* out);  // 0x50c640
// UpdateChildren over the system families (0x90-0x98) of `parent`.
int UpdateSystemFamilyChildren(GameObj* parent, NotifyContext* ctx);  // 0x50c8a0
