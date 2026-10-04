#pragma once
#include "gameobj/child_iterator.h"

// Iterator over the missions of family 0x57 under a parent (state filter 1), with its own vtable holding just the destructor.

// Family 0x57 (MissionSubdue), VT 0x663e00.
class MissionSubdueIterator : public ChildIterator {
public:
    explicit MissionSubdueIterator(GameObjNotifier* parent);  // 0x569e10
    virtual ~MissionSubdueIterator();                         // 0x569e90 (deleting), 0x569eb0
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(MissionSubdueIterator, 0x20);
#pragma clang diagnostic pop
