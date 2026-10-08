#pragma once
// Small helpers shared by the notice classes (events/player_notices.h).
#include <cstdint>

#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"

namespace notice {

// The ObjectBase subobject the text templates take for an object argument; null stays null.
inline ObjectBase* nameOf(GameObjNotifier* object) { return object ? static_cast<GameObj*>(object) : nullptr; }

inline bool inFamilies(const ObjectKey& key, uint32_t first, uint32_t last) { return key.Family() >= first && key.Family() < last; }

// The object itself or the first one above it whose family is a system (0x90-0x97); its key is left in *key.
inline GameObjNotifier* containingSystem(GameObjNotifier* start, ObjectKey* key) {
    GameObjNotifier* object = start;
    while (object) {
        object->GetKey(key);
        if (inFamilies(*key, 0x90, 0x98)) break;
        object = object->m_parent;
    }
    return object;
}

}  // namespace notice
