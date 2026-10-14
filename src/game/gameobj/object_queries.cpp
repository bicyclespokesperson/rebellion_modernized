#include "gameobj/object_queries.h"

#include "events/game_manager.h"
#include "gameobj/game_obj.h"
#include "gameobj/game_root.h"

namespace
{

constexpr uint32_t kDestroyedFlag = 0x8;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusUnknownObject = 0x12;

GameRoot* currentGameRoot()
{
  GameManager* manager = GetGameManager();
  return manager ? manager->Root() : nullptr;
}

bool InFamilyRange(uint32_t family, uint32_t first, uint32_t last)
{
  return family >= first && family < last;
}

GameObjNotifier* FirstMasterIn(uint32_t first, uint32_t last)
{
  const FamilyRange range(first, last);
  return GetFirstMasterObjectInRange(&range);
}

int ResolveMasterIn(uint32_t first,
                    uint32_t last,
                    const ObjectKey* key,
                    GameObjNotifier** out,
                    const StatusCode* missing,
                    StatusCode* status)
{
  *out = nullptr;
  *status = StatusCode(kStatusCategory, kStatusOk);
  GameObjNotifier* object = GetMasterObject(key);
  if (object && !InFamilyRange(object->GetFamily(), first, last))
    return 0;
  *out = object;
  const StatusCode found(kStatusCategory, kStatusOk);
  const bool alive = object && !(static_cast<GameObj*>(object)->m_stateFlags & kDestroyedFlag);
  *status = alive ? found : *missing;
  return 1;
}

GameObjNotifier* GetMasterIn(uint32_t first, uint32_t last, const ObjectKey* key)
{
  GameObjNotifier* object = nullptr;
  const StatusCode missing(kStatusCategory, kStatusUnknownObject);
  StatusCode status;
  ResolveMasterIn(first, last, key, &object, &missing, &status);
  return object;
}

} // namespace

GameObjNotifier* GetFirstMasterInFamilies_01_ff()
{
  return FirstMasterIn(0x1, 0xff);
}

GameObjNotifier* GetMasterInFamilies_01_ff(const ObjectKey* key)
{
  return GetMasterIn(0x1, 0xff, key);
}

int ResolveMasterInFamilies_01_ff(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0x1, 0xff, key, out, missing, status);
}

GameObjNotifier* GetMasterInFamilies_14_1c(const ObjectKey* key)
{
  return GetMasterIn(0x14, 0x1c, key);
}

int ResolveMasterInFamilies_14_1c(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0x14, 0x1c, key, out, missing, status);
}

GameObjNotifier* GetFirstMasterInFamilies_30_3c()
{
  return FirstMasterIn(0x30, 0x3c);
}

GameObjNotifier* GetMasterInFamilies_30_3c(const ObjectKey* key)
{
  return GetMasterIn(0x30, 0x3c, key);
}

int ResolveMasterInFamilies_30_3c(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0x30, 0x3c, key, out, missing, status);
}

GameObjNotifier* GetFirstMasterInFamilies_10_40()
{
  return FirstMasterIn(0x10, 0x40);
}

int ResolveMasterInFamilies_14_20(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0x14, 0x20, key, out, missing, status);
}

GameObjNotifier* GetFirstMasterInFamilies_10_20()
{
  return FirstMasterIn(0x10, 0x20);
}

GameObjNotifier* GetMasterInFamilies_20_30(const ObjectKey* key)
{
  return GetMasterIn(0x20, 0x30, key);
}

int ResolveMasterInFamilies_20_30(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0x20, 0x30, key, out, missing, status);
}

GameObjNotifier* GetFirstMasterInFamilies_28_2c()
{
  return FirstMasterIn(0x28, 0x2c);
}

GameObjNotifier* GetMasterInFamilies_a0_b0(const ObjectKey* key)
{
  return GetMasterIn(0xa0, 0xb0, key);
}

int ResolveMasterInFamilies_a0_b0(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0xa0, 0xb0, key, out, missing, status);
}

GameObjNotifier* GetFirstMasterInFamilies_40_80()
{
  return FirstMasterIn(0x40, 0x80);
}

GameObjNotifier* GetMasterInFamilies_40_80(const ObjectKey* key)
{
  return GetMasterIn(0x40, 0x80, key);
}

int ResolveMasterInFamilies_40_80(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0x40, 0x80, key, out, missing, status);
}

GameObjNotifier* GetMasterInFamilies_28_30(const ObjectKey* key)
{
  return GetMasterIn(0x28, 0x30, key);
}

int ResolveMasterInFamilies_28_30(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0x28, 0x30, key, out, missing, status);
}

GameObjNotifier* GetFirstMasterInFamilies_30_40()
{
  return FirstMasterIn(0x30, 0x40);
}

GameObjNotifier* GetMasterInFamilies_30_40(const ObjectKey* key)
{
  return GetMasterIn(0x30, 0x40, key);
}

int ResolveMasterInFamilies_30_40(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0x30, 0x40, key, out, missing, status);
}

GameObjNotifier* GetFirstMasterInFamilies_90_98()
{
  return FirstMasterIn(0x90, 0x98);
}

GameObjNotifier* GetMasterInFamilies_90_98(const ObjectKey* key)
{
  return GetMasterIn(0x90, 0x98, key);
}

int ResolveMasterInFamilies_90_98(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0x90, 0x98, key, out, missing, status);
}

GameObjNotifier* GetFirstMasterInFamilies_20_22()
{
  return FirstMasterIn(0x20, 0x22);
}

GameObjNotifier* GetFirstMasterInFamilies_90_92()
{
  return FirstMasterIn(0x90, 0x92);
}

GameObjNotifier* GetFirstMasterInFamilies_18_1c()
{
  return FirstMasterIn(0x18, 0x1c);
}

GameObjNotifier* GetMasterInFamilies_18_1c(const ObjectKey* key)
{
  return GetMasterIn(0x18, 0x1c, key);
}

int ResolveMasterInFamilies_18_1c(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0x18, 0x1c, key, out, missing, status);
}

GameObjNotifier* GetFirstMasterInFamilies_08_10()
{
  return FirstMasterIn(0x8, 0x10);
}

GameObjNotifier* GetMasterInFamilies_08_10(const ObjectKey* key)
{
  return GetMasterIn(0x8, 0x10, key);
}

int ResolveMasterInFamilies_08_10(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0x8, 0x10, key, out, missing, status);
}

GameObjNotifier* GetFirstMasterInFamilies_2c_2d()
{
  return FirstMasterIn(0x2c, 0x2d);
}

GameObjNotifier* GetMasterInFamilies_38_3c(const ObjectKey* key)
{
  return GetMasterIn(0x38, 0x3c, key);
}

int ResolveMasterInFamilies_38_3c(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0x38, 0x3c, key, out, missing, status);
}

int ResolveMasterInFamilies_f8_f9(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0xf8, 0xf9, key, out, missing, status);
}

GameObjNotifier* GetFirstMasterInFamilies_2d_2e()
{
  return FirstMasterIn(0x2d, 0x2e);
}

GameObjNotifier* GetFirstMasterInFamilies_80_90()
{
  return FirstMasterIn(0x80, 0x90);
}

GameObjNotifier* GetMasterInFamilies_80_90(const ObjectKey* key)
{
  return GetMasterIn(0x80, 0x90, key);
}

int ResolveMasterInFamilies_80_90(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0x80, 0x90, key, out, missing, status);
}

int ResolveMasterInFamilies_f3_f4(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0xf3, 0xf4, key, out, missing, status);
}

int ResolveMasterInFamilies_f9_fa(const ObjectKey* key,
                                  GameObjNotifier** out,
                                  const StatusCode* missing,
                                  StatusCode* status)
{
  return ResolveMasterIn(0xf9, 0xfa, key, out, missing, status);
}

namespace
{

GameObjNotifier* ReadCached(GameObjNotifier* GameRoot::* member)
{
  GameRoot* root = currentGameRoot();
  return root ? root->*member : nullptr;
}

GameObjNotifier* GetMasterWithKey(uint32_t family, uint32_t id)
{
  const ObjectKey key = ObjectKey::Make(family, id);
  return GetMasterObject(&key);
}

} // namespace

GameObjNotifier* GetGameRootObjectCached()
{
  return currentGameRoot();
}

GameObjNotifier* GetGameRootMember_b0()
{
  return ReadCached(&GameRoot::m_master_f2000003);
}

GameObjNotifier* GetGameRootMember_b8()
{
  return ReadCached(&GameRoot::m_master_f8000006);
}

GameObjNotifier* GetGameRootMember_bc()
{
  return ReadCached(&GameRoot::m_master_f9000007);
}

GameObjNotifier* GetGameRootMember_c0()
{
  return ReadCached(&GameRoot::m_master_fa000008);
}

GameObjNotifier* GetGameRootSideMember(int side)
{
  GameRoot* root = currentGameRoot();
  if (side < 1 || side > 2 || !root)
    return nullptr;
  return side == 1 ? root->m_master_f300000f : root->m_master_f3000010;
}

GameObjNotifier* GetGameRootSideMemberThunk(int side)
{
  return GetGameRootSideMember(side);
}

GameObjNotifier* GetGameRootMember_dc()
{
  return ReadCached(&GameRoot::m_master_98000481);
}

GameObjNotifier* GetMaster_33000243()
{
  return GetMasterWithKey(0x33, 0x243);
}

GameObjNotifier* GetMaster_31000241()
{
  return GetMasterWithKey(0x31, 0x241);
}

GameObjNotifier* GetMaster_32000242()
{
  return GetMasterWithKey(0x32, 0x242);
}

GameObjNotifier* GetMaster_35000281()
{
  return GetMasterWithKey(0x35, 0x281);
}

GameObjNotifier* GetMaster_34000280()
{
  return GetMasterWithKey(0x34, 0x280);
}
