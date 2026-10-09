#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "gameobj/object_key.h"

// Helpers shared by the AI classes (src/game/ai).

class DatRecord;
class GameObjNotifier;

namespace ai {

constexpr int kSideAlliance = 1;
constexpr int kSideEmpire = 2;

// 1 if `family` lies in [first, last).
inline bool InFamilies(uint32_t family, uint32_t first, uint32_t last) { return family >= first && family < last; }

// 1 if the key's family lies in [first, last).
inline int KeyInFamilies(const ObjectKey& key, uint32_t first, uint32_t last) {
    return key.Family() >= first && key.Family() < last;
}

// The side bit-view used by the AI for a side id: 1 and 2 map to themselves, anything else to 0.
inline int ViewOfSide(int side) { return side == kSideAlliance ? 1 : (side == kSideEmpire ? 2 : 0); }

}  // namespace ai

// ---- functions of other modules the AI calls ----------------------------------------------------------------------

// The data record table (0x567420 returns the table): the first record of the record type `type` (0x14 = capital
// ships, ...), null if the table has none. Records of a type are chained by their AvlNode successor pointer.
DatRecord* GetFirstRecordOfType(int type);  // 0x51cad0

// The data record of the side's research tier `tier` of the capital ship (0x28) / facility (0x2a) records, in *out;
// non-zero when the table has one (both are cdecl wrappers of 0x51cb60).
int GetTierRecord28(int view, int tier, DatRecord** out);      // 0x51cbd0
int GetTierRecord2a(int view, int tier, DatRecord** out);  // 0x51cc10

// Keys that name the story characters (0x4024d0, 0x4025f0, 0x402cf0). The key is passed by value on the stack.
int IsStoryKeyOf4(uint32_t key);  // 0x4024d0: 0x32000242, 0x30000240, 0x34000280, 0x35000281
int IsStoryKeyOf6(uint32_t key);  // 0x4025f0: the four above and 0x31000241, 0x33000243
int REB_STDCALL IsStoryKeyOf2(uint32_t key);  // 0x402cf0: 0x32000242, 0x35000281

class ObjectNameNode;
// The key of an object name node (copies its NullableKey): 0x42ec10.
ObjectKey* REB_THISCALL GetObjectNameNodeKey(ObjectNameNode* node, ObjectKey* out);

// The key of a data record (record type in the family byte, record id): 0x402e40.
ObjectKey* REB_THISCALL GetAiRecordKey(DatRecord* record, ObjectKey* out);
// The key of the data record of a game object, 0 if it has none: 0x402d80.
ObjectKey* REB_THISCALL GetObjectRecordKey(GameObjNotifier* object, ObjectKey* out);
// The key of a game object (family from its class, id from the node): 0x4025b0.
ObjectKey* REB_THISCALL GetObjectKey(GameObjNotifier* object, ObjectKey* out);

class Side;
class IntPair;
// A copy of the side's maintenance state pair (+0x58): 0x41bde0.
IntPair* REB_THISCALL GetSidePairAt58(Side* side, IntPair* out);
