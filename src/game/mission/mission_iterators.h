#pragma once
#include "gameobj/child_iterator.h"

// Iterators over the missions of one family under a parent (state filter 1). Each has its own vtable holding just the
// destructor.

// Family 0x57 (MissionSubdue), VT 0x663e00.
class MissionSubdueIterator : public ChildIterator {
public:
    explicit MissionSubdueIterator(GameObjNotifier* parent);  // 0x569e10
    virtual ~MissionSubdueIterator();                         // 0x569e90 (deleting), 0x569eb0
};

// Family 0x58 (the mission class whose GetFamily is 0x5705a0), VT 0x666a68.
class MissionFamily58Iterator : public ChildIterator {
public:
    explicit MissionFamily58Iterator(GameObjNotifier* parent);  // 0x571650
    virtual ~MissionFamily58Iterator();                         // 0x5716d0 (deleting), 0x5716f0
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(MissionSubdueIterator, 0x20);
REB_ASSERT_SIZE(MissionFamily58Iterator, 0x20);
#pragma clang diagnostic pop
