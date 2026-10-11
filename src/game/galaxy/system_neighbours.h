#pragma once
#include <cstdint>

#include "gameobj/object_key.h"

class GameObjNotifier;

// A position on the galaxy map.
struct MapPosition {
    int16_t m_x;
    int16_t m_y;
};

// For every star system, all systems ordered by distance from it (nearest first). Built once by the galaxy setup
// (0x5562e0) and consulted by the AI and the map UI.

// A position within a pointer array of systems: `begin` is the start of a system's neighbour list, `current` the
// element being looked at. Both stay null for an invalid key (VT 0x66a890, only the destructor is virtual).
class SystemNeighbourCursor {
public:
    SystemNeighbourCursor();           // 0x58af30
    virtual ~SystemNeighbourCursor();  // 0x58af70 (scalar deleting 0x58af50)

    GameObjNotifier** m_begin;    // +0x04
    GameObjNotifier** m_current;  // +0x08
};

// One system's neighbours: the 200 systems by ascending distance (unused ids last), then a spare null.
struct SystemNeighbours {
    GameObjNotifier* m_systems[201];
};

// Ids of systems start at 100 (family 0x90): entry `id - 100` belongs to the system with that id.
class SystemNeighbourTable {
public:
    static constexpr int kSystemCount = 200;
    static constexpr int kFirstSystemId = 100;

    SystemNeighbourTable();           // 0x58af80, all entries null
    virtual ~SystemNeighbourTable();  // 0x58afc0 (scalar deleting 0x58afa0)

    // Points `out` at the neighbour list of the system `key` names. Returns 1 if the key is a system's, else 0 with
    // `out` null. BUG(original): the id is not checked against the table size.
    int GetNeighbours(const ObjectKey* key, SystemNeighbourCursor* out);  // 0x58afd0
    // Fills every system's list from the current master systems. Always returns 1.
    int Build();  // 0x58b060

    SystemNeighbours m_entries[kSystemCount];  // +0x04
};

// A system and its distance from the system whose list is being built.
struct SystemDistance {
    GameObjNotifier* m_system;
    int m_distance;
};

// qsort comparator (0x58b140): listed systems first, by ascending distance; unlisted (null) slots last.
int CompareSystemDistance(const void* a, const void* b);
