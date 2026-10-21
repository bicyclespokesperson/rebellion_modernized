#pragma once
#include <cstdint>

#include "ui/cool_drag_list.h"
#include "ui/cool_list_item.h"

#include "ui/message_record_decl.h"

// A row of the message index (0x468ab0): shows a message record's text. Rows whose m_visible is 0 are excluded from
// visible list traversal.
class MessageListItem : public CoolListItem
{
  public:
  explicit MessageListItem(MessageRecord* record); // 0x4ad6b0
  ~MessageListItem() override;                     // 0x4ad720 (scalar deleting destructor 0x4ad690)
  bool IsVisible() const override
  {
    return m_visible != 0;
  }
  void Read(istream* in) override;   // slot 1, 0x5ae1e0: does nothing
  void Write(ostream* out) override; // slot 2, 0x5ae1e0: does nothing

  MessageRecord* m_record; // +0x68 not owned
  int m_visible;           // +0x6c 1
};

// The message index's item list: First skips invisible rows (0x4665f0).
class MessageItemList : public CoolListItemList
{
  public:
  explicit MessageItemList(int sortMode); // 0x4ad620
  ~MessageItemList() override;            // 0x4ad660 (scalar deleting destructor 0x4ad640)
  CoolListItem* First() override;         // slot 2, 0x4ad670
};
