#include "tactical/tactical_registry.h"

#include "tactical/tactical_externals.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_groups.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_ship.h"
#include "tactical/tactical_simulator.h"

namespace
{

constexpr int kFighterGroupKind = 3;
constexpr int kCapitalGroupKind = 4;
constexpr int kCapitalShipClass = 0;

int kindOf(TacticalUnit* unit)
{
  return TacticalUnitKind(unit);
}

IdNode* shipNode(TacticalShipBase* ship)
{
  return static_cast<TacticalUnit*>(ship);
}

TacticalHangar* hangarOfDeathStar(TacticalUnit* deathStar)
{
  return static_cast<TacticalDeathStar*>(deathStar);
}

} // namespace

int TacticalUnitKind(TacticalUnit* unit)
{
  return unit->GetUnitKind();
}

TacticalRegistry::TacticalRegistry()
{
  m_deathStar = nullptr;
  for (uint32_t base = 0; base < 2000000; base += 1000000)
  {
    m_handleCounter[base / 1000000] = base;
    m_groupCounter[base / 1000000] = base;
  }
}

TacticalRegistry::~TacticalRegistry()
{
  m_clearing = true;
  TacticalUnit* deathStar = m_deathStar;
  m_deathStar = nullptr;
  decltype(m_ships) ships;
  decltype(m_viewGroups) viewGroups;
  decltype(m_fighterGroups) fighterGroups;
  decltype(m_capitalGroups) capitalGroups;
  ships.swap(m_ships);
  viewGroups.swap(m_viewGroups);
  fighterGroups.swap(m_fighterGroups);
  capitalGroups.swap(m_capitalGroups);
  delete deathStar;
  ships.clear();
  viewGroups.clear();
  fighterGroups.clear();
  capitalGroups.clear();
}

uint32_t TacticalRegistry::NextUnitHandle(unsigned side)
{
  if (side < 2)
    return ++m_handleCounter[side];
  return 0;
}

uint32_t TacticalRegistry::NextGroupHandle(unsigned side)
{
  if (side < 2)
    return ++m_groupCounter[side];
  return 0;
}

TacticalShipBase* TacticalRegistry::FindShip(int handle)
{
  auto found = m_ships.find(static_cast<uint32_t>(handle));
  return found == m_ships.end() ? nullptr : found->second.get();
}

TacticalHangar* TacticalRegistry::FindLaunchInterface(int handle)
{
  TacticalShipBase* ship = FindShip(handle);
  if (!ship)
  {
#ifndef REB_NO_TACTICAL_3D
    if (GetTacticalGame()->m_abstract == 0)
    {
      TacticalUnit* world = FUN_005a9330(GetObjectManager());
      return world ? hangarOfDeathStar(world) : nullptr;
    }
#endif
    GetHeadlessSimulator();
    TacticalUnit* deathStar = GetTacticalRegistry()->GetDeathStar();
    return deathStar ? hangarOfDeathStar(deathStar) : nullptr;
  }
  if (ship->GetUnitClass() == kCapitalShipClass)
    return static_cast<TacticalCapitalShip*>(ship);
  return nullptr;
}

IdNode* TacticalRegistry::FindShipOrViewGroup(int handle)
{
  TacticalShipBase* ship = FindShip(handle);
  if (ship)
    return shipNode(ship);
  auto found = m_viewGroups.find(static_cast<uint32_t>(handle));
  return found == m_viewGroups.end() ? nullptr : static_cast<IdNode*>(found->second.get());
}

IdNode* TacticalRegistry::FindViewGroup(int handle)
{
  auto found = m_viewGroups.find(static_cast<uint32_t>(handle));
  return found == m_viewGroups.end() ? nullptr : static_cast<IdNode*>(found->second.get());
}

IdNode* TacticalRegistry::FindFighterGroup(int handle)
{
  auto found = m_fighterGroups.find(static_cast<uint32_t>(handle));
  return found == m_fighterGroups.end() ? nullptr : static_cast<IdNode*>(found->second.get());
}

IdNode* TacticalRegistry::FindCapitalShipGroup(int handle)
{
  auto found = m_capitalGroups.find(static_cast<uint32_t>(handle));
  return found == m_capitalGroups.end() ? nullptr : static_cast<IdNode*>(found->second.get());
}

IdNode* TacticalRegistry::FindShipGroup(int handle)
{
  IdNode* node = FindFighterGroup(handle);
  return node ? node : FindCapitalShipGroup(handle);
}

IdNode* TacticalRegistry::RemoveShipGroup(int handle)
{
  auto fighter = m_fighterGroups.find(static_cast<uint32_t>(handle));
  if (fighter != m_fighterGroups.end())
  {
    auto* node = static_cast<IdNode*>(fighter->second.release());
    m_fighterGroups.erase(fighter);
    return node;
  }
  auto capital = m_capitalGroups.find(static_cast<uint32_t>(handle));
  if (capital == m_capitalGroups.end())
    return nullptr;
  auto* node = static_cast<IdNode*>(capital->second.release());
  m_capitalGroups.erase(capital);
  return node;
}

uint32_t TacticalRegistry::AddShip(TacticalShipBase* ship)
{
  if (m_clearing)
    return 0;
  if (!ship)
    return 0;
  auto* node = static_cast<TacticalUnit*>(ship);
  uint32_t id = node->m_id;
  const bool assignedId = id == 0;
  if (assignedId)
  {
    if (m_nextShipId == 0)
      return 0;
    id = m_nextShipId;
    node->SetId(id);
  }
  decltype(m_ships)::iterator position;
  bool inserted = false;
  try
  {
    auto result = m_ships.emplace(id, nullptr);
    position = result.first;
    inserted = result.second;
  }
  catch (...)
  {
    if (assignedId)
      node->SetId(0);
    throw;
  }
  if (!inserted)
  {
    if (assignedId)
      node->SetId(0);
    return 0;
  }
  position->second.reset(ship);
  if (m_nextShipId != 0 && m_nextShipId <= id)
    m_nextShipId = id + 1;
  return id;
}

uint32_t TacticalRegistry::AddViewGroup(TacticalUnit* group)
{
  if (m_clearing)
    return 0;
  if (!group)
    return 0;
  const bool assignedId = group->m_id == 0;
  uint32_t id = assignedId ? m_nextViewGroupId : group->m_id;
  if (id == 0)
    return 0;
  if (assignedId)
    group->SetId(id);
  decltype(m_viewGroups)::iterator position;
  bool inserted = false;
  try
  {
    auto result = m_viewGroups.emplace(id, nullptr);
    position = result.first;
    inserted = result.second;
  }
  catch (...)
  {
    if (assignedId)
      group->SetId(0);
    throw;
  }
  if (!inserted)
  {
    if (assignedId)
      group->SetId(0);
    return 0;
  }
  position->second.reset(group);
  if (m_nextViewGroupId != 0 && m_nextViewGroupId <= id)
    m_nextViewGroupId = id + 1;
  return id;
}

uint32_t TacticalRegistry::AddFighterGroup(FighterGroup* group)
{
  if (m_clearing)
    return 0;
  if (!group)
    return 0;
  TacticalUnit* unit = group;
  const bool assignedId = unit->m_id == 0;
  uint32_t id = assignedId ? m_nextFighterGroupId : unit->m_id;
  if (id == 0)
    return 0;
  if (assignedId)
    unit->SetId(id);
  decltype(m_fighterGroups)::iterator position;
  bool inserted = false;
  try
  {
    auto result = m_fighterGroups.emplace(id, nullptr);
    position = result.first;
    inserted = result.second;
  }
  catch (...)
  {
    if (assignedId)
      unit->SetId(0);
    throw;
  }
  if (!inserted)
  {
    if (assignedId)
      unit->SetId(0);
    return 0;
  }
  position->second.reset(group);
  if (m_nextFighterGroupId != 0 && m_nextFighterGroupId <= id)
    m_nextFighterGroupId = id + 1;
  return id;
}

uint32_t TacticalRegistry::AddCapitalShipGroup(CapitalShipGroup* group)
{
  if (m_clearing)
    return 0;
  if (!group)
    return 0;
  TacticalUnit* unit = group;
  const bool assignedId = unit->m_id == 0;
  uint32_t id = assignedId ? m_nextCapitalGroupId : unit->m_id;
  if (id == 0)
    return 0;
  if (assignedId)
    unit->SetId(id);
  decltype(m_capitalGroups)::iterator position;
  bool inserted = false;
  try
  {
    auto result = m_capitalGroups.emplace(id, nullptr);
    position = result.first;
    inserted = result.second;
  }
  catch (...)
  {
    if (assignedId)
      unit->SetId(0);
    throw;
  }
  if (!inserted)
  {
    if (assignedId)
      unit->SetId(0);
    return 0;
  }
  position->second.reset(group);
  if (m_nextCapitalGroupId != 0 && m_nextCapitalGroupId <= id)
    m_nextCapitalGroupId = id + 1;
  return id;
}

uint32_t TacticalRegistry::AddShipGroup(TacticalUnit* group)
{
  if (m_clearing || !group)
    return 0;
  if (kindOf(group) == kFighterGroupKind)
    return AddFighterGroup(static_cast<FighterGroup*>(group));
  if (kindOf(group) == kCapitalGroupKind)
    return AddCapitalShipGroup(static_cast<CapitalShipGroup*>(group));
  return 0;
}

TacticalUnit* TacticalRegistry::GetDeathStar()
{
  return m_deathStar;
}

TacticalUnit::TacticalUnit()
{
  SetId(0);
}

TacticalUnit::~TacticalUnit() = default;

void TacticalUnit::AssignHandle(int side)
{
  SetId(GetTacticalRegistry()->NextUnitHandle(side));
}

void TacticalUnit::AssignGroupId(int side)
{
  SetId(GetTacticalRegistry()->NextGroupHandle(side));
}
