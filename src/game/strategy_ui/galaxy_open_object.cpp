#include <cstdint>

#include "ai/ai_common.h"
#include "character/role.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/object_lookup.h"
#include "mission/mission.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/sector_window.h"
#include "ui/custom_dialog_box.h"

namespace
{

struct FamilyRange
{
  uint32_t m_first;
  uint32_t m_end;
};

constexpr FamilyRange kPlanetaryFamilies = {8, 0x10};
constexpr FamilyRange kUnitFamilies = {0x10, 0x14};
constexpr FamilyRange kFleetFamilies = {0x14, 0x1c};
constexpr FamilyRange kShipFamilies = {0x1c, 0x20};
constexpr FamilyRange kFacilityFamilies = {0x20, 0x30};
constexpr FamilyRange kDefenseFamilies = {0x22, 0x28};
constexpr FamilyRange kManufacturingFamilies = {0x28, 0x30};
constexpr FamilyRange kCharacterFamilies = {0x30, 0x40};
constexpr FamilyRange kSystemFamilies = {0x90, 0x98};
constexpr FamilyRange kDeepSpaceFamilies = {0x98, 0xa0};
constexpr FamilyRange kHiddenFleetFamilies = {0xf2, 0xf3};

constexpr int kSystemWindowOfCharacter = 10;
constexpr int kMissionWindowOfCharacter = 11;
constexpr int kFleetWindow = 4;
constexpr int kDefenseWindow = 10;
constexpr int kManufacturingWindow = 9;
constexpr int kSystemPageWindow = 10;

bool InRange(uint32_t family, const FamilyRange& range)
{
  return family >= range.m_first && family < range.m_end;
}

uint32_t FamilyOf(GameObjNotifier* object)
{
  ObjectKey key;
  return GetObjectKey(object, &key)->Family();
}

ObjectKey KeyOf(GameObjNotifier* object)
{
  ObjectKey key;
  GetObjectKey(object, &key);
  return key;
}

struct Destination
{
  ObjectKey m_sector;
  int m_windowType = 0;
  GameObjNotifier* m_shown = nullptr;
};

} // namespace

int GalaxyWindow::OpenWindowForObject(const ObjectKey* object)
{
  Destination destination;
  GameObjNotifier* target = GetObjectInFamilies_01_ff(m_side, object);
  if (!target)
    return 0;
  const uint32_t family = object->Family();
  GameObjNotifier* parent = target->m_parent;

  if (InRange(family, kFacilityFamilies))
  {
    destination.m_shown = parent;
    destination.m_sector = KeyOf(parent->m_parent);
    if (InRange(family, kDefenseFamilies))
    {
      destination.m_windowType = kDefenseWindow;
    }
    else if (InRange(family, kManufacturingFamilies))
    {
      destination.m_windowType = kManufacturingWindow;
    }
  }
  else if (InRange(family, kFleetFamilies))
  {
    if (InRange(FamilyOf(parent), kPlanetaryFamilies))
    {
      GameObjNotifier* shown = parent->m_parent;
      if (InRange(FamilyOf(shown), kHiddenFleetFamilies))
        return 0;
      destination.m_sector = KeyOf(shown->m_parent);
      destination.m_windowType = kFleetWindow;
      destination.m_shown = shown;
    }
  }
  else if (InRange(family, kShipFamilies) || InRange(family, kUnitFamilies))
  {
    if (InRange(FamilyOf(parent), kFleetFamilies))
    {
      GameObjNotifier* shown = parent->m_parent->m_parent;
      if (InRange(FamilyOf(shown), kHiddenFleetFamilies))
        return 0;
      destination.m_sector = KeyOf(shown->m_parent);
      destination.m_windowType = kFleetWindow;
      destination.m_shown = shown;
    }
    else if (InRange(FamilyOf(parent), kSystemFamilies))
    {
      destination.m_sector = KeyOf(parent->m_parent);
      destination.m_windowType = kSystemPageWindow;
      destination.m_shown = parent;
    }
  }
  else if (!InRange(family, kCharacterFamilies))
  {
    if (InRange(family, kSystemFamilies))
    {
      destination.m_sector = KeyOf(parent);
      destination.m_shown = target;
    }
    else if (InRange(family, kPlanetaryFamilies))
    {
      if (InRange(FamilyOf(parent), kHiddenFleetFamilies))
        return 0;
      destination.m_sector = KeyOf(parent->m_parent);
      destination.m_windowType = kFleetWindow;
      destination.m_shown = parent;
    }
  }
  else if (InRange(FamilyOf(parent), kFleetFamilies))
  {
    if (InRange(FamilyOf(parent->m_parent), kHiddenFleetFamilies))
      return 0;
    GameObjNotifier* shown = parent->m_parent->m_parent;
    destination.m_sector = KeyOf(shown->m_parent);
    destination.m_windowType = kFleetWindow;
    destination.m_shown = shown;
  }
  else if (InRange(FamilyOf(parent), kSystemFamilies))
  {
    destination.m_sector = KeyOf(parent->m_parent);
    destination.m_shown = parent;
    bool onVisibleMission = false;
    const ObjectKey missionKey = static_cast<Role*>(target)->m_missionKey;
    GameObjNotifier* mission = GetObjectInFamilies_40_80(m_side, &missionKey);
    if (mission && static_cast<Mission*>(mission)->GetRecordHidden() == 0)
      onVisibleMission = true;
    destination.m_windowType = onVisibleMission ? kMissionWindowOfCharacter : kSystemWindowOfCharacter;
  }
  else if (InRange(FamilyOf(parent), kDeepSpaceFamilies))
  {
    return 0;
  }

  if (destination.m_sector.IsSet())
  {
    SectorWindow* sectorWindow = static_cast<SectorWindow*>(OpenSectorWindow(&destination.m_sector));
    if (!sectorWindow)
      return 0;
    if (destination.m_windowType != 0)
    {
      const ObjectKey shownKey = KeyOf(destination.m_shown);
      CustomDialogBox* page =
        static_cast<CustomDialogBox*>(sectorWindow->OpenWindowOfType(&shownKey, destination.m_windowType));
      page->ShowObjectPage(object);
    }
  }
  return 1;
}
