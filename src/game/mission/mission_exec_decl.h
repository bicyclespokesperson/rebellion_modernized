#pragma once
#include <cstdint>

#include "character/character.h"
#include "character/role.h"
#include "data/random.h"
#include "fleet/capship.h"
#include "fleet/fleet_iterator.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_queries.h"
#include "mission/mission.h"
#include "mission/mission_bases.h"
#include "mission/mission_jedi_training.h"
#include "mission/mission_type_iterators.h"

// The mission-execution part of the mission manager (family 0xf8, ctor 0x541fc0, 0x74 bytes), range 0x545180-0x549af0.
// The class itself belongs to the story module: this header declares only what this slice needs and can be dropped
// once that declaration exists. Everything declared as a MissionMgr member here is reconstructed in
// mission_exec_*.cpp; every other original function this slice calls is declared below by its Ghidra default name
// (FUN_xxxxxxxx resolves to the original address at link time) and carries the exact stack signature of the original.
// Names of the MissionMgr members are provisional.

#include "mission/mission_rules.h"
#include "story/mission_mgr.h"

// ---- the rules object (game root member +0xbc, type 0xf9, 0x554020-0x557410) ----
