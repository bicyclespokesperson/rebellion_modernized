#pragma once
#include <cstdint>

#include "core/layout.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"
#include "ui/cool_list_item.h"

// Bits of FleetListItem::m_flags.
enum FleetListItemFlags : uint32_t {
    kFleetItemHidden = 0x01,    // a ship row below a collapsed fleet: Next/Previous skip it
    kFleetItemExpanded = 0x02,  // a fleet row whose ships are shown
    kFleetItemFleet = 0x04,     // a fleet row (otherwise a ship row)
    kFleetItemOwn = 0x08,       // the object is controlled by the side of the player
};

// A row of the fleet window's fleet list: the fleet, or a ship of the fleet shown below it. m_userData holds what the
// object carries (FleetContents).
class FleetListItem : public CoolListItem {
public:
    explicit FleetListItem(const ObjectKey* key);  // 0x4c7e10: id = the key's low 24 bits
    virtual ~FleetListItem();                      // 0x4c7eb0 (scalar deleting destructor 0x4c7e90)
    // The next/previous row, skipping hidden ones.
    virtual CoolListItem* Next() override;      // slot 3, 0x4c7f10
    virtual CoolListItem* Previous() override;  // slot 4, 0x4c7f30

    PersistentInt m_flags;  // +0x68 FleetListItemFlags
    ObjectKey m_key;        // +0x6c
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(FleetListItem, 0x70);
REB_ASSERT_OFFSET(FleetListItem, m_flags, 0x68);
REB_ASSERT_OFFSET(FleetListItem, m_key, 0x6c);
#pragma clang diagnostic pop
