#pragma once
#include "core/reb_abi.h"
#include "gameobj/game_obj.h"

// Shared forwarder bodies of the leaf classes (see game_obj_forwarders.cpp).
int REB_THISCALL LeafReadForward(GameObj* self, istream* in);       // 0x569840
int REB_THISCALL LeafWriteForward(GameObj* self, ostream* out);     // 0x569850
int REB_THISCALL LeafChecksumForward(GameObj* self, Crc32* crc);    // 0x569860
