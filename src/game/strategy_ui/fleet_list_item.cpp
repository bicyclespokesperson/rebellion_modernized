#include "strategy_ui/fleet_list_item.h"


FleetListItem::FleetListItem(const ObjectKey* key) : CoolListItem(key->m_value & 0xffffff), m_flags(0) { m_key = *key; }

FleetListItem::~FleetListItem() {  }

CoolListItem* FleetListItem::Next() {
    FleetListItem* next = static_cast<FleetListItem*>(m_next);
    if (next && (next->m_flags.m_value & kFleetItemHidden) != 0) return next->Next();
    return next;
}

CoolListItem* FleetListItem::Previous() {
    FleetListItem* previous = static_cast<FleetListItem*>(m_prev);
    if (previous && (previous->m_flags.m_value & kFleetItemHidden) != 0) return previous->Previous();
    return previous;
}
