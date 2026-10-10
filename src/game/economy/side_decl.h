#pragma once
// Minimal declarations of classes and functions owned by other modules (ManuMgr, facilities, galaxy, notification
// hubs, rules). Dropped at merge once the owning modules provide real headers.
#include <cstdint>

#include "core/containers.h"
#include "data/gnprtb.h"
#include "data/random.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "gameobj/unit_obj_base.h"

// Member of the game root's object at +0xbc: finds the master that owns `object`.

// A manufacturing manager (family 0xa0-0xaf, ManuMgr module).
class ManuMgr;

// Rules/object-type database wrappers (0x51c-0x51d).
class SdRecord;
