#pragma once
#include <cstdint>

#include "gameobj/object_key.h"

class GameObjNotifier;
class NotifyContext;
class Role;

// Functions of other modules that the encounter resolution calls. Drop when their owners declare them.

// 0x50c640: the key of the object that owns `self` (written to *out and returned).
ObjectKey* __thiscall GetOwnerKeyValue(GameObjNotifier* self, ObjectKey* out);

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
