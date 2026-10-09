#pragma once
#include <cstdint>

#include "core/layout.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "galaxy/system.h"
#include "gameobj/status_code.h"
#include "rules/loyalty_rule.h"

// Galaxy seeding of a new game (0x518b20-0x51c040): the setup stages the game root's state machine (GameRoot::StepSetup,
// 0x5136d0) runs, and the helpers they share. The GameRoot members of this range are declared in game_root.h.
//
// Applies `rule` to every system of `sector` (0x518b20). Returns 1 if all of them succeeded.
int ApplyLoyaltyRuleToSector(GameObjNotifier* sector, const LoyaltyRule* rule, NotifyContext* ctx);  // 0x518b20
// Posts the command that makes `system` meet `rule` (0x518be0). Returns 1 when nothing was posted, otherwise the new
// command's id (0 if it had none and was deleted again).
uint32_t ApplyLoyaltyRuleToSystem(System* system, const LoyaltyRule* rule, NotifyContext* ctx);  // 0x518be0
// The sector master that follows `sector` (0x518ff0).
GameObjNotifier* __thiscall GetNextSectorMaster(GameObjNotifier* sector);  // 0x518ff0
// The system master that follows `system` (0x51a5d0).
GameObjNotifier* __thiscall NextSystemMaster(GameObjNotifier* system);

// Setup stage number accessors (the global at 0x6b2bb4; stage 0x1a marks setup complete).
int GetSetupStage();            // 0x51c000
void SetSetupStage(int stage);  // 0x51c010
void AdvanceSetupStage();       // 0x51c030
// Installed as g_gameHook2AEC: hands out the next object id from the game root's counter (+0x6c), 0 without a root.
int AllocateObjectId();         // 0x51c040
