#include "ui/message_list_item.h"

#include "ui/message_record_decl.h"

MessageListItem::MessageListItem(MessageRecord* record) : CoolListItem(record->m_id)
{
  m_record = record;
  m_text = record->m_text0.c_str();
  m_visible = 1;
}

MessageListItem::~MessageListItem() = default;

void MessageListItem::Read(istream*)
{
}

void MessageListItem::Write(ostream*)
{
}

MessageItemList::MessageItemList(int sortMode) : CoolListItemList(sortMode)
{
}

MessageItemList::~MessageItemList() = default;

CoolListItem* MessageItemList::First()
{
  for (const auto& entry : Entries())
  {
    auto* item = static_cast<MessageListItem*>(entry.get());
    if (item->m_visible)
      return item;
  }
  return nullptr;
}
