#include "gameobj/game_obj_iterator.h"


namespace {
constexpr FamilyRange kAllFamilies(1, 0xff);
}

GameObjIterator::GameObjIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kAllFamilies, stateFilter) {}

GameObjIterator::~GameObjIterator() = default;
