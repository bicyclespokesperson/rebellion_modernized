#include <cstdint>

#include "ai/ai_common.h"
#include "character/character.h"
#include "character/role.h"
#include "character/special_forces.h"
#include "core/checked_new.h"
#include "economy/defense_facility.h"
#include "economy/manu_mgr.h"
#include "economy/manufacturing_facility.h"
#include "economy/production_facility_types.h"
#include "fleet/capship.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/sector_window.h"
#include "ui/cool_app.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_list_item.h"
#include "ui/custom_dialog_box.h"

namespace
{

constexpr int kDisplayOff = 0x80;
constexpr uint32_t kSpriteSlot = 7;
constexpr uint32_t kMarkerHidden = 0x80;
constexpr uint32_t kMarkerHasFacilities = 0x8;
constexpr uint32_t kSideMarkerBits[4] = {0x80, 0x10, 0x20, 0x40};
constexpr int kMultiSideIcon = 0x27b0;
constexpr uint32_t kLoyaltyShown = 0x52;
constexpr uint32_t kStateUsable = 1;
constexpr uint32_t kRoleBusyBit = 7;

struct Thresholds
{
  int m_high;
  int m_midHigh;
  int m_midLow;
  int m_lowHigh;
  int m_lowLow;
};

constexpr Thresholds kLoyaltyThresholds = {0x51, 0x50, 0x3c, 0x3b, 0x32};
constexpr Thresholds kSingleThreshold = {1, 1, 1, 1, 1};
constexpr Thresholds kFleetThresholds = {3, 2, 2, 1, 1};
constexpr Thresholds kFacilityThresholds = {5, 4, 2, 1, 1};
constexpr Thresholds kManagerThresholds = {5, 4, 3, 1, 1};
constexpr Thresholds kDefaultThresholds = {6, 5, 3, 2, 1};

struct MarkerReading
{
  uint32_t m_value = 0;
  uint32_t m_kind = 0;
  int m_icon = 0;
  bool m_changed = false;
};

uint32_t SideCode(int side)
{
  return side == 1 ? 1 : side == 2 ? 2 : 0;
}
uint32_t OpponentCode(int side)
{
  return side == 1 ? 2 : side == 2 ? 1 : 0;
}
uint32_t OpposingViewSide(int side)
{
  return side == 3 ? 3 : 2 - (side != 1);
}

bool IsSystemFamily(const ObjectKey& key)
{
  return key.Family() >= 0x90 && key.Family() < 0x98;
}

uint32_t ControllingSide(GameObjNotifier* object)
{
  return (static_cast<GameObjRoot*>(object)->m_flags >> 6) & 3;
}

GameObj* AsGameObj(GameObjNotifier* object)
{
  return static_cast<GameObj*>(object);
}

bool IsFreeUnit(GameObjNotifier* unit, bool checkCharacterFlags)
{
  Role* role = static_cast<Role*>(unit);
  if (!(AsGameObj(unit)->m_stateFlags & kStateUsable))
    return false;
  if ((role->m_flags78 >> kRoleBusyBit) & 1)
    return false;
  if (!checkCharacterFlags)
    return true;
  Character* character = static_cast<Character*>(unit);
  return !(character->m_flagsAc & 1) && character->m_injury == 0 && character->m_commandKind == 0;
}

struct UnitTally
{
  int m_ownFree = 0;
  int m_ownBusy = 0;
  int m_otherFree = 0;
  int m_otherBusy = 0;
};

// Counts of the characters and special forces of one side in a system, split by availability: those at the system
// itself, and those aboard the system's fleets (own: of that side, other: of any other).
struct SystemUnitCounts
{
  int m_freeCharacters = 0;
  int m_busyCharacters = 0;
  int m_freeForces = 0;
  int m_busyForces = 0;
  int m_shipOwnBusyCharacters = 0;
  int m_shipOwnFreeCharacters = 0;
  int m_shipOtherBusyCharacters = 0;
  int m_shipOtherFreeCharacters = 0;
  int m_shipOwnBusyForces = 0;
  int m_shipOwnFreeForces = 0;
  int m_shipOtherBusyForces = 0;
  int m_shipOtherFreeForces = 0;
};

void CountUnits(GameObjNotifier* system, uint32_t side, SystemUnitCounts* counts)
{
  CharacterIterator characters(system, 3, side);
  for (characters.Begin(); characters.Current(); characters.Advance())
  {
    if (IsFreeUnit(characters.Current(), true))
    {
      ++counts->m_freeCharacters;
    }
    else
    {
      ++counts->m_busyCharacters;
    }
  }
  SpecialForceIterator forces(system, 3, side);
  for (forces.Begin(); forces.Current(); forces.Advance())
  {
    if (IsFreeUnit(forces.Current(), false))
    {
      ++counts->m_freeForces;
    }
    else
    {
      ++counts->m_busyForces;
    }
  }
  FleetIterator fleets(system, 3);
  for (fleets.Begin(); fleets.Current(); fleets.Advance())
  {
    CapShipIterator ships(fleets.Current(), 3);
    for (ships.Begin(); ships.Current(); ships.Advance())
    {
      CharacterIterator crew(ships.Current(), 3);
      for (crew.Begin(); crew.Current(); crew.Advance())
      {
        const bool busy = !IsFreeUnit(crew.Current(), true);
        const uint32_t crewSide = static_cast<Character*>(crew.Current())->GetRecordSide();
        if (side == crewSide)
        {
          if (busy)
          {
            ++counts->m_shipOwnBusyCharacters;
          }
          else
          {
            ++counts->m_shipOwnFreeCharacters;
          }
        }
        else if (busy)
        {
          ++counts->m_shipOtherBusyCharacters;
        }
        else
        {
          ++counts->m_shipOtherFreeCharacters;
        }
      }
      SpecialForceIterator crewForces(ships.Current(), 3, side);
      for (crewForces.Begin(); crewForces.Current(); crewForces.Advance())
      {
        const bool busy = !IsFreeUnit(crewForces.Current(), false);
        if (side == ControllingSide(crewForces.Current()))
        {
          if (busy)
          {
            ++counts->m_shipOwnBusyForces;
          }
          else
          {
            ++counts->m_shipOwnFreeForces;
          }
        }
        else if (busy)
        {
          ++counts->m_shipOtherBusyForces;
        }
        else
        {
          ++counts->m_shipOtherFreeForces;
        }
      }
    }
  }
}

} // namespace

// ---- Changes of a system ---------------------------------------------------------------------------------------

void GalaxyWindow::OnSystemChanged(const ObjectKey* system, int kind, int a, int b)
{
  GameObjNotifier* object = GetObjectInFamilies_90_98(m_side, system);
  ObjectKey sectorKey;
  GetObjectKey(object->m_parent, &sectorKey);
  const uint32_t sectorWindowId = (static_cast<uint16_t>(static_cast<uint16_t>(sectorKey.m_value) << 6)) | 1;
  SectorWindow* sectorWindow = static_cast<SectorWindow*>(m_children.FindById(sectorWindowId));
  UpdateSystemMarker(system, object, 1);
  if (!sectorWindow)
    return;
  if (kind == 1)
  {
    sectorWindow->UpdateEnergyBar(system, b, a);
  }
  else if (kind == 2)
  {
    sectorWindow->UpdateLoyaltyBar(system, b, a);
  }
}

// Brings the marker of a system in the Galactic Information Display up to date for the display mode that is on:
// the number (value) the mode shows for the system, the side (kind) whose colour it gets and the picture that
// follows from both through GidIconId.
void GalaxyWindow::UpdateSystemMarker(const ObjectKey* key, GameObjNotifier* object, int redraw)
{
  if (m_displayMode == kDisplayOff)
    return;
  if (!object)
  {
    object = GetObjectInFamilies_01_ff(m_side, key);
    if (!object)
      return;
  }
  GameObjNotifier* system = object;
  for (;;)
  {
    ObjectKey candidate;
    GetObjectKey(system, &candidate);
    if (IsSystemFamily(candidate))
      break;
    system = system->m_parent;
    if (!system)
      return;
  }
  ObjectKey systemKey;
  GetObjectKey(system, &systemKey);
  System* systemObject = static_cast<System*>(system);
  GameObj* systemGameObj = AsGameObj(system);
  const uint32_t systemSide = ControllingSide(system);
  CoolListItem* marker = m_systemItems.FindById(systemKey.Id());
  const int mode = m_displayMode;
  const bool revealsUnexplored = mode == 0x22 || mode == 0x44;
  const bool showsFleets =
    mode == 0x22 || mode == 0x20 || mode == 0x21 || mode == 0x43 || mode == 0x44 || mode == 0x92 || mode == 0x93;
  bool hidden;
  if (mode == 0x17)
  {
    hidden = (systemObject->m_flags & 2) != 0 || (!showsFleets && (systemGameObj->m_stateFlags & 8));
  }
  else
  {
    hidden = !(systemObject->m_flags & 2) || (!showsFleets && (systemGameObj->m_stateFlags & 8));
  }
  MarkerReading reading;
  reading.m_kind = systemSide;
  bool changed = false;
  if (hidden && !revealsUnexplored)
  {
    marker->m_state.m_value |= kMarkerHidden;
    changed = true;
  }
  else
  {
    const auto iconFor = [&](const Thresholds& t)
    {
      return GidIconId(reading.m_value, reading.m_kind, t.m_high, t.m_midHigh, t.m_midLow, t.m_lowHigh, t.m_lowLow);
    };
    const auto clearHiddenValueBit = [&]()
    {
      if (marker->m_state.m_value & kMarkerHidden)
        marker->m_userData &= ~kMarkerHidden;
    };
    const uint32_t ownCode = SideCode(m_side);
    const uint32_t opponentCode = OpponentCode(m_side);
    bool useDefaultThresholds = false;
    bool computeIcon = true;
    switch (mode)
    {
      case 0x10:
      case 0x11:
        reading.m_value = systemObject->GetLoyaltyFor(m_side);
        reading.m_icon = iconFor(kLoyaltyThresholds);
        clearHiddenValueBit();
        computeIcon = false;
        break;
      case 0x12:
        reading.m_value = (systemObject->m_flags >> 2) & 1;
        reading.m_icon = iconFor(kSingleThreshold);
        computeIcon = false;
        break;
      case 0x13:
      case 0x14:
      {
        const bool sameLoyalty = systemObject->m_controlData.Loyalty() == static_cast<uint32_t>(m_side);
        const bool match = systemSide == static_cast<uint32_t>(m_side) && (mode == 0x13 ? sameLoyalty : !sameLoyalty);
        reading.m_value = match ? kLoyaltyShown : 0;
        reading.m_icon = iconFor(kLoyaltyThresholds);
        clearHiddenValueBit();
        computeIcon = false;
        break;
      }
      case 0x15:
      case 0x16:
      {
        const uint32_t opposing = OpposingViewSide(m_side);
        const bool sameLoyalty = systemObject->m_controlData.Loyalty() == opposing;
        const bool match = systemSide == opposing && (mode == 0x15 ? sameLoyalty : !sameLoyalty);
        reading.m_value = match ? kLoyaltyShown : 0;
        reading.m_icon = iconFor(kLoyaltyThresholds);
        clearHiddenValueBit();
        computeIcon = false;
        break;
      }
      case 0x17:
        reading.m_value = (systemObject->m_flags & 2) ? 0 : kLoyaltyShown;
        reading.m_kind = 3;
        reading.m_icon = iconFor(kLoyaltyThresholds);
        clearHiddenValueBit();
        computeIcon = false;
        break;
      case 0x20:
      case 0x21:
      {
        const int filter = 1;
        reading.m_icon = 0;
        reading.m_kind = ownCode;
        const uint32_t own = CountFleets(systemGameObj, ownCode, filter);
        const uint32_t opposing = CountFleets(systemGameObj, opponentCode, filter);
        reading.m_value = own;
        if (opposing == 0)
        {
          if (own == 0)
          {
            reading.m_kind = systemSide;
            reading.m_value = CountFleets(systemGameObj, systemSide, filter);
          }
        }
        else if (own == 0)
        {
          reading.m_kind = opponentCode;
          reading.m_value = opposing;
        }
        else
        {
          reading.m_value = own + opposing;
          reading.m_icon = kMultiSideIcon;
        }
        if (static_cast<int16_t>(reading.m_icon) == 0)
          reading.m_icon = iconFor(kFleetThresholds);
        computeIcon = false;
        break;
      }
      case 0x22:
      {
        const int filter = 2;
        reading.m_icon = 0;
        reading.m_kind = ownCode;
        const uint32_t own = CountFleets(systemGameObj, ownCode, filter);
        const uint32_t opposing = CountFleets(systemGameObj, opponentCode, filter);
        reading.m_value = own;
        if (opposing != 0)
        {
          if (own != 0)
          {
            reading.m_icon = kMultiSideIcon;
            reading.m_value = own + opposing;
          }
          else
          {
            reading.m_kind = opponentCode;
            reading.m_value = opposing;
          }
        }
        else if (own == 0)
        {
          reading.m_kind = (systemObject->m_flags & 2) ? systemSide : 0;
        }
        if (static_cast<int16_t>(reading.m_icon) == 0)
          reading.m_icon = iconFor(kFleetThresholds);
        computeIcon = false;
        break;
      }
      case 0x43:
      case 0x44:
      {
        SystemUnitCounts counts;
        CountUnits(system, ownCode, &counts);
        int available;
        int unavailable;
        if (mode == 0x43)
        {
          available =
            counts.m_shipOwnFreeForces + counts.m_shipOwnFreeCharacters + counts.m_freeForces + counts.m_freeCharacters;
          unavailable = counts.m_shipOtherFreeForces + counts.m_shipOtherFreeCharacters;
        }
        else
        {
          available =
            counts.m_shipOwnBusyForces + counts.m_shipOwnBusyCharacters + counts.m_busyForces + counts.m_busyCharacters;
          unavailable = counts.m_shipOtherBusyForces + counts.m_shipOtherBusyCharacters;
        }
        if (available == 0)
        {
          if (unavailable == 0)
          {
            reading.m_kind = systemSide;
          }
          else
          {
            reading.m_kind = m_side == 1 ? 2 : (m_side == 2 ? 1 : 0);
          }
        }
        else
        {
          reading.m_kind = m_side == 1 ? 1 : (m_side == 2 ? 2 : 0);
        }
        reading.m_value = available + unavailable;
        reading.m_icon = iconFor(kSingleThreshold);
        computeIcon = false;
        break;
      }
      case 0x51:
        reading.m_value = systemObject->m_energy - systemObject->m_energyAllocated;
        useDefaultThresholds = true;
        break;
      case 0x52:
        reading.m_value = systemObject->m_rawMaterial - systemObject->m_rawMaterialAllocated;
        reading.m_icon = iconFor(kFleetThresholds);
        computeIcon = false;
        break;
      case 0x53:
        reading.m_value = CountProductionFacilityType2C(systemGameObj, reading.m_kind, 1);
        useDefaultThresholds = true;
        break;
      case 0x54:
        reading.m_value = CountProductionFacilityType2D(systemGameObj, reading.m_kind, 1);
        useDefaultThresholds = true;
        break;
      case 0x61:
        reading.m_value = CountManufacturingFacilityChildren(systemGameObj, reading.m_kind, 1) > 0;
        reading.m_icon = iconFor(kSingleThreshold);
        computeIcon = false;
        break;
      case 0x62:
        reading.m_value = CountManufacturingFacility28Children(systemGameObj, reading.m_kind, 1);
        reading.m_icon = iconFor(kFacilityThresholds);
        computeIcon = false;
        break;
      case 99:
        reading.m_value = CountManufacturingFacility29Children(systemGameObj, reading.m_kind, 1);
        reading.m_icon = iconFor(kFacilityThresholds);
        computeIcon = false;
        break;
      case 100:
        reading.m_value = CountManufacturingFacility2AChildren(systemGameObj, reading.m_kind, 1);
        reading.m_icon = iconFor(kFacilityThresholds);
        computeIcon = false;
        break;
      case 0x65:
      case 0x66:
      case 0x67:
      {
        const int slot = mode == 0x65 ? 0 : (mode == 0x66 ? 2 : 1);
        ManuMgr* manager = static_cast<ManuMgr*>(systemObject->FindManufacturingManagerBySlot123(slot));
        bool idle = false;
        if (manager && manager->CountHostFacilities(0) != 0 && manager->m_remainingGameObjCount == 0)
        {
          idle = reading.m_kind == ownCode;
        }
        reading.m_value = idle ? 0xf : 0;
        reading.m_icon = iconFor(mode == 0x67 ? kManagerThresholds : kFacilityThresholds);
        computeIcon = false;
        break;
      }
      case 0x71:
        reading.m_value = CountTroopRegs(systemGameObj, reading.m_kind, 1);
        useDefaultThresholds = true;
        break;
      case 0x72:
        reading.m_value = CountFightSquads(systemGameObj, reading.m_kind, 1);
        useDefaultThresholds = true;
        break;
      case 0x73:
        reading.m_value = CountDefenseFacilities25(systemGameObj, reading.m_kind, 1);
        reading.m_icon = iconFor(kSingleThreshold);
        computeIcon = false;
        break;
      case 0x74:
        reading.m_value = CountDefenseFacilities24(systemGameObj, reading.m_kind, 1);
        useDefaultThresholds = true;
        break;
      case 0x75:
        reading.m_value = CountDefenseFacilities22(systemGameObj, reading.m_kind, 1) +
                          CountDefenseFacilities23(systemGameObj, reading.m_kind, 1);
        useDefaultThresholds = true;
        break;
      case 0x91:
        reading.m_value = CountTroopRegs(systemGameObj, reading.m_kind, 1) +
                          CountDefenseFacilities(systemGameObj, reading.m_kind, 1) +
                          CountFightSquads(systemGameObj, reading.m_kind, 1);
        useDefaultThresholds = true;
        break;
      case 0x92:
      case 0x93:
      {
        ObjectKey selectedKey;
        GetObjectKey(system, &selectedKey);
        if (!m_selectedIds.FindById(selectedKey.Id()))
        {
          reading.m_value = 0;
          reading.m_kind = 0;
          changed = true;
          marker->m_state.m_value |= kMarkerHidden;
        }
        else
        {
          reading.m_value = 7;
          reading.m_kind = mode == 0x92 ? 2 : 1;
        }
        useDefaultThresholds = true;
        break;
      }
      default:
        computeIcon = false;
        break;
    }
    if (computeIcon && useDefaultThresholds)
      reading.m_icon = iconFor(kDefaultThresholds);

    if (marker->m_userData != reading.m_value && static_cast<int16_t>(reading.m_icon) != 0)
    {
      marker->m_userData = reading.m_value;
      changed = true;
    }
    PersistentInt flags(0);
    AllFacilityIterator facilities(system, 3);
    facilities.Begin();
    uint32_t markerBits = 0;
    if (facilities.Current())
      markerBits |= kMarkerHasFacilities;
    if (reading.m_kind < 4)
      markerBits |= kSideMarkerBits[reading.m_kind];
    flags.m_value = static_cast<int>(markerBits);
    if (flags.Get() != marker->m_state.Get())
    {
      changed = true;
      marker->m_state.m_value = flags.Get();
    }
  }

  GameObjNotifier* sector = system->m_parent;
  if (!changed || !marker || !sector)
    return;
  marker->SetImageIds(static_cast<uint16_t>(reading.m_icon), 0, CoolAppGetResourceModule(kSpriteSlot));
  if (redraw)
    RedrawSectorMarkers(sector);
  ObjectKey sectorKey;
  GetObjectKey(sector, &sectorKey);
  const uint32_t sectorWindowId = (static_cast<uint16_t>(static_cast<uint16_t>(sectorKey.m_value) << 6)) | 1;
  CustomDialogBox* sectorWindow = static_cast<CustomDialogBox*>(m_children.FindById(sectorWindowId));
  if (!sectorWindow)
    return;
  ObjectKey updatedKey;
  GetObjectKey(system, &updatedKey);
  sectorWindow->OnObjectUpdated(&updatedKey, system);
}
