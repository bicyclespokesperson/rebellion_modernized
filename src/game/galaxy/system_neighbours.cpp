#include "galaxy/system_neighbours.h"

#include <cstdlib>
#include <cstring>

#include "core/vptr_store.h"
#include "gameobj/game_obj_root.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"

// The system's map position (its data record's +0x4c), 0,0 without a record; returns `out`.
MapPosition* __thiscall FUN_00509620(GameObjNotifier* system, MapPosition* out);
// Straight-line distance of two map positions.
int __cdecl FUN_0055d860(const MapPosition* a, const MapPosition* b);
// The next master system after `system`.
GameObjNotifier* __thiscall FUN_0051a5d0(GameObjNotifier* system);

namespace {

bool IsSystemFamily(uint32_t family) { return family >= family::kSystems.m_first && family < family::kSystems.m_second; }

int DistanceBetween(GameObjNotifier* from, GameObjNotifier* to) {
    MapPosition toPosition;
    MapPosition fromPosition;
    MapPosition* target = FUN_00509620(to, &toPosition);
    return FUN_0055d860(FUN_00509620(from, &fromPosition), target);
}

int SystemIndex(const GameObjNotifier* system) { return static_cast<int>(system->m_id) - SystemNeighbourTable::kFirstSystemId; }

}  // namespace

SystemNeighbourCursor::SystemNeighbourCursor() : m_begin(nullptr), m_current(nullptr) {}

SystemNeighbourCursor::~SystemNeighbourCursor() { REB_KEEP_VPTR_STORE(); }

SystemNeighbourTable::SystemNeighbourTable() { std::memset(m_entries, 0, sizeof(m_entries)); }

SystemNeighbourTable::~SystemNeighbourTable() { REB_KEEP_VPTR_STORE(); }

int SystemNeighbourTable::GetNeighbours(const ObjectKey* key, SystemNeighbourCursor* out) {
    out->m_begin = nullptr;
    out->m_current = nullptr;
    const int valid = IsSystemFamily(key->Family());
    if (valid) {
        SystemNeighbours* entry = m_entries + (static_cast<int>(key->Id()) - kFirstSystemId);
        out->m_begin = entry->m_systems;
        if (entry) out->m_current = entry->m_systems;
    }
    return valid;
}

int SystemNeighbourTable::Build() {
    for (GameObjNotifier* from = GetFirstMasterInFamilies_90_98(); from; from = FUN_0051a5d0(from)) {
        SystemDistance distances[kSystemCount];
        std::memset(distances, 0, sizeof(distances));
        for (GameObjNotifier* to = GetFirstMasterInFamilies_90_98(); to; to = FUN_0051a5d0(to)) {
            SystemDistance& slot = distances[SystemIndex(to)];
            slot.m_system = to;
            slot.m_distance = DistanceBetween(from, to);
        }
        std::qsort(distances, kSystemCount, sizeof(SystemDistance), CompareSystemDistance);
        SystemNeighbours& list = m_entries[SystemIndex(from)];
        for (int i = 0; i < kSystemCount; ++i) list.m_systems[i] = distances[i].m_system;
    }
    return 1;
}

int CompareSystemDistance(const void* a, const void* b) {
    const SystemDistance* first = static_cast<const SystemDistance*>(a);
    const SystemDistance* second = static_cast<const SystemDistance*>(b);
    const bool firstListed = first->m_system != nullptr;
    const bool secondListed = second->m_system != nullptr;
    if (!firstListed) return secondListed ? 1 : 0;
    if (!secondListed) return -1;
    if (first->m_distance > second->m_distance) return 1;
    return first->m_distance >= second->m_distance ? 0 : -1;
}
