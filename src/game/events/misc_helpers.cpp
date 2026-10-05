#include "events/misc_helpers.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"

namespace {
constexpr FamilyRange kCharacterRange(0x30, 0x38);
}

CharacterRangeIterator::CharacterRangeIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kCharacterRange, stateFilter) {}

CharacterRangeIterator::~CharacterRangeIterator() { REB_KEEP_VPTR_STORE(); }

ObjectKeyPairNode::ObjectKeyPairNode() : ObjectKeyNode(), m_second() {}

ObjectKeyPairNode::ObjectKeyPairNode(const ObjectKey& first, const ObjectKey& second)
    : ObjectKeyNode(first), m_second(second) {}

ObjectKeyPairNode::ObjectKeyPairNode(const ObjectKeyPairNode& other) : ObjectKeyNode(other), m_second(other.m_second) {}

ObjectKeyPairNode::~ObjectKeyPairNode() { REB_KEEP_VPTR_STORE(); }

ObjectKeyNode* ObjectKeyPairNode::Clone() { return NewOrNull<ObjectKeyPairNode>(*this); }
