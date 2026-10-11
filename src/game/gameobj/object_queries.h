#pragma once

#include "gameobj/object_database.h"
#include "gameobj/object_family.h"

// Master-view lookups restricted to a family range (0x504d50-0x507130). The names carry the half-open range
// [first, last) in hex. Three shapes exist per range, not every range has all three:
//
//   GetFirstMasterInFamilies_xx_yy()      the first visible master object of the range (null if none)
//   GetMasterInFamilies_xx_yy(key)        the master object named by `key`, even if destroyed; null if it is
//                                         unknown or its family lies outside the range
//   ResolveMasterInFamilies_xx_yy(key, &object, &missing, &status)
//                                         the lookup behind the previous one. Returns 0 if the object exists but its
//                                         family is outside the range (*object null, *status {1,-1}). Otherwise returns
//                                         1 with *object set (null if unknown) and *status = {1,-1} for a live object
//                                         or a copy of *missing for an unknown or destroyed one.

GameObjNotifier* GetFirstMasterInFamilies_01_ff();  // 0x504d50
GameObjNotifier* GetMasterInFamilies_01_ff(const ObjectKey* key);  // 0x504dc0
int ResolveMasterInFamilies_01_ff(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x504e60
GameObjNotifier* GetMasterInFamilies_14_1c(const ObjectKey* key);  // 0x504f70
int ResolveMasterInFamilies_14_1c(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x505010
GameObjNotifier* GetFirstMasterInFamilies_30_3c();  // 0x505120
GameObjNotifier* GetMasterInFamilies_30_3c(const ObjectKey* key);  // 0x505190
int ResolveMasterInFamilies_30_3c(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x505230
GameObjNotifier* GetFirstMasterInFamilies_10_40();  // 0x505340
int ResolveMasterInFamilies_14_20(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x5053b0
GameObjNotifier* GetFirstMasterInFamilies_10_20();  // 0x5054c0
GameObjNotifier* GetMasterInFamilies_20_30(const ObjectKey* key);  // 0x505530
int ResolveMasterInFamilies_20_30(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x5055d0
GameObjNotifier* GetFirstMasterInFamilies_28_2c();  // 0x5056e0
GameObjNotifier* GetMasterInFamilies_a0_b0(const ObjectKey* key);  // 0x505750
int ResolveMasterInFamilies_a0_b0(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x5057f0
GameObjNotifier* GetFirstMasterInFamilies_40_80();  // 0x505900
GameObjNotifier* GetMasterInFamilies_40_80(const ObjectKey* key);  // 0x505970
int ResolveMasterInFamilies_40_80(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x505a10
GameObjNotifier* GetMasterInFamilies_28_30(const ObjectKey* key);  // 0x505b20
int ResolveMasterInFamilies_28_30(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x505bc0
GameObjNotifier* GetFirstMasterInFamilies_30_40();  // 0x505cd0
GameObjNotifier* GetMasterInFamilies_30_40(const ObjectKey* key);  // 0x505d40
int ResolveMasterInFamilies_30_40(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x505de0
GameObjNotifier* GetFirstMasterInFamilies_90_98();  // 0x505ef0
GameObjNotifier* GetMasterInFamilies_90_98(const ObjectKey* key);  // 0x505f60
int ResolveMasterInFamilies_90_98(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x506000
GameObjNotifier* GetFirstMasterInFamilies_20_22();  // 0x506110
GameObjNotifier* GetFirstMasterInFamilies_90_92();  // 0x506180
GameObjNotifier* GetFirstMasterInFamilies_18_1c();  // 0x5061f0
GameObjNotifier* GetMasterInFamilies_18_1c(const ObjectKey* key);  // 0x506260
int ResolveMasterInFamilies_18_1c(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x506300
GameObjNotifier* GetFirstMasterInFamilies_08_10();  // 0x506410
GameObjNotifier* GetMasterInFamilies_08_10(const ObjectKey* key);  // 0x506480
int ResolveMasterInFamilies_08_10(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x506520
GameObjNotifier* GetFirstMasterInFamilies_2c_2d();  // 0x506630
GameObjNotifier* GetMasterInFamilies_38_3c(const ObjectKey* key);  // 0x5066a0
int ResolveMasterInFamilies_38_3c(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x506740
int ResolveMasterInFamilies_f8_f9(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x506850
GameObjNotifier* GetFirstMasterInFamilies_2d_2e();  // 0x506960
GameObjNotifier* GetFirstMasterInFamilies_80_90();  // 0x5069d0
GameObjNotifier* GetMasterInFamilies_80_90(const ObjectKey* key);  // 0x506a40
int ResolveMasterInFamilies_80_90(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x506ae0
int ResolveMasterInFamilies_f3_f4(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x506bf0
int ResolveMasterInFamilies_f9_fa(const ObjectKey* key, GameObjNotifier** out, const StatusCode* missing,
                                StatusCode* status);  // 0x506d00

// ---- the game root's cached master objects (0x506e10-0x506f40) ----
//
// The GameManager-owned game root (family 0xf1) keeps pointers to a few master objects; these read them, null-guarded
// against a missing game root. The jmp stubs 0x506ef0-0x506f40 forward to these.
GameObjNotifier* GetGameRootObjectCached();            // 0x506e10, the game root itself
GameObjNotifier* GetGameRootMember_b0();               // 0x506e20
GameObjNotifier* GetGameRootMember_b8();               // 0x506e40
GameObjNotifier* GetGameRootMember_bc();               // 0x506e60
GameObjNotifier* GetGameRootMember_c0();               // 0x506e80
// side 1 gives +0xc4, side 2 gives +0xc8, anything else 0.
GameObjNotifier* GetGameRootSideMember(int side);      // 0x506ea0
GameObjNotifier* GetGameRootSideMemberThunk(int side);  // 0x506f30
GameObjNotifier* GetGameRootMember_dc();               // 0x506ed0

// ---- masters with a fixed key ----
GameObjNotifier* GetMaster_33000243();  // 0x506f50
GameObjNotifier* GetMaster_31000241();  // 0x506fb0
GameObjNotifier* GetMaster_32000242();  // 0x507010
GameObjNotifier* GetMaster_35000281();  // 0x507070
GameObjNotifier* GetMaster_34000280();  // 0x5070d0
