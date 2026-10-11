#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/stream_io.h"
#include "strategy_ui/advisor_notices.h"

// The tree of saved UI notices used by the human player's order tree.
class UiRecordTree : public IdAvlTree
{
public:
  UiRecordTree(); // 0x436a40
  ~UiRecordTree() override; // 0x437690 (scalar deleting destructor 0x437670)
  virtual void Read(istream* in); // slot 3, 0x4376a0: 16-bit count, then kind and body of every record
  virtual void Write(ostream* out); // slot 4, 0x437720
};

// Creates an empty notice of the requested kind through its concrete notice prototype.
UiNotice* CreateUiRecord(int kind);

constexpr int kUiHandlerCount = 0x2e;
extern UiNotice* g_uiHandlers[kUiHandlerCount];
