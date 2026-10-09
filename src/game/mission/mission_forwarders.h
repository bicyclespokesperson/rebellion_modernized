#pragma once
#include "core/reb_abi.h"
#include "gameobj/game_obj.h"

// Shared tiny bodies of the mission leaf classes (see mission_forwarders.cpp).
int REB_STDCALL ReturnOneTwoArgs(int, NotifyContext*);                                          // 0x524fb0
int REB_THISCALL MissionTypeCreateViewsForward(void* self, int controlKind, NotifyContext* ctx);  // 0x576250
