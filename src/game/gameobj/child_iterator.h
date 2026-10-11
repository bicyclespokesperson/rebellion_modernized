#pragma once
#include <cstdint>

#include "gameobj/game_obj_root.h"
#include "gameobj/status_code.h"

// Walks the children of a game object in key order, keeping only those whose family is in a range, that pass a
// state filter (GameObj::MatchesStateFilter, 1-4) and, optionally, that are controlled by a given side. Every game
// class's per-family iterator is a subclass that fixes the range (and has its own vtable with just the destructor).
//
//   for (it.Begin(); it.Current(); it.Advance()) ...
class ChildIterator {
public:
    // Any side.
    ChildIterator(GameObjNotifier* parent, const FamilyRange* range, uint32_t stateFilter);
    // Only children controlled by `side`.
    ChildIterator(GameObjNotifier* parent, uint32_t side, const FamilyRange* range, uint32_t stateFilter);
    virtual ~ChildIterator();

    // Moves to the next accepted child (or to the end); returns *this.
    ChildIterator* Advance();
    // Moves to the first accepted child (or to the end); returns *this.
    ChildIterator* Begin();
    void Reset();
    // Number of accepted children.
    int Count();
    int HasCurrent();
    // True if the current child passes the range, state and side filters.
    int Accepts();
    GameObjNotifier* Current() const { return m_current; }

    GameObjNotifier* m_parent;  // +0x04
    FamilyRange m_range;        // +0x08
    uint32_t m_stateFilter;     // +0x10
    int m_hasSideFilter;        // +0x14
    uint32_t m_side;            // +0x18
    GameObjNotifier* m_current;  // +0x1c
};

// Iterator over the "unit" families 0x10-0x3f (troops, ships, squadrons, facilities, characters, special forces).
class UnitIterator : public ChildIterator {
public:
    UnitIterator(GameObjNotifier* parent, uint32_t stateFilter);
    virtual ~UnitIterator();
};
