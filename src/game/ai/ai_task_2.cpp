#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_task_family.h"
#include "ai/ai_task_types.h"
#include "ai/ai_task_util.h"
#include "core/checked_new.h"
#include "economy/manufacturing_facility.h"
#include "galaxy/system.h"
#include "gameobj/object_lookup.h"

using namespace aitask;

namespace
{

constexpr FamilyParams kParams = {0,
                                  0,
                                  100,
                                  0xd8,
                                  0xd8,
                                  0x60,
                                  0x80,
                                  0x15,
                                  0x3800003,
                                  6,
                                  0x1000000,
                                  0xc4,
                                  0x10000,
                                  0x40,
                                  8,
                                  6,
                                  FacilityKind::kManufacturing28,
                                  0x200000,
                                  FacilityKind::kManufacturing29,
                                  0x400000,
                                  false,
                                  false,
                                  true,
                                  false};

constexpr int kScrapOrder = 0x200;
constexpr int kStageShields = 0x3ea;
constexpr int kStageIons = 0x3eb;
constexpr int kStageFacilities = 0x3ec;
constexpr uint32_t kSystemClaimed = 0x1000000;
constexpr uint32_t kShieldClassKey = 0x2d000002;
constexpr uint32_t kIonClassKey = 0x2c000001;

} // namespace

AiTask2::AiTask2(int view)
: AiStagedTask(2, view),
  m_shieldSystem(),
  m_ionSystem(),
  m_system(),
  m_manager(),
  m_classKey(),
  m_targets(),
  m_dropped(),
  m_selector()
{
  m_classKey.m_value = 0;
  m_stage = 0;
  m_step = 0;
  m_count = 0;
  m_cap = 0;
  m_limit = 0;
  m_targets.Clear();
  m_dropped.Clear();
  m_x24 = 3;
}

AiTask2::~AiTask2() = default;

FamilyFields AiTask2::Fields()
{
  return {m_owner,   m_view,     m_x24,       &m_selector, &m_targets, &m_dropped,
          &m_system, &m_manager, &m_classKey, &m_count,    &m_cap,     &m_limit};
}

void AiTask2::Read(istream* in)
{
  AiTaskBase::Read(in);
  ReadValue32(in, &m_stage);
  ReadValue32(in, &m_step);
  ReadValue32(in, &m_count);
  ReadValue32(in, &m_cap);
  ReadValue32(in, &m_limit);
  m_shieldSystem.Read(in);
  m_ionSystem.Read(in);
  m_system.Read(in);
  m_manager.Read(in);
  m_classKey.Read(in);
  m_selector.Read(in);
  m_targets.Load(in);
  m_dropped.Load(in);
}

void AiTask2::Write(ostream* out)
{
  AiTaskBase::Write(out);
  WriteValue32(out, &m_stage);
  WriteValue32(out, &m_step);
  WriteValue32(out, &m_count);
  WriteValue32(out, &m_cap);
  WriteValue32(out, &m_limit);
  m_shieldSystem.Write(out);
  m_ionSystem.Write(out);
  m_system.Write(out);
  m_manager.Write(out);
  m_classKey.Write(out);
  m_selector.Write(out);
  m_targets.Save(out);
  m_dropped.Save(out);
}

OrderBase* AiTask2::Step()
{
  if (m_flags.Get() != 1)
  {
    m_stage = 0;
    m_step = 0;
    m_finished = 1;
    return nullptr;
  }
  const uint32_t stage = m_stage;
  if (stage == 0)
  {
    if (ChooseManager() != 0)
    {
      m_stage = (m_owner->Stat(0x1d4) <= m_owner->Stat(0x1d0) ? 1 : 0) + kStageShields;
      return nullptr;
    }
    m_stage = kStageFacilities;
    return nullptr;
  }
  if (stage > 0x3e9)
  {
    if (stage < 0x3ec)
      return StepDefenses();
    if (stage == 0x3ec)
      return StepFacilities();
  }
  m_stage = 0;
  return nullptr;
}

int AiTask2::ChooseManager()
{
  int result = 0;
  bool found = false;
  ObjectKeyNode* node = m_targets.First();
  while (node && !found)
  {
    m_manager = KeyOfNode(node);
    ObjectKey& key = m_manager;
    if (!IsSystemKey(key))
    {
      ObjectKeyNode* next = NextNode(node);
      ObjectKey nodeKey = KeyOfNode(node);
      m_targets.RemoveKey(&nodeKey);
      node = next;
      continue;
    }
    AiSystemEval* eval = m_owner->m_systems.FindEval(key);
    if (!eval)
    {
      ObjectKeyNode* next = NextNode(node);
      ObjectKey nodeKey = KeyOfNode(node);
      m_targets.RemoveKey(&nodeKey);
      node = next;
      continue;
    }
    if ((eval->m_flags30.m_value & 1) != 0 && (eval->m_flags28.m_value & kSystemClaimed) != 0)
    {
      if (0 < eval->Stat(0x60) && (eval->m_flags28.m_value & 0x800) != 0 && (eval->m_flags28.m_value & 3) == 0)
        found = true;
      node = NextNode(node);
      continue;
    }
    eval->m_flags28.m_value &= ~kSystemClaimed;
    eval->MarkChanged();
    ObjectKeyNode* next = NextNode(node);
    ObjectKey nodeKey = KeyOfNode(node);
    m_targets.RemoveKey(&nodeKey);
    node = next;
  }
  if (!found)
  {
    m_manager.Reset();
    return result;
  }
  System* system = static_cast<System*>(GetObjectInFamilies_90_98(m_view, &m_manager));
  if (system)
  {
    GameObjNotifier* manager = system->FindManufacturingManagerBySlot123(1);
    if (manager)
    {
      ObjectKey key;
      GetObjectKey(manager, &key);
      m_manager = key;
      result = 1;
    }
  }
  return result;
}

OrderBase* AiTask2::StepFacilities()
{
  auto finish = [this](OrderBase* order)
  {
    m_step = 0;
    m_stage = 0;
    m_finished = 1;
    return order;
  };
  OrderBase* order = nullptr;
  switch (m_step)
  {
    case 1:
      m_step = PickSystem() != 0 ? 3 : 2;
      return nullptr;
    case 2:
      order = BuildFirstScrapOrder();
      if (order)
      {
        m_finished = 1;
        ++m_priority;
      }
      m_step = 3;
      return order;
    case 3:
      if (SelectTarget() != 0)
      {
        m_step = 7;
        return nullptr;
      }
      break;
    case 4:
      order = BuildScrapOrder();
      if (order)
      {
        m_finished = 1;
        ++m_priority;
      }
      m_step = 5;
      return order;
    case 5:
      m_step = ComputeFacilityCount();
      if (m_step != 0)
        return nullptr;
      return finish(nullptr);
    case 6:
      return finish(BuildCapitalOrder());
    case 7:
      m_owner->m_bitsA &= ~0x40u;
      if (m_owner->m_bitB == 0x40 && PickSupplySystem() != 0)
      {
        m_owner->AdvanceBitCursor();
        m_step = 4;
        return nullptr;
      }
      m_step = 10;
      return nullptr;
    case 8:
    {
      OrderBase* destination = BuildDestinationOrder(&m_system);
      if (destination)
      {
        m_step = 9;
        m_finished = 1;
        ++m_priority;
        return destination;
      }
      break;
    }
    case 9:
      return finish(BuildManufactureOrder210());
    case 10:
      AddTargetSystem();
      RefreshTargets();
      break;
    default:
      m_step = 1;
      return nullptr;
  }
  return finish(nullptr);
}

OrderBase* AiTask2::StepDefenses()
{
  OrderBase* order = nullptr;
  const bool ions = m_stage == kStageIons;
  ObjectKey* target = ions ? &m_ionSystem : &m_shieldSystem;
  switch (m_step)
  {
    case 1:
      m_step = PickSystem() != 0 ? 3 : 2;
      return nullptr;
    case 2:
      order = BuildFirstScrapOrder();
      if (order)
      {
        m_finished = 1;
        ++m_priority;
      }
      m_step = 3;
      return order;
    case 3:
      if ((ions ? PickIonTarget() : PickShieldTarget()) != 0)
      {
        m_step = 4;
        return nullptr;
      }
      break;
    case 4:
      order = BuildAllScrapOrder(target);
      if (order)
      {
        m_finished = 1;
        ++m_priority;
      }
      m_step = 8;
      return order;
    default:
      m_step = 1;
      return nullptr;
    case 8:
    {
      OrderBase* destination = BuildDestinationOrder(target);
      if (destination)
      {
        m_step = 9;
        m_finished = 1;
        ++m_priority;
        return destination;
      }
      break;
    }
    case 9:
      m_classKey.m_value = ions ? kShieldClassKey : kIonClassKey;
      m_count = 1;
      order = BuildManufactureOrder210();
      break;
  }
  m_step = 0;
  m_stage = 0;
  m_finished = 1;
  return order;
}

int AiTask2::PickSystem()
{
  int result = 1;
  ObjectKey scratch;
  AiSideController* owner = m_owner;
  if (owner->Stat(0x184) < owner->Stat(0x188))
  {
    m_selector.Add(owner->RankSectors(0xe0, 0, 0, 0, 0, 0, 0x15, 1));
    m_selector.Add(owner->RankSectors(0xe0, 0, 0, 0, 0, 0, 0x11, 1));
    m_selector.Add(owner->RankSectors(0xe0, 0, 0, 0, 0, 0, 0x13, 1));
    m_system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(owner->RankSystemsOfSector(&m_system, 0, 0, 1, 0x3e00000, 0, 0, 6, 1));
    m_selector.Add(owner->RankSystemsOfSector(&m_system, 0, 0, 1, 0x3e00000, 0, 0, 4, 1));
    m_selector.Add(owner->RankSystemsOfSector(&m_system, 0, 0, 1, 0x3e00000, 0, 0, 5, 1));
    m_system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (IsSystemKey(m_system))
      result = 0;
  }
  return result;
}

OrderBase* AiTask2::BuildFirstScrapOrder()
{
  ObjectKeyValueSet team;
  OrderBase* order = nullptr;
  if (IsSystemKey(m_system))
  {
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_system);
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_view, &m_system);
    if (system && eval)
    {
      ManufacturingFacility28Iterator walkers(system, 1);
      walkers.Begin();
      if (walkers.Current())
      {
        if (AddCurrentToTeam(&team, &walkers) == nullptr)
        {
          if (AddFirstNode<ManufacturingFacility29Iterator>(&team, system) == nullptr)
            AddFirstNode<ManufacturingFacility2AIterator>(&team, system);
        }
      }
    }
  }
  if (team.First())
    order = NewTeamOrder(kScrapOrder, m_view, &team);
  return order;
}

int AiTask2::PickShieldTarget()
{
  int result = 0;
  ObjectKey scratch;
  if (IsSystemKey(m_shieldSystem))
  {
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_shieldSystem);
    if ((eval->m_flags28.m_value & 3) == 0 && (eval->m_flags28.m_value & 0x80) != 0)
      result = 1;
  }
  if (result == 0)
  {
    m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x100, 0, 0, 0, 0, 0, 2));
    m_shieldSystem = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSectorByDistance(&m_shieldSystem, &m_manager, 0x80, 0, 0, 3, 0, 0, 2));
    m_shieldSystem = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (!IsSystemKey(m_shieldSystem))
    {
      m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x800, 0, 0, 0, 0, 0, 2));
      m_shieldSystem = *m_selector.PickBest(&scratch);
      m_selector.Reset();
      m_selector.Add(m_owner->RankSystemsOfSectorByDistance(&m_shieldSystem, &m_manager, 0x40, 0, 0, 3, 0, 0, 2));
      m_shieldSystem = *m_selector.PickBest(&scratch);
      m_selector.Reset();
    }
    if (IsSystemKey(m_shieldSystem))
      result = 1;
  }
  return result;
}

int AiTask2::PickIonTarget()
{
  int result = 0;
  ObjectKey scratch;
  if (IsSystemKey(m_ionSystem))
  {
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_ionSystem);
    if ((eval->m_flags28.m_value & 3) == 0 && (eval->m_flags28.m_value & 0x100) != 0)
      result = 1;
  }
  if (result == 0)
  {
    m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x200, 0, 0, 0, 0, 0, 2));
    m_ionSystem = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSectorByDistance(&m_ionSystem, &m_manager, 0x100, 0, 0, 3, 0, 0, 2));
    m_ionSystem = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (!IsSystemKey(m_ionSystem))
    {
      m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x400, 0, 0, 0, 0, 0, 2));
      m_ionSystem = *m_selector.PickBest(&scratch);
      m_selector.Reset();
      m_selector.Add(m_owner->RankSystemsOfSectorByDistance(&m_ionSystem, &m_manager, 0x20, 0, 0, 3, 0, 0, 2));
      m_ionSystem = *m_selector.PickBest(&scratch);
      m_selector.Reset();
    }
    if (IsSystemKey(m_ionSystem))
      result = 1;
  }
  return result;
}

OrderBase* AiTask2::BuildAllScrapOrder(ObjectKey* systemKey)
{
  ObjectKeyValueSet team;
  OrderBase* order = nullptr;
  ObjectKeyValueNode* last = nullptr;
  if (IsSystemKey(*systemKey))
  {
    AiSystemEval* eval = m_owner->m_systems.FindEval(*systemKey);
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_view, systemKey);
    if (system && eval)
    {
      if ((eval->m_flags28.m_value & 0x200000) == 0)
        last = AddAllOfKind<ManufacturingFacility28Iterator>(&team, system, last);
      if ((eval->m_flags28.m_value & 0x400000) == 0)
        last = AddAllOfKind<ManufacturingFacility29Iterator>(&team, system, last);
      if ((eval->m_flags28.m_value & 0x3800000) == 0)
        last = AddAllOfKind<ManufacturingFacility2AIterator>(&team, system, last);
    }
  }
  if (team.First())
    order = NewTeamOrder(kScrapOrder, m_view, &team);
  return order;
}

int AiTask2::SelectTarget()
{
  int result = 0;
  int best = 1000;
  ObjectKey scratch;
  m_system.Reset();
  m_limit = 0;
  for (ObjectKeyNode* node = m_targets.First(); node; node = NextNode(node))
  {
    AiSystemEval* eval = m_owner->m_systems.FindEval(KeyOfNode(node));
    if (eval && (eval->m_flags30.m_value & 1) != 0 && (eval->m_flags28.m_value & 3) == 0 && 0 < eval->Stat(0x114) &&
        eval->Stat(0x60) < best)
    {
      m_limit = eval->Stat(0x114);
      best = eval->Stat(0x60);
      m_system = KeyOfNode(eval);
      result = 1;
    }
  }
  if (result == 0)
  {
    m_selector.Add(m_owner->RankSectors(0x80, 0, 0, 0, 0, 0, 0x15, 1));
    m_selector.Add(m_owner->RankSectors(0x80, 0, 0, 0, 0, 0, 4, 2));
    m_system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0, 0, 1, 0x3800003, 0, 0x40000000, 6, 1));
    if (m_targets.Count() != 0)
      m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0, 0, 1, 0x3800003, 0, 0x40000000, 0x33, 1));
    m_system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (IsSystemKey(m_system))
    {
      result = 1;
      AiSystemEval* eval = m_owner->m_systems.FindEval(m_system);
      if (eval)
      {
        ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(m_system);
        if (node)
        {
          m_targets.Add(node);
          eval->m_flags28.m_value |= kSystemClaimed;
          eval->MarkChanged();
        }
      }
    }
  }
  return result;
}

int AiTask2::PickSupplySystem()
{
  return aitask::PickSupplySystemNear(Fields(), kParams);
}

OrderBase* AiTask2::BuildScrapOrder()
{
  return aitask::BuildScrapOrder(Fields(), kParams);
}

int AiTask2::ComputeFacilityCount()
{
  return aitask::ComputeCount(Fields(), kParams);
}

OrderBase* AiTask2::BuildCapitalOrder()
{
  return aitask::BuildCapitalOrder(Fields(), kParams, FacilityKind::kManufacturing2A);
}

OrderBase* AiTask2::BuildDestinationOrder(ObjectKey* target)
{
  return aitask::BuildDestinationOrder(Fields(), target, 0xa0);
}

OrderBase* AiTask2::BuildManufactureOrder210()
{
  return aitask::BuildManufactureOrder(Fields(), 0x210, 0xa0);
}

void AiTask2::AddTargetSystem()
{
  aitask::AddTargetSystem(Fields(), kParams);
}

int AiTask2::NeedForTargets()
{
  return aitask::NeedForTargets(Fields(), kParams);
}

void AiTask2::RefreshTargets()
{
  aitask::RefreshTargets(Fields(), kParams);
}
