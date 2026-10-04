#pragma once
#include <cstdint>

#include "core/layout.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "galaxy/system.h"
#include "gameobj/status_code.h"

// Galaxy seeding of a new game (0x518b20-0x51c040): the setup stages the game root's state machine (GameRoot::StepSetup,
// 0x5136d0) runs, and the helpers they share. The GameRoot members of this range are declared in game_root.h.
//
// A SeedRule is one row of a seeding table (the table class itself lives with the rules code at 0x518a70): it says
// "systems of families [m_familyFirst, m_familyLast) that belong to side m_controlSide (0 = any) should hold at least
// m_target units of kind m_unitKind; deficits are filled by posting a command (class 0x562c70) with the signed
// m_amount". Only the fields the seeding reads are named.
struct SeedRule {
    // Copies the family range into *out (0x51bf10); returns out.
    IntPair* __thiscall GetFamilyRange(IntPair* out);  // 0x51bf10

    int m_controlSide;       // +0x00, 0 or 1-3
    IntPair m_familyRange;   // +0x04
    int m_unitKind;          // +0x0c, 1 or 2
    int m_target;            // +0x10
    int m_direction;         // +0x14, 1-3; anything but 1 negates the amount
    int m_amount;            // +0x18, 0 disables the rule
};

// Applies `rule` to every system of `sector` (0x518b20). Returns 1 if all of them succeeded.
int ApplySeedRuleToSector(GameObjNotifier* sector, const SeedRule* rule, NotifyContext* ctx);  // 0x518b20
// Posts the command that makes `system` meet `rule` (0x518be0). Returns 1 when nothing was posted, otherwise the new
// command's id (0 if it had none and was deleted again).
uint32_t ApplySeedRuleToSystem(System* system, const SeedRule* rule, NotifyContext* ctx);  // 0x518be0
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
