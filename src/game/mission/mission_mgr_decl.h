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
int __thiscall FUN_005429e0(MissionMgr* self, const ActivityRequest* request, int a, int b, ObjectKeyValueSet* team,
                            ObjectKeyValueSet* characters, StatusCode* status);
int __thiscall FUN_00542b60(MissionMgr* self, const ActivityRequest* request, int a, int b, ObjectKey* missionKey,
                            ObjectKey* secondKey, ObjectKeyValueSet* team, ObjectKeyValueSet* characters,
                            StatusCode* status);
int __thiscall FUN_00546e70(MissionMgr* self, ObjectKey* key, int value, NotifyContext* ctx);
int __thiscall FUN_00546ea0(MissionMgr* self, ObjectKey* key, int a, int b, NotifyContext* ctx);
int __thiscall FUN_00546f80(MissionMgr* self, NotifyContext* ctx);
int __thiscall FUN_00546fa0(MissionMgr* self, NotifyContext* ctx);

class Mission;
// Mission::ResetImpliedTeamIfPhase8 / ResumeFromPhase11 take the notify context (mission.h declares the first without).
int __thiscall FUN_00522a10(Mission* self, NotifyContext* ctx);
int __thiscall FUN_00522a30(Mission* self, NotifyContext* ctx);
