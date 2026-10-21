#pragma once
#include "gameobj/game_obj.h"

// Shared forwarder bodies of the leaf classes (see game_obj_forwarders.cpp).
int LeafReadForward(GameObj* self, istream* in);    // 0x569840
int LeafWriteForward(GameObj* self, ostream* out);  // 0x569850
int LeafChecksumForward(GameObj* self, Crc32* crc); // 0x569860
