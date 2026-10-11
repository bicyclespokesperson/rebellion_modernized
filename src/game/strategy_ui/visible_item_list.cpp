#include "strategy_ui/visible_item_list.h"

#include "ui/object_list_item.h"

VisibleItemList::~VisibleItemList() = default;

CoolListItem* VisibleItemList::First()
{
  for (const auto& ownedItem : Entries())
  {
    auto* item = static_cast<ObjectListItem*>(ownedItem.get());
    if (item->m_visible)
      return item;
  }
  return nullptr;
}

void ClearAllRows(CoolListItemList* list)
{
  for (const auto& ownedRow : list->Entries())
    static_cast<ObjectListItem*>(ownedRow.get())->m_visible = 1;
  list->Clear();
}
