#include "gameobj/object_lookup.h"

namespace
{
bool InFamilyRange(uint32_t family, uint32_t first, uint32_t last)
{
  return family >= first && family < last;
}
} // namespace

GameObjNotifier* GetObjectInFamilies_01_ff(int view, const ObjectKey* key)
{
  GameObjNotifier* object = GetObjectForView(key, view);
  if (object && !InFamilyRange(object->GetFamily(), 0x1, 0xff))
    object = nullptr;
  return object;
}

GameObjNotifier* GetObjectInFamilies_14_1c(int view, const ObjectKey* key)
{
  GameObjNotifier* object = GetObjectForView(key, view);
  if (object && !InFamilyRange(object->GetFamily(), 0x14, 0x1c))
    object = nullptr;
  return object;
}

GameObjNotifier* GetObjectInFamilies_30_3c(int view, const ObjectKey* key)
{
  GameObjNotifier* object = GetObjectForView(key, view);
  if (object && !InFamilyRange(object->GetFamily(), 0x30, 0x3c))
    object = nullptr;
  return object;
}

GameObjNotifier* GetObjectInFamilies_a0_b0(int view, const ObjectKey* key)
{
  GameObjNotifier* object = GetObjectForView(key, view);
  if (object && !InFamilyRange(object->GetFamily(), 0xa0, 0xb0))
    object = nullptr;
  return object;
}

GameObjNotifier* GetObjectInFamilies_40_80(int view, const ObjectKey* key)
{
  GameObjNotifier* object = GetObjectForView(key, view);
  if (object && !InFamilyRange(object->GetFamily(), 0x40, 0x80))
    object = nullptr;
  return object;
}

GameObjNotifier* GetObjectInFamilies_30_40(int view, const ObjectKey* key)
{
  GameObjNotifier* object = GetObjectForView(key, view);
  if (object && !InFamilyRange(object->GetFamily(), 0x30, 0x40))
    object = nullptr;
  return object;
}

GameObjNotifier* GetObjectInFamilies_90_98(int view, const ObjectKey* key)
{
  GameObjNotifier* object = GetObjectForView(key, view);
  if (object && !InFamilyRange(object->GetFamily(), 0x90, 0x98))
    object = nullptr;
  return object;
}

GameObjNotifier* GetObjectInFamilies_20_22(int view, const ObjectKey* key)
{
  GameObjNotifier* object = GetObjectForView(key, view);
  if (object && !InFamilyRange(object->GetFamily(), 0x20, 0x22))
    object = nullptr;
  return object;
}

GameObjNotifier* GetObjectInFamilies_1c_20(int view, const ObjectKey* key)
{
  GameObjNotifier* object = GetObjectForView(key, view);
  if (object && !InFamilyRange(object->GetFamily(), 0x1c, 0x20))
    object = nullptr;
  return object;
}

GameObjNotifier* GetObjectInFamilies_08_10(int view, const ObjectKey* key)
{
  GameObjNotifier* object = GetObjectForView(key, view);
  if (object && !InFamilyRange(object->GetFamily(), 0x8, 0x10))
    object = nullptr;
  return object;
}

GameObjNotifier* GetObjectInFamilies_80_90(int view, const ObjectKey* key)
{
  GameObjNotifier* object = GetObjectForView(key, view);
  if (object && !InFamilyRange(object->GetFamily(), 0x80, 0x90))
    object = nullptr;
  return object;
}

GameObjNotifier* GetObjectInFamilies_3c_40(int view, const ObjectKey* key)
{
  GameObjNotifier* object = GetObjectForView(key, view);
  if (object && !InFamilyRange(object->GetFamily(), 0x3c, 0x40))
    object = nullptr;
  return object;
}

GameObjNotifier* GetFirstObjectInFamilies_14_1c(int view)
{
  const FamilyRange range = {0x14, 0x1c};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_30_40(int view)
{
  const FamilyRange range = {0x30, 0x40};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_90_98(int view)
{
  const FamilyRange range = {0x90, 0x98};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_20_22(int view)
{
  const FamilyRange range = {0x20, 0x22};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_2a_2b(int view)
{
  const FamilyRange range = {0x2a, 0x2b};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_25_26(int view)
{
  const FamilyRange range = {0x25, 0x26};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_18_1c(int view)
{
  const FamilyRange range = {0x18, 0x1c};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_1c_20(int view)
{
  const FamilyRange range = {0x1c, 0x20};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_08_10(int view)
{
  const FamilyRange range = {0x8, 0x10};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_22_23(int view)
{
  const FamilyRange range = {0x22, 0x23};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_2c_2d(int view)
{
  const FamilyRange range = {0x2c, 0x2d};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_24_25(int view)
{
  const FamilyRange range = {0x24, 0x25};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_2d_2e(int view)
{
  const FamilyRange range = {0x2d, 0x2e};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_80_90(int view)
{
  const FamilyRange range = {0x80, 0x90};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_28_29(int view)
{
  const FamilyRange range = {0x28, 0x29};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_3c_40(int view)
{
  const FamilyRange range = {0x3c, 0x40};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_14_18(int view)
{
  const FamilyRange range = {0x14, 0x18};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_29_2a(int view)
{
  const FamilyRange range = {0x29, 0x2a};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_10_14(int view)
{
  const FamilyRange range = {0x10, 0x14};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetFirstObjectInFamilies_23_24(int view)
{
  const FamilyRange range = {0x23, 0x24};
  return GetFirstObjectInRangeForView(&range, view, 1);
}

GameObjNotifier* GetGameRootObject(int view)
{
  const ObjectKey key = ObjectKey::Make(0xf1, 0x4);
  return GetObjectForView(&key, view);
}

GameObjNotifier* GetObject_32000242(int view)
{
  const ObjectKey key = ObjectKey::Make(0x32, 0x242);
  return GetObjectForView(&key, view);
}

GameObjNotifier* GetObject_35000281(int view)
{
  const ObjectKey key = ObjectKey::Make(0x35, 0x281);
  return GetObjectForView(&key, view);
}

GameObjNotifier* GetObject_34000280(int view)
{
  const ObjectKey key = ObjectKey::Make(0x34, 0x280);
  return GetObjectForView(&key, view);
}

GameObjNotifier* GetSideObject(int view, int side)
{
  const ObjectKey key = ObjectKey::Make(0xf3, side != 1 ? 0x10 : 0xf);
  return GetObjectForView(&key, view);
}

GameObjNotifier* GetFirstObjectInRangeForSide(int view, const FamilyRange* range, int resolve)
{
  return GetFirstObjectInRangeForView(range, view, resolve);
}
