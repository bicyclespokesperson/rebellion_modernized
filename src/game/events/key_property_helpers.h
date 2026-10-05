#pragma once
// Five small callbacks (0x4f5d10-0x4f5de0) that sit after the SidedMessage class in the binary. They are entries of a
// callback table at 0x65d234 for the object-key properties (0x5ae1e0 / 0x4f5d10 / 0x5ae1e0 / 0x4f5d60 / 0x4f5d80 /
// 0x4f5dd0 / 0x4f5de0, then the limits 0xff, 0xff); the table's owner is not identified yet. The two getters are
// function-local statics in the original, built on first use and destroyed at exit.
#include "gameobj/object_key.h"

// A shared empty ObjectKeyValueSet (static at 0x6b2960).
ObjectKeyValueSet* GetEmptyKeyValueSet();  // 0x4f5d10
// Constructs an unset key in place and returns it.
ObjectKey* __stdcall ConstructUnsetKey(ObjectKey* key);  // 0x4f5d60
// A shared unset ObjectKey (static at 0x6b2958).
ObjectKey* GetEmptyKey();  // 0x4f5d80
void __stdcall ResetKey(ObjectKey* key);  // 0x4f5dd0
// Calls virtual slot 1 of the object and returns 1.
int __stdcall CallSlot1(void* object);  // 0x4f5de0
