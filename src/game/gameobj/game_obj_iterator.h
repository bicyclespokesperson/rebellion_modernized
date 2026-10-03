#pragma once
#include "core/layout.h"
#include "gameobj/child_iterator.h"

// Iterator over all children of any family (1 <= family < 0xff); the base of the iterators GameObj's own code uses
// (VT 0x65d41c, destructor only).
class GameObjIterator : public ChildIterator {
public:
    GameObjIterator(GameObjNotifier* parent, uint32_t stateFilter);
    virtual ~GameObjIterator();
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(GameObjIterator, 0x20);
#pragma clang diagnostic pop
