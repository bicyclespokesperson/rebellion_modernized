#include "ui/message_list_item.h"

#include "ui/message_record_decl.h"

MessageListItem::MessageListItem(MessageRecord* record) : CoolListItem(record->m_id) {
    m_record = record;
    m_text = record->m_text0.m_text;
    m_visible = 1;
}

MessageListItem::~MessageListItem() = default;

void MessageListItem::Read(istream*) {}

void MessageListItem::Write(ostream*) {}

CoolListItem* MessageListItem::Next() {
    MessageListItem* next = static_cast<MessageListItem*>(m_next);
    if (next && !next->m_visible) return next->Next();
    return next;
}

CoolListItem* MessageListItem::Previous() {
    MessageListItem* previous = static_cast<MessageListItem*>(m_prev);
    if (previous && !previous->m_visible) return previous->Previous();
    return previous;
}

MessageItemList::MessageItemList(int sortMode) : CoolListItemList(sortMode) {}

MessageItemList::~MessageItemList() = default;

CoolListItem* MessageItemList::First() {
    MessageListItem* first = static_cast<MessageListItem*>(m_head);
    if (first && !first->m_visible) return first->Next();
    return first;
}
