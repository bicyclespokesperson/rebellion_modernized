#pragma once
#include <cstdint>

#include "core/id_registry.h"
#include "core/stream_io.h"
#include "strategy_ui/advisor_notices.h"

// The tree of saved UI notices used by the human player's order tree.
class UiRecordTree
{
  public:
  void Clear()
  {
    m_notices.Clear();
  }
  size_t Count() const
  {
    return m_notices.Count();
  }
  UiNotice* First() const
  {
    return m_notices.First();
  }
  UiNotice* FindById(uint32_t id) const
  {
    return m_notices.FindById(id);
  }
  UiNotice* DetachById(uint32_t id)
  {
    return m_notices.DetachById(id);
  }
  uint32_t AddWithId(UiNotice* notice);
  auto Entries() const
  {
    return m_notices.Entries();
  }

  UiRecordTree();           // 0x436a40
  ~UiRecordTree();          // 0x437690 (scalar deleting destructor 0x437670)
  void Read(istream* in);   // 0x4376a0: 16-bit count, then kind and body of every record
  void Write(ostream* out); // 0x437720

  private:
  IdRegistry<UiNotice> m_notices;
};

// Creates an empty notice of the requested kind through its concrete notice prototype.
UiNotice* CreateUiRecord(int kind);

constexpr int kUiHandlerCount = 0x2e;
extern UiNotice* g_uiHandlers[kUiHandlerCount];
