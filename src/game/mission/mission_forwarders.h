#pragma once
#include "gameobj/game_obj.h"

// Shared tiny bodies of the mission leaf classes (see mission_forwarders.cpp).
int __stdcall ReturnOneTwoArgs(int, NotifyContext*);                                          // 0x524fb0
int __thiscall MissionTypeCreateViewsForward(void* self, int controlKind, NotifyContext* ctx);  // 0x576250
