#pragma once
// STAND-IN declarations for the mission manager (family 0xf8, 0x541fc0-0x54cd80) that the mission commands, timers and
// events call. Drop each part when its owner reconstructs it.
#include <cstdint>

#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"

// ---- the mission manager (family 0xf8, key 0xf8000006), reached through the game root's cached master --------------

class MissionMgr;
// Slot 111 of the mission manager creates a mission; 0x5429e0 / 0x542b60 are the two checks that precede it.
constexpr int kMissionMgrCreateMissionSlot = 111;

class Mission;
