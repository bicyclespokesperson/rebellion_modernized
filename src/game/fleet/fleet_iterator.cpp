#include "fleet/fleet_iterator.h"

#include "gameobj/object_family.h"

namespace {
constexpr uint32_t kUsableFilter = 1;
}

FleetIterator::FleetIterator(GameObjNotifier* parent) : ChildIterator(parent, &family::kFleets, kUsableFilter) {}

FleetIterator::FleetIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &family::kFleets, stateFilter) {}

FleetIterator::FleetIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &family::kFleets, stateFilter) {}

FleetIterator::~FleetIterator() {  }
