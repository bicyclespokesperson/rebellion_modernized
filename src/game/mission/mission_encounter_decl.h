#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "gameobj/object_key.h"

class GameObjNotifier;
class NotifyContext;
class Role;

// Functions of other modules that the encounter resolution calls. Drop when their owners declare them.

// 0x50c640: the key of the object that owns `self` (written to *out and returned).
ObjectKey* REB_THISCALL GetOwnerKeyValue(GameObjNotifier* self, ObjectKey* out);

#include "events/misc_helpers.h"

#include "story/story_characters.h"

namespace mission_encounter {

constexpr uint32_t kStateUsable = 1;

}  // namespace mission_encounter
