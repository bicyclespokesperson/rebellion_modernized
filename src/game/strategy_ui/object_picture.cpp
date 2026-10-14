#include <cstdint>
#include <new>

#include "ai/ai_common.h"
#include "character/character.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "ui/cool_app.h"

namespace
{

constexpr uint32_t kSpriteSlot = 7;
constexpr uint32_t kPictureBitmapKind = 10;
constexpr uint32_t kRecordIdMask = 0xfff;
constexpr uint32_t kAlliance = 1;
constexpr uint32_t kEmpire = 2;

constexpr uint32_t kStateUsable = 0x1;
constexpr uint32_t kStateDestroyed = 0x8;
constexpr uint32_t kStateEnroute = 0x10;
constexpr uint32_t kStateHasOrders = 0x200;
constexpr uint32_t kStateCompleted = 0x4;
constexpr uint32_t kCharacterCaptured = 0x1;

constexpr uint32_t kCharacterFrameBitmap = 0x2cec;

struct FamilyRange
{
  uint32_t m_first;
  uint32_t m_end;
  bool Contains(uint32_t family) const
  {
    return family >= m_first && family < m_end;
  }
};

constexpr FamilyRange kPlanetaryFamilies = {8, 0x10};
constexpr FamilyRange kUnitFamilies = {0x10, 0x14};
constexpr FamilyRange kDefenseFamilies = {0x20, 0x30};
constexpr FamilyRange kShieldFamilies = {0x22, 0x28};
constexpr FamilyRange kCharacterFamilies = {0x30, 0x40};
constexpr FamilyRange kMajorCharacterFamilies = {0x3c, 0x40};
constexpr FamilyRange kMinorCharacterFamilies = {0x30, 0x3c};
constexpr FamilyRange kMissionFamilies = {0x40, 0x80};

constexpr uint32_t kTypeKeyCharacterA = 0x3c000003;
constexpr uint32_t kTypeKeyCharacterB = 0x3c000005;
constexpr uint32_t kTypeKeyUnitA = 0x10000002;
constexpr uint32_t kTypeKeyUnitB = 0x10000008;

DibBitmap* LoadPicture(HMODULE module, uint32_t id)
{
  void* memory = ::operator new(sizeof(DibBitmap));
  return memory ? new (memory) DibBitmap(module, id, kPictureBitmapKind) : nullptr;
}

void DeletePicture(DibBitmap* picture)
{
  if (!picture)
    return;
  picture->~DibBitmap();
  ::operator delete(picture);
}

void DrawOverlay(HMODULE module, uint32_t id, DibBitmap* target)
{
  DibBitmap* overlay = LoadPicture(module, id);
  if (overlay->IsLoaded())
    overlay->DrawKeyed(target, 0, 0);
  DeletePicture(overlay);
}

uint32_t ControllingSide(GameObjNotifier* object)
{
  return (static_cast<GameObjRoot*>(object)->m_flags >> 6) & 3;
}

uint32_t IdForSide(uint32_t side, bool alternate, uint32_t allianceBase, uint32_t empireBase, uint32_t alternateStep)
{
  return (alternate ? alternateStep : 0) + (side == kAlliance ? allianceBase : empireBase);
}

} // namespace

DibBitmap*
CreateObjectPicture(HMODULE module, const ObjectKey* key, GameObjNotifier* object, int alternateArg, int decorateArg)
{
  const bool alternate = alternateArg != 0;
  const bool decorate = decorateArg != 0;
  HMODULE spriteModule = CoolAppGetResourceModule(kSpriteSlot);
  if (!object)
  {
    object = GetObjectInFamilies_01_ff(g_galaxyWindow->m_side, key);
  }
  else
  {
    ObjectKey objectKey;
    if (GetObjectKey(object, &objectKey)->m_value != key->m_value)
    {
      object = GetObjectInFamilies_01_ff(g_galaxyWindow->m_side, key);
    }
  }
  const uint32_t family = key->Family();
  if (!object)
    return nullptr;
  const uint32_t recordBits = static_cast<GameObj*>(object)->GetRecordTypeBits();
  ObjectKey typeKey;
  GetObjectRecordKey(object, &typeKey);
  const uint32_t objectSide = ControllingSide(object);
  const uint32_t stateFlags = static_cast<GameObj*>(object)->m_stateFlags;

  HMODULE pictureModule = module;
  uint32_t pictureId;
  if (kMissionFamilies.Contains(family))
  {
    pictureId = recordBits & kRecordIdMask;
    if (objectSide == kAlliance)
    {
      pictureId += alternate ? 0 : 0x4000;
    }
    else if (objectSide == kEmpire)
    {
      pictureId += alternate ? 0x1000 : 0x5000;
    }
  }
  else if (kPlanetaryFamilies.Contains(family))
  {
    pictureModule = spriteModule;
    pictureId = IdForSide(objectSide, alternate, 0x28a0, 0x28d2, 0x19);
  }
  else if (!alternate)
  {
    pictureId = (recordBits & kRecordIdMask) + 0x4000;
  }
  else
  {
    pictureId = recordBits & kRecordIdMask;
  }

  DibBitmap* picture = LoadPicture(pictureModule, pictureId);
  if (!picture->IsLoaded())
  {
    DeletePicture(picture);
    return nullptr;
  }
  DibBitmap* result = nullptr;
  {
    void* memory = ::operator new(sizeof(DibBitmap));
    const int height = picture->Height();
    const int width = picture->Width();
    result = memory ? new (memory) DibBitmap(width, height, picture) : nullptr;
    const uint8_t colorKey = picture->m_colorKey;
    const int resultHeight = result->Height();
    const int resultWidth = result->Width();
    result->FillRect(0, 0, resultWidth, resultHeight, colorKey);

    if (kCharacterFamilies.Contains(family) && !alternate)
    {
      DibBitmap* frame = LoadPicture(spriteModule, kCharacterFrameBitmap);
      frame->Blit(result, 0, 0, 0, 0, 0, 0);
      DeletePicture(frame);
    }
    else if (kUnitFamilies.Contains(family))
    {
      DibBitmap* frame = LoadPicture(spriteModule, (alternate ? 1 : 0) + 0x2cf9);
      frame->Blit(result, 0, 0, 0, 0, 0, 0);
      DeletePicture(frame);
    }

    if ((stateFlags & kStateEnroute) && !(stateFlags & kStateDestroyed) && decorate)
    {
      HMODULE overlayModule = module;
      int overlayId = (recordBits & kRecordIdMask) + (alternate ? 0x1000 : 0x5000);
      if (kMajorCharacterFamilies.Contains(family))
      {
        if (typeKey.m_value != kTypeKeyCharacterA && typeKey.m_value != kTypeKeyCharacterB)
        {
          overlayId = (alternate ? 0x13 : 0) + 0x2ced;
          overlayModule = spriteModule;
        }
      }
      if (kMinorCharacterFamilies.Contains(family))
      {
        overlayId = alternate ? 0 : 0x2ced;
        overlayModule = spriteModule;
      }
      else if (kShieldFamilies.Contains(family))
      {
        overlayId = (alternate ? 1 : 0) + 0x2cf5;
        overlayModule = spriteModule;
      }
      else if (kDefenseFamilies.Contains(family))
      {
        overlayId = (alternate ? 1 : 0) + 0x2cf1;
        overlayModule = spriteModule;
      }
      else if (kUnitFamilies.Contains(family))
      {
        if (typeKey.m_value != kTypeKeyUnitA && typeKey.m_value != kTypeKeyUnitB)
        {
          overlayId = (alternate ? 1 : 0) + 0x2cfb;
          overlayModule = spriteModule;
        }
      }
      else if (kPlanetaryFamilies.Contains(family))
      {
        overlayId = (alternate ? 3 : 0) + (objectSide == kAlliance ? 0x28b7 : 0x28e9);
        overlayModule = spriteModule;
      }
      if (static_cast<int16_t>(overlayId) != 0)
        DrawOverlay(overlayModule, overlayId, result);
    }

    if ((stateFlags & kStateHasOrders) && decorate)
    {
      HMODULE overlayModule = module;
      int overlayId = (recordBits & kRecordIdMask) + (alternate ? 0x2000 : 0x6000);
      if (kCharacterFamilies.Contains(family))
      {
        overlayId = alternate ? 0 : 0x2cee;
        overlayModule = spriteModule;
      }
      else if (kPlanetaryFamilies.Contains(family))
      {
        overlayId = (alternate ? 3 : 0) + (objectSide == kAlliance ? 0x28b8 : 0x28ea);
        overlayModule = spriteModule;
      }
      if (static_cast<int16_t>(overlayId) != 0)
        DrawOverlay(overlayModule, overlayId, result);
    }

    if (!(stateFlags & kStateCompleted) && decorate)
    {
      const auto overlayId =
        static_cast<int16_t>((alternate ? 1 : 0) + ((objectSide & 3) == kAlliance ? 0x2d32 : 0x2d34));
      if (overlayId != 0)
        DrawOverlay(spriteModule, overlayId, result);
    }
  }

  picture->DrawKeyed(result, 0, 0);
  DeletePicture(picture);
  if (kMinorCharacterFamilies.Contains(family) && !alternate &&
      (static_cast<Character*>(object)->m_flagsAc & kCharacterCaptured) && decorate)
  {
    DrawOverlay(module, (recordBits & kRecordIdMask) + 0x7000, result);
  }
  return result;
}
