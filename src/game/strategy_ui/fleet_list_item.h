#pragma once
#include <cstdint>

#include "core/utility_classes.h"
#include "gameobj/object_key.h"
#include "ui/cool_list_item.h"

// Bits of FleetListItem::m_flags.
enum FleetListItemFlags : uint32_t
{
  kFleetItemHidden = 0x01, // a ship row below a collapsed fleet, excluded from visible list traversal
  kFleetItemExpanded = 0x02, // a fleet row whose ships are shown
  kFleetItemFleet = 0x04, // a fleet row (otherwise a ship row)
  kFleetItemOwn = 0x08, // the object is controlled by the side of the player
};

// A row of the fleet window's fleet list: the fleet, or a ship of the fleet shown below it. m_userData holds what the
// object carries (FleetContents).
class FleetListItem : public CoolListItem
{
public:
  explicit FleetListItem(const ObjectKey* key); // 0x4c7e10: id = the key's low 24 bits
  ~FleetListItem() override; // 0x4c7eb0 (scalar deleting destructor 0x4c7e90)
  bool IsVisible() const override
  {
    return (m_flags.m_value & kFleetItemHidden) == 0;
  }
  PersistentInt m_flags; // +0x68 FleetListItemFlags
  ObjectKey m_key; // +0x6c
};
