#pragma once
#include <cstdint>

#include "gameobj/object_key.h"

class GameObjNotifier;
class NotifyContext;
class Role;

// Functions of other modules that the encounter resolution calls. Drop when their owners declare them.

// 0x50c640: the key of the object that owns `self` (written to *out and returned).
ObjectKey* __thiscall GetOwnerKeyValue(GameObjNotifier* self, ObjectKey* out);

// CharacterMgr (game root member 0xc0) methods, 0x560200 and 0x55ff60: the fight of two persons of an encounter and the
// force detection of one by another.
int __thiscall FUN_00560200(GameObjNotifier* manager, Role* member, Role* other, NotifyContext* ctx);
int __thiscall FUN_0055ff60(GameObjNotifier* manager, Role* detected, Role* detector, NotifyContext* ctx);

// Role::RollCombatCheck (0x5349e0): its first argument is the key of the opponent, not an int.
int __thiscall FUN_005349e0(Role* role, ObjectKey* opponent, int skill, NotifyContext* ctx);

#include "gameobj/child_iterator.h"

// Iterators of the major / minor characters of an object (families 0x30-0x38 and 0x38-0x3c), owned by the character
// modules.
class MajorCharacterIterator : public ChildIterator {
public:
    MajorCharacterIterator(GameObjNotifier* parent, uint32_t stateFilter);  // 0x5947c0
    virtual ~MajorCharacterIterator();                                        // 0x594840
};

#include "story/story_characters.h"

namespace mission_encounter {

constexpr uint32_t kStateUsable = 1;

}  // namespace mission_encounter
