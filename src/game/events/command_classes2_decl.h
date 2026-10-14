#pragma once
// STAND-IN declarations for the code the command_classes2 classes call (modules owned elsewhere); drop a
// declaration when the owning module's header provides it.
#include <cstdint>

#include "gameobj/game_hooks.h"
#include "gameobj/object_key.h"

// The manager's key at +0x7c (copies it into *out).
// The data record of a key (via the object-type database).
// Fetches the side object that controls `manager` into *out (null for neutral). Returns 0 only if that object is missing.
