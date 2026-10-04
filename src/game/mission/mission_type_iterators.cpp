#include "mission/mission_type_iterators.h"

#include "core/vptr_store.h"

namespace {
constexpr FamilyRange kFamilySubdue(0x57, 0x58);
constexpr uint32_t kStateFilter = 1;
}  // namespace

MissionSubdueIterator::MissionSubdueIterator(GameObjNotifier* parent) : ChildIterator(parent, &kFamilySubdue, kStateFilter) {}

MissionSubdueIterator::~MissionSubdueIterator() { REB_KEEP_VPTR_STORE(); }
