#include "core/reb_x87.h"
#include "tactical/tactical_externals.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_groups.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_ship.h"
#include "tactical/tactical_simulator.h"

// The headless simulator's handling of units (0x5af4e0-0x5afe40): putting them into the right lists, taking them out
// of the battle, and the commanders' skill figures.

namespace
{

TacticalShipBase* findShip(int handle)
{
  return GetTacticalRegistry()->FindShip(handle);
}

ShipGroup* findGroup(int handle)
{
  return ShipGroupOf(GetTacticalRegistry()->FindShipGroup(handle));
}

TacticalCharacterUnit* findCharacter(IdAvlTree& characters, int handle)
{
  return static_cast<TacticalCharacterUnit*>(characters.FindById(handle));
}

// Takes the unit's group (if any) out of the side's list once the group has lost its last member.
void leaveGroup(HeadlessSimulator* simulator, TacticalShipBase* unit)
{
  ShipGroup* group = findGroup(unit->m_groupHandle);
  if (group == nullptr)
    return;
  RemoveMemberFromGroup(group, unit->m_id);
  if (group->m_firstMember == 0)
    reinterpret_cast<UnitRefTree*>(&simulator->m_sides[group->m_side]->m_groups)->RemoveById(group->m_id);
}

void removeFromStateList(HeadlessSimulator* simulator, TacticalShipBase* unit)
{
  if (unit->m_state == 2)
  {
    simulator->m_active.RemoveById(unit->m_id);
  }
  else if (unit->m_state == 1)
  {
    simulator->m_launched.RemoveById(unit->m_id);
  }
}

} // namespace

int HeadlessSimulator::ActivateUnit(TacticalShipBase* ship)
{
  m_units.AddUnit(ship);
  ship->m_state = 2;
  return ship->m_id;
}

void HeadlessSimulator::RetireUnit(TacticalShipBase* unit)
{
  if (unit->m_state == 0xb)
    return;
  removeFromStateList(this, unit);
  unit->m_state = 0xb;
  unit->TakeDamage(100000.0f);
  m_retired.AddUnit(unit);
  leaveGroup(this, unit);
  if (unit->GetUnitClass() != 0)
  {
    m_sides[unit->m_side]->m_hangars.RemoveById(unit->m_id);
    return;
  }
  m_sides[unit->m_side]->m_fleet.RemoveById(unit->m_id);
  TacticalCapitalShip* ship = static_cast<TacticalCapitalShip*>(unit);
  for (UnitRefNode* node = ship->m_docked->FirstNode(); node;)
  {
    UnitRefNode* next = node->Next();
    RetireUnit(node->Value());
    node = next;
  }
  ship->m_docked->Clear();
  for (UnitRefNode* node = ship->m_launched->FirstNode(); node;)
  {
    UnitRefNode* next = node->Next();
    RetireUnit(node->Value());
    node = next;
  }
  ship->m_launched->Clear();
}

void HeadlessSimulator::DockAtDeathStar(TacticalShipBase* unit)
{
  if (unit->m_state == 9)
    return;
  if (unit->m_state == 0xb)
    return;
  removeFromStateList(this, unit);
  unit->m_state = 9;
  leaveGroup(this, unit);
  if (unit->GetUnitClass() == 0)
  {
    m_sides[unit->m_side]->m_fleet.RemoveById(unit->m_id);
    return;
  }
  m_sides[unit->m_side]->m_hangars.RemoveById(unit->m_id);
}

double HeadlessSimulator::OpponentStrength(int side)
{
  if (side == 0)
    return m_sides[1]->m_strength;
  return m_sides[0]->m_strength;
}

void HeadlessSimulator::SetSideFlag(int side, int value)
{
  m_sides[side]->m_unk80 = value;
}

void HeadlessSimulator::SetSideValue(int side, float value)
{
  m_sides[side]->m_unk7c = value;
}

double HeadlessSimulator::CommanderSkillA(int side)
{
  const int handle = m_firstCommander[side];
  if (handle != 0)
  {
    TacticalCharacterUnit* commander = findCharacter(m_characters, handle);
    if (commander != nullptr)
    {
      TacticalShipBase* ship = findShip(commander->m_shipHandle);
      if (ship != nullptr && IsActiveUnit(ship) != 0)
      {
        double skill = 9.0f - static_cast<double>(commander->m_scaleA);
        if (skill <= 1.0f)
          skill = 1.0f;
        if (skill < 9.0f)
          return skill;
        return 9.0f;
      }
    }
  }
  return 1.0f;
}

double HeadlessSimulator::CommanderSkillB(int side)
{
  const int handle = m_secondCommander[side];
  if (handle != 0)
  {
    TacticalCharacterUnit* commander = findCharacter(m_characters, handle);
    if (commander != nullptr)
    {
      TacticalShipBase* ship = findShip(commander->m_shipHandle);
      if (ship != nullptr)
      {
        double skill = static_cast<double>(commander->m_scaleB) - -1.0f;
        if (skill <= 1.0f)
          skill = 1.0f;
        if (skill < 9.0f)
          return skill;
        return 9.0f;
      }
    }
  }
  return 1.0f;
}

int HeadlessSimulator::IsActiveUnit(TacticalShipBase* ship)
{
  if (ship != nullptr && m_active.Find(ship->m_id) != nullptr && ship->m_state == 2)
    return 1;
  return 0;
}

void HeadlessSimulator::RegisterFighter(TacticalShipBase* unit)
{
  unit->AssignHandle(unit->m_side);
  GetTacticalRegistry()->AddShip(unit);
}

void HeadlessSimulator::RegisterCapitalShip(TacticalShipBase* unit)
{
  unit->AssignHandle(unit->m_side);
  GetTacticalRegistry()->AddShip(unit);
  static_cast<TacticalCapitalShip*>(unit)->m_ownerHandle = unit->m_id;
}

void HeadlessSimulator::AssignCharacterHandle(TacticalUnit* unit)
{
  unit->AssignHandle(static_cast<TacticalCharacterUnit*>(unit)->m_notAlliance);
}

// ---- construction ---------------------------------------------------------------------------------------------------

SimulatorSideA::~SimulatorSideA() = default;

SimulatorSideB::~SimulatorSideB() = default;

HeadlessSimulator::HeadlessSimulator()
{
  m_elapsed = 0;
  m_random.Seed(0xbc614e);
  m_unk0c = 1;
  m_sides[0] = new SimulatorSideA();
  m_sides[1] = new SimulatorSideB();
  for (int side = 0; side < 2; ++side)
  {
    m_secondCommander[side] = 0;
    m_firstCommander[side] = 0;
    m_bestStrength[side] = 0;
    m_stagnantTime[side] = 0;
  }
}

HeadlessSimulator::~HeadlessSimulator()
{
  delete m_sides[0];
  m_sides[0] = nullptr;
  delete m_sides[1];
  m_sides[1] = nullptr;
}

// ---- the Death Star's strike on a capital-ship group (0x5aefc0) and target assignment (0x5afa00) ----------------------

namespace
{

constexpr float kHullPerDamageStep = 8.0f;
constexpr float kBestSkill = 9.0f;
constexpr float kStrikeChanceScale = 0.5f;
constexpr uint32_t kMaxVisits = 100;

void retireAll(HeadlessSimulator* simulator, UnitRefTree* units, bool clearAfterwards)
{
  for (UnitRefNode* node = units->FirstNode(); node;)
  {
    UnitRefNode* next = node->Next();
    simulator->RetireUnit(node->Value());
    node = next;
  }
  if (clearAfterwards)
    units->Clear();
}

} // namespace

// Walks (cyclically, at most 100 visits) over a copy of the group's ships. A ship that out-rolls its turn rate loses hull
// or is retired; one with hull left may then be picked by the Death Star: its weapon strike removes the ships docked in and
// launched from the Death Star and ends the run.
void HeadlessSimulator::DeathStarAttackRun(ShipGroup* group)
{
  UnitRefTree units;
  units.CopyFrom(static_cast<UnitRefTree*>(group));
  uint32_t visits = 0;
  UnitRefNode* node = units.FirstNode();
  if (node == nullptr)
    return;
  for (;;)
  {
    UnitRefNode* next = node->Next();
    if (next == nullptr)
      next = units.FirstNode();
    if (++visits > kMaxVisits)
    {
      retireAll(this, &units, false);
      return;
    }
    TacticalShipBase* ship = node->Value();
    const uint32_t turnRate = static_cast<uint32_t>(reb_ftol(ship->GetTurnRate()));
    bool retired = false;
    if (static_cast<uint32_t>(GetHeadlessSimulator()->RandomBetween(1, 10)) > turnRate)
    {
      const uint32_t steps = static_cast<uint32_t>(reb_ftol(ship->m_hull / kHullPerDamageStep));
      if (steps <= 1)
      {
        RetireUnit(ship);
        retired = true;
      }
      else
      {
        const uint32_t roll = static_cast<uint32_t>(GetHeadlessSimulator()->RandomBetween(1, static_cast<int>(steps)));
        ship->TakeDamage(static_cast<float>(static_cast<double>(roll) * kHullPerDamageStep));
        if (!(ship->m_hull > 0.0f))
        {
          RetireUnit(ship);
          retired = true;
        }
      }
      if (retired)
        units.RemoveById(static_cast<int>(ship->m_id));
    }
    if (retired)
    {
      if (units.FirstNode() == nullptr)
        return;
    }
    else if (ship->m_hull > 0.0f)
    {
      const double skillB = GetHeadlessSimulator()->CommanderSkillB(0);
      const float skill =
        skillB < kBestSkill ? static_cast<float>(GetHeadlessSimulator()->CommanderSkillB(0)) : kBestSkill;
      const float chance = static_cast<float>((ship->TorpedoStrength() + skill) * kStrikeChanceScale);
      const uint32_t strikeRoll = static_cast<uint32_t>(GetHeadlessSimulator()->RandomBetween(1, 100));
      if (strikeRoll <= static_cast<uint32_t>(reb_ftol(chance)))
      {
        TacticalDeathStar* deathStar = static_cast<TacticalDeathStar*>(GetTacticalRegistry()->GetDeathStar());
        if (deathStar != nullptr)
        {
          deathStar->ResetCharged();
          retireAll(this, deathStar->m_docked, true);
          retireAll(this, deathStar->m_launched, true);
        }
        return;
      }
    }
    node = next;
    if (units.FirstNode() == nullptr)
      return;
  }
}

void HeadlessSimulator::AssignTarget(TacticalShipBase* target, ShipGroup* group)
{
  group->m_formationA.Clear();
  group->m_formationA.AddUnitLinear(target);
}
