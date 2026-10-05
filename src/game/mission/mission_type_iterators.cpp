#include "mission/mission_type_iterators.h"

#include "core/vptr_store.h"

namespace {
constexpr FamilyRange kFamilySubdue(0x57, 0x58);
constexpr FamilyRange kFamilyResearch(0x53, 0x54);
constexpr FamilyRange kFamilyIncite(0x56, 0x57);
constexpr uint32_t kStateFilter = 1;
}  // namespace

MissionSubdueIterator::MissionSubdueIterator(GameObjNotifier* parent) : ChildIterator(parent, &kFamilySubdue, kStateFilter) {}

MissionSubdueIterator::~MissionSubdueIterator() { REB_KEEP_VPTR_STORE(); }

MissionResearchIterator::MissionResearchIterator(GameObjNotifier* parent) : ChildIterator(parent, &kFamilyResearch, kStateFilter) {}

MissionResearchIterator::~MissionResearchIterator() { REB_KEEP_VPTR_STORE(); }

MissionInciteIterator::MissionInciteIterator(GameObjNotifier* parent) : ChildIterator(parent, &kFamilyIncite, kStateFilter) {}

MissionInciteIterator::~MissionInciteIterator() { REB_KEEP_VPTR_STORE(); }
