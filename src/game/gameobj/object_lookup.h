#pragma once
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"

// Typed object lookups over the object database (0x4f2d10-0x4f3f10). Each is a thin wrapper that fixes the family
// range of the lookup; the names carry the half-open range [first, last) in hex. `view` is the object view (1 =
// Alliance, 2 = Empire; any other value finds nothing).
//
// GetObjectInFamilies_xx_yy(view, key): the object named by `key`, or null if its family is outside the range.
// GetFirstObjectInFamilies_xx_yy(view): the first (lowest key) visible object of the range.
//
// Families (see code_map.md): fleets 0x08, troop regiments 0x10-0x13, capital ships 0x14-0x1b, fighter squadrons
// 0x1c-0x1f, facilities 0x20-0x2f, characters 0x30-0x3b, special forces 0x3c-0x3f, missions 0x40-0x7f, sectors
// 0x80-0x8f, systems 0x90-0x97, manufacturing managers 0xa0-0xaf, game root 0xf1, side roots 0xf2, sides 0xf3.

GameObjNotifier* GetObjectInFamilies_01_ff(int view, const ObjectKey* key);
GameObjNotifier* GetObjectInFamilies_14_1c(int view, const ObjectKey* key);
GameObjNotifier* GetObjectInFamilies_30_3c(int view, const ObjectKey* key);
GameObjNotifier* GetObjectInFamilies_a0_b0(int view, const ObjectKey* key);
GameObjNotifier* GetObjectInFamilies_40_80(int view, const ObjectKey* key);
GameObjNotifier* GetObjectInFamilies_30_40(int view, const ObjectKey* key);
GameObjNotifier* GetObjectInFamilies_90_98(int view, const ObjectKey* key);
GameObjNotifier* GetObjectInFamilies_20_22(int view, const ObjectKey* key);
GameObjNotifier* GetObjectInFamilies_1c_20(int view, const ObjectKey* key);
GameObjNotifier* GetObjectInFamilies_08_10(int view, const ObjectKey* key);
GameObjNotifier* GetObjectInFamilies_80_90(int view, const ObjectKey* key);
GameObjNotifier* GetObjectInFamilies_3c_40(int view, const ObjectKey* key);
GameObjNotifier* GetFirstObjectInFamilies_14_1c(int view);
GameObjNotifier* GetFirstObjectInFamilies_30_40(int view);
GameObjNotifier* GetFirstObjectInFamilies_90_98(int view);
GameObjNotifier* GetFirstObjectInFamilies_20_22(int view);
GameObjNotifier* GetFirstObjectInFamilies_2a_2b(int view);
GameObjNotifier* GetFirstObjectInFamilies_25_26(int view);
GameObjNotifier* GetFirstObjectInFamilies_18_1c(int view);
GameObjNotifier* GetFirstObjectInFamilies_1c_20(int view);
GameObjNotifier* GetFirstObjectInFamilies_08_10(int view);
GameObjNotifier* GetFirstObjectInFamilies_22_23(int view);
GameObjNotifier* GetFirstObjectInFamilies_2c_2d(int view);
GameObjNotifier* GetFirstObjectInFamilies_24_25(int view);
GameObjNotifier* GetFirstObjectInFamilies_2d_2e(int view);
GameObjNotifier* GetFirstObjectInFamilies_80_90(int view);
GameObjNotifier* GetFirstObjectInFamilies_28_29(int view);
GameObjNotifier* GetFirstObjectInFamilies_3c_40(int view);
GameObjNotifier* GetFirstObjectInFamilies_14_18(int view);
GameObjNotifier* GetFirstObjectInFamilies_29_2a(int view);
GameObjNotifier* GetFirstObjectInFamilies_10_14(int view);
GameObjNotifier* GetFirstObjectInFamilies_23_24(int view);
GameObjNotifier* GetGameRootObject(int view);
GameObjNotifier* GetObject_32000242(int view);
GameObjNotifier* GetObject_35000281(int view);
GameObjNotifier* GetObject_34000280(int view);
GameObjNotifier* GetSideObject(int view, int side);
GameObjNotifier* GetFirstObjectInRangeForSide(int view, const FamilyRange* range, int resolve);
