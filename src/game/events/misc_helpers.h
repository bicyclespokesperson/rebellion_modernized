#pragma once
// Two small classes the linker placed between the mission code and the keyed events (0x5947a0-0x5949d0).
#include <cstdint>

#include "core/layout.h"
#include "gameobj/child_iterator.h"
#include "gameobj/object_key.h"

// Iterates the children of an object whose families lie in 0x30-0x38 (VT 0x66b9f4).
class CharacterRangeIterator : public ChildIterator {
public:
    CharacterRangeIterator(GameObjNotifier* parent, uint32_t stateFilter);  // 0x5947c0
    virtual ~CharacterRangeIterator();                                       // 0x594840
};

// An ObjectKeyNode with a second key (VT 0x66b9f8); its Load/Read/Save/Write/Checksum slots live at 0x58f0c0-0x5855d0.
class ObjectKeyPairNode : public ObjectKeyNode {
public:
    ObjectKeyPairNode();                                              // 0x594850
    ObjectKeyPairNode(const ObjectKey& first, const ObjectKey& second);  // 0x5948c0
    ObjectKeyPairNode(const ObjectKeyPairNode& other);                // 0x594920
    virtual ~ObjectKeyPairNode();                                     // 0x594980
    virtual ObjectKeyNode* Clone() override;                          // 1: 0x5949d0

    ObjectKey m_second;  // +0x1c
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(CharacterRangeIterator, 0x20);
REB_ASSERT_SIZE(ObjectKeyPairNode, 0x20);
REB_ASSERT_OFFSET(ObjectKeyPairNode, m_second, 0x1c);
#pragma clang diagnostic pop
