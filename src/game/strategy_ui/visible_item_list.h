#pragma once
#include "ui/cool_drag_list.h"

// The item list of the finder windows and the message index: a CoolListItemList of ObjectListItems whose First() skips
// the invisible first row (VT 0x65a120; the owner windows build it as CoolListItemList(2) and store the vtable).
class VisibleItemList : public CoolListItemList
{
  public:
  explicit VisibleItemList(int sortMode)
      : CoolListItemList(sortMode)
  {
  }
  ~VisibleItemList() override;    // 0x466320 (scalar deleting destructor 0x466300)
  CoolListItem* First() override; // slot 2, 0x466330
};

// Makes every row visible again (the rows' Next/Previous skip invisible ones) and removes them all.
void ClearAllRows(CoolListItemList* list);
