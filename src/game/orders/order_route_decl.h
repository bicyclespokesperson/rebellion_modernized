#pragma once
// STAND-IN declarations for code owned by neighbouring modules (the manufacturing
// manager 0x527000-0x52e9ff, commands 0x57d000-0x57f000), declared by their original addresses for the order classes
// that call them. Replace each by the owner's real declaration when the module is merged and drop this header.
#include <cstdint>

#include "core/containers.h"
#include "gameobj/game_obj.h"
#include "gameobj/game_obj_root.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "orders/command_decl.h"

class DatRecord;

// Manufacturing manager (family 0xa0-0xaf) pieces.
// The set of objects the manager's facility list holds (a static empty one for a sided object).
