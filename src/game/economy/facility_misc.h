#pragma once
#include "core/containers.h"
#include "core/layout.h"

// An IdAvlTree with an "ok" flag at +0x0c that its constructor sets (VT 0x65f270, 0x10 bytes). The object-order code
// (0x42a320, 0x46a9c0) builds one on the stack, has a game object fill it (0x4f5380) and walks it; nothing else is known.
class OkIdAvlTree : public IdAvlTree {
public:
    OkIdAvlTree();           // 0x526730
    virtual ~OkIdAvlTree();  // 0x526770 (scalar deleting destructor 0x526750)

    int m_ok;  // +0x0c
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(OkIdAvlTree, 0x10);
REB_ASSERT_OFFSET(OkIdAvlTree, m_ok, 0x0c);
#pragma clang diagnostic pop
