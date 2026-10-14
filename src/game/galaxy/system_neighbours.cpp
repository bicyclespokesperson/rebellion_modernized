#include "galaxy/system_neighbours.h"

#include <cstdlib>
#include <cstring>

#include "galaxy/game_setup_seed.h"
#include "gameobj/game_obj_root.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"
#include "rules/loyalty_params.h"

namespace
{

bool IsSystemFamily(uint32_t family)
{
  return family >= family::kSystems.m_first && family < family::kSystems.m_second;
}

int DistanceBetween(GameObjNotifier* from, GameObjNotifier* to)
{
  MapPosition toPosition;
  MapPosition fromPosition;
  auto* target = reinterpret_cast<MapPosition*>(
    static_cast<System*>(to)->System::GetRecordField4c(reinterpret_cast<uint32_t*>(&toPosition)));
  return MapDistance(reinterpret_cast<MapPosition*>(static_cast<System*>(from)->System::GetRecordField4c(
                       reinterpret_cast<uint32_t*>(&fromPosition))),
                     target);
}

int SystemIndex(const GameObjNotifier* system)
{
  return static_cast<int>(system->m_id) - SystemNeighbourTable::kFirstSystemId;
}

} // namespace

SystemNeighbourCursor::SystemNeighbourCursor() : m_begin(nullptr), m_current(nullptr)
{
}

SystemNeighbourCursor::~SystemNeighbourCursor() = default;

SystemNeighbourTable::SystemNeighbourTable()
{
  std::memset(m_entries, 0, sizeof(m_entries));
}

SystemNeighbourTable::~SystemNeighbourTable() = default;

int SystemNeighbourTable::GetNeighbours(const ObjectKey* key, SystemNeighbourCursor* out)
{
  out->m_begin = nullptr;
  out->m_current = nullptr;
  const int valid = IsSystemFamily(key->Family());
  if (valid)
  {
    const int index = static_cast<int>(key->Id()) - kFirstSystemId;
    if (index < 0 || index >= kSystemCount)
      return 0;
    SystemNeighbours* entry = m_entries + index;
    out->m_begin = entry->m_systems;
    if (entry)
      out->m_current = entry->m_systems;
  }
  return valid;
}

int SystemNeighbourTable::Build()
{
  for (GameObjNotifier* from = GetFirstMasterInFamilies_90_98(); from; from = NextSystemMaster(from))
  {
    SystemDistance distances[kSystemCount];
    std::memset(distances, 0, sizeof(distances));
    for (GameObjNotifier* to = GetFirstMasterInFamilies_90_98(); to; to = NextSystemMaster(to))
    {
      SystemDistance& slot = distances[SystemIndex(to)];
      slot.m_system = to;
      slot.m_distance = DistanceBetween(from, to);
    }
    std::qsort(distances, kSystemCount, sizeof(SystemDistance), CompareSystemDistance);
    SystemNeighbours& list = m_entries[SystemIndex(from)];
    for (int i = 0; i < kSystemCount; ++i)
      list.m_systems[i] = distances[i].m_system;
  }
  return 1;
}

int CompareSystemDistance(const void* a, const void* b)
{
  const auto* first = static_cast<const SystemDistance*>(a);
  const auto* second = static_cast<const SystemDistance*>(b);
  const bool firstListed = first->m_system != nullptr;
  const bool secondListed = second->m_system != nullptr;
  if (!firstListed)
    return secondListed ? 1 : 0;
  if (!secondListed)
    return -1;
  if (first->m_distance > second->m_distance)
    return 1;
  return first->m_distance >= second->m_distance ? 0 : -1;
}
