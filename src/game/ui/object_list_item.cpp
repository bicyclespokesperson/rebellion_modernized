#include "ui/object_list_item.h"


ObjectListItem::ObjectListItem(uint32_t id, const ObjectKey* key) : CoolListItem(id) {
    m_key = *key;
    m_visible = 1;
    m_markerImage = nullptr;
}

ObjectListItem::ObjectListItem(const ObjectKey* key) : CoolListItem(key->m_value & 0xffffff) {
    m_key = *key;
    m_markerImage = nullptr;
    m_visible = 1;
}

ObjectListItem::~ObjectListItem() = default;

void ObjectListItem::Read(istream*) {}

void ObjectListItem::Write(ostream*) {}

CoolListItem* ObjectListItem::Next() {
    ObjectListItem* next = static_cast<ObjectListItem*>(m_next);
    if (next && !next->m_visible) return next->Next();
    return next;
}

CoolListItem* ObjectListItem::Previous() {
    ObjectListItem* previous = static_cast<ObjectListItem*>(m_prev);
    if (previous && !previous->m_visible) return previous->Previous();
    return previous;
}
