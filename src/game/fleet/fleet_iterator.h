#pragma once
#include "gameobj/child_iterator.h"

// Iterator over the fleets (families 0x08-0x0f) below an object, VT 0x65d648 (destructor only). The state filter is
// 1 (usable) unless given.
class FleetIterator : public ChildIterator
{
  public:
  FleetIterator(GameObjNotifier* parent);                                      // 0x4ffdd0
  FleetIterator(GameObjNotifier* parent, uint32_t stateFilter);                // 0x4ffe70
  FleetIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side); // 0x4ffef0
  ~FleetIterator() override;                                                   // 0x4ffe50 (deleting), 0x4fff70
};
