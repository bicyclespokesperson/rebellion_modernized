#include "strategy_ui/fleet_list_item.h"

FleetListItem::FleetListItem(const ObjectKey* key)
    : CoolListItem(key->m_value & 0xffffff)
    , m_flags(0)
{
  m_key = *key;
}

FleetListItem::~FleetListItem() = default;
