#include "strategy_ui/visible_item_list.h"

#include "ui/object_list_item.h"

VisibleItemList::~VisibleItemList() = default;

CoolListItem* VisibleItemList::First() {
    ObjectListItem* head = static_cast<ObjectListItem*>(m_head);
    if (head && !head->m_visible) return head->Next();
    return head;
}

void ClearAllRows(CoolListItemList* list) {
    for (LinkNode* row = list->m_head; row; row = row->m_next) static_cast<ObjectListItem*>(row)->m_visible = 1;
    list->Clear();
}
