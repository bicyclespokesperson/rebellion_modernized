#include "mission/mission_iterators.h"

#include "core/vptr_store.h"

namespace {
constexpr FamilyRange kFamilySubdue(0x57, 0x58);
constexpr FamilyRange kFamily58(0x58, 0x59);
constexpr uint32_t kStateFilter = 1;
}  // namespace

MissionSubdueIterator::MissionSubdueIterator(GameObjNotifier* parent) : ChildIterator(parent, &kFamilySubdue, kStateFilter) {}

MissionSubdueIterator::~MissionSubdueIterator() { REB_KEEP_VPTR_STORE(); }

MissionFamily58Iterator::MissionFamily58Iterator(GameObjNotifier* parent) : ChildIterator(parent, &kFamily58, kStateFilter) {}

MissionFamily58Iterator::~MissionFamily58Iterator() { REB_KEEP_VPTR_STORE(); }
