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
                                  0x800000,
                                  0xc4,
                                  0x10000,
                                  0x80,
                                  0,
                                  0,
                                  FacilityKind::kManufacturing28,
                                  0x200000,
                                  FacilityKind::kManufacturing29,
                                  0x400000,
                                  false,
                                  false,
                                  false,
                                  false};

constexpr int kScrapOrder = 0x200;
constexpr int kStageBuild = 0x3ec;
constexpr uint32_t kSystemClaimed = 0x800000;
constexpr uint32_t kShieldClassKey = 0x2d000002;
constexpr uint32_t kIonClassKey = 0x2c000001;

} // namespace

AiTask1::AiTask1(int view)
: AiStagedTask(1, view),
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
  m_limit = 0;
  m_cap = 0;
  m_targets.Clear();
  m_dropped.Clear();
  m_x24 = 4;
}

AiTask1::~AiTask1() = default;

FamilyFields AiTask1::Fields()
{
  return {m_owner,   m_view,     m_x24,       &m_selector, &m_targets, &m_dropped,
          &m_system, &m_manager, &m_classKey, &m_count,    &m_cap,     &m_limit};
}

void AiTask1::Read(istream* in)
{
  AiTaskBase::Read(in);
  ReadValue32(in, &m_stage);
  ReadValue32(in, &m_step);
  ReadValue32(in, &m_count);
  ReadValue32(in, &m_limit);
  ReadValue32(in, &m_cap);
  m_system.Read(in);
  m_targets.Load(in);
  m_dropped.Load(in);
  m_manager.Read(in);
  m_classKey.Read(in);
  m_selector.Read(in);
}

void AiTask1::Write(ostream* out)
{
  AiTaskBase::Write(out);
  WriteValue32(out, &m_stage);
  WriteValue32(out, &m_step);
  WriteValue32(out, &m_count);
  WriteValue32(out, &m_limit);
  WriteValue32(out, &m_cap);
  m_system.Write(out);
  m_targets.Save(out);
  m_dropped.Save(out);
  m_manager.Write(out);
  m_classKey.Write(out);
  m_selector.Write(out);
}

OrderBase* AiTask1::Step()
{
  if (m_flags.Get() != 1)
  {
    m_stage = 0;
    m_step = 0;
    m_finished = 1;
    return nullptr;
  }
  if (m_stage != 0)
  {
    if (m_stage == kStageBuild)
      return StepBuild();
    m_stage = 0;
    return nullptr;
  }
  if (ChooseTarget() != 0)
  {
    m_stage = kStageBuild;
    return nullptr;
  }
  if (PickSystem() != 0)
  {
    m_stage = kStageBuild;
    m_step = 0;
    return nullptr;
  }
  if (!(m_owner->m_bitB == 0x80 && (m_owner->m_flags.Get() & 0x40) == 0))
    AddTargetSystem();
  RefreshTargets();
  m_stage = 0;
  m_step = 0;
  m_finished = 1;
  return nullptr;
}

int AiTask1::ChooseTarget()
{
  int result = 0;
  bool found = false;
  m_cap = 0;
  ObjectKeyNode* node = m_targets.First();
  while (node && !found)
  {
    ObjectKey key = KeyOfNode(node);
    AiSystemEval* eval = m_owner->m_systems.FindEval(key);
    if (!eval)
    {
      ObjectKeyNode* next = NextNode(node);
      m_targets.RemoveKey(&key);
      node = next;
      continue;
    }
    if ((eval->m_flags30.m_value & 1) != 0 && (eval->m_flags28.m_value & kSystemClaimed) != 0)
    {
      if ((eval->m_flags28.m_value & 0x800) == 0)
      {
        if (m_targets.Count() == 1 && eval->Stat(0x60) == 0)
        {
          eval->m_flags28.m_value &= ~kSystemClaimed;
          eval->MarkChanged();
          ObjectKeyNode* next = NextNode(node);
          m_targets.RemoveKey(&key);
          node = next;
          continue;
        }
      }
      else if ((eval->m_flags28.m_value & 3) == 0)
      {
        m_cap = eval->Stat(0x60);
        m_manager = KeyOfNode(node);
        found = true;
      }
      node = NextNode(node);
      continue;
    }
    eval->m_flags28.m_value &= ~kSystemClaimed;
    eval->MarkChanged();
    ObjectKeyNode* next = NextNode(node);
    m_targets.RemoveKey(&key);
    node = next;
  }
  if (IsSystemKey(m_manager))
  {
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
  }
  return result;
}

int AiTask1::PickSystem()
{
  int result = 0;
  ObjectKey scratch;
  m_cap = 0;
  if (m_targets.Count() != 0)
    return result;
  m_selector.Add(m_owner->RankSectors(0x2000, 0, 0, 0, 0, 0, 4, 2));
  m_selector.Add(m_owner->RankSectors(0x2000, 0, 0, 0, 0, 0, 0x15, 1));
  m_selector.Add(m_owner->RankSectors(0x2000, 0, 0, 0, 0, 0, 8, 1));
  m_system = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0x800, 0, 1, 0x3800000, 0, 0, 6, 1));
  m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0x800, 0, 1, 0x3800000, 0, 0, 0, 1));
  m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0x800, 0, 1, 0x3800000, 0, 0, 0x33, 1));
  m_system = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  if (!IsSystemKey(m_system))
  {
    m_selector.Add(m_owner->RankSectors(0, 0, 4, 0, 0, 0, 0x14, 1));
    m_system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0, 0, 1, 0, 0, 0, 6, 1));
    m_system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
  }
  if (!IsSystemKey(m_system))
    m_owner->m_flags.m_value |= 0x40;
  if (IsSystemKey(m_system))
  {
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_system);
    if (eval)
    {
      ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(m_system);
      if (node)
      {
        m_targets.Add(node);
        eval->m_flags28.m_value = (eval->m_flags28.m_value & 0xfc7fffff) | kSystemClaimed;
        eval->MarkChanged();
        if ((eval->m_flags28.m_value & 0x800) != 0)
        {
          result = 1;
          System* system = static_cast<System*>(GetObjectInFamilies_90_98(m_view, &m_system));
          if (system)
          {
            GameObjNotifier* manager = system->FindManufacturingManagerBySlot123(1);
            if (manager)
            {
              m_cap = eval->Stat(0x60);
              ObjectKey key;
              GetObjectKey(manager, &key);
              m_manager = key;
            }
          }
        }
      }
    }
  }
  return result;
}

OrderBase* AiTask1::StepBuild()
{
  auto finish = [this](OrderBase* order)
  {
    m_step = 0;
    m_stage = 0;
    m_finished = 1;
    return order;
  };
  switch (m_step)
  {
    case 1:
      m_owner->m_bitsA &= ~0x80u;
      if (m_owner->m_bitB != 0x80)
      {
        m_step = 10;
        return nullptr;
      }
      m_owner->AdvanceBitCursor();
      m_step = 2;
      return nullptr;
    case 2:
      if (SelectTarget() == 0)
      {
        m_stage = 0;
        m_step = 0;
        m_finished = 1;
        return nullptr;
      }
      m_step = 3;
      return nullptr;
    case 3:
    {
      OrderBase* order = BuildScrapOrder();
      if (order)
      {
        m_finished = 1;
        ++m_priority;
      }
      m_step = 4;
      return order;
    }
    case 4:
      m_step = ComputeFacilityCount();
      if (m_step == 0)
      {
        m_stage = 0;
        m_finished = 1;
      }
      return nullptr;
    case 5:
      return finish(BuildCapitalOrder());
    case 6:
    {
      int picked;
      if (m_owner->Stat(0x1d4) < m_owner->Stat(0x1d0))
      {
        m_classKey.m_value = kShieldClassKey;
        picked = PickIonTarget();
      }
      else
      {
        m_classKey.m_value = kIonClassKey;
        picked = PickShieldTarget();
      }
      if (picked == 0)
      {
        m_step = 0;
        m_stage = 0;
      }
      else
      {
        m_count = 1;
        m_step = 7;
      }
    }
      [[fallthrough]];
    case 7:
    {
      OrderBase* order = BuildAllScrapOrder();
      if (order)
      {
        m_finished = 1;
        ++m_priority;
      }
      m_step = 8;
      return order;
    }
    case 8:
    {
      OrderBase* order = BuildDestinationOrder();
      if (order)
      {
        m_step = 9;
        m_finished = 1;
        ++m_priority;
        return order;
      }
      finish(nullptr);
      return nullptr;
    }
    case 9:
      return finish(BuildManufactureOrder210());
    case 10:
      AddTargetSystem();
      finish(nullptr);
      return nullptr;
    default:
      m_step = 1;
      return nullptr;
  }
}

int AiTask1::SelectTarget()
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
    m_selector.Add(m_owner->RankSectors(0x80, 0, 0, 0, 0, 0, 4, 2));
    m_selector.Add(m_owner->RankSectors(0x80, 0, 0, 0, 0, 0, 0x15, 1));
    m_selector.Add(m_owner->RankSectors(0x80, 0, 0, 0, 0, 0, 8, 1));
    m_system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0, 0, 1, 0x3800003, 0, 0x40000000, 6, 1));
    m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0, 0, 1, 0x3800003, 0, 0x40000000, 0, 1));
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

OrderBase* AiTask1::BuildScrapOrder()
{
  return aitask::BuildScrapOrder(Fields(), kParams);
}

int AiTask1::ComputeFacilityCount()
{
  int result = 0;
  m_count = 0;
  if (!IsSystemKey(m_system))
    return 0;
  const int need = NeedForTargets();
  if (0 < need)
  {
    const int classes = m_owner->ChooseClass(&m_classKey, 0x2a, 0x10000, 0x4000, 2);
    if ((m_classKey.m_value & 0xff000000) != 0 && classes != 0)
      m_count = need / classes;
    if (0 < m_count)
    {
      AiSystemEval* eval = m_owner->m_systems.FindEval(m_system);
      if (eval)
      {
        int limit = eval->Stat(0x84);
        if (m_limit < limit)
          limit = m_limit;
        if (limit < m_count)
          m_count = limit;
        if (m_cap < m_count)
          m_count = m_cap;
        result = 8;
      }
    }
  }
  else if (need < 0)
  {
    result = 5;
  }
  if (m_count == 0)
    result = 6;
  return result;
}

OrderBase* AiTask1::BuildCapitalOrder()
{
  return aitask::BuildCapitalOrder(Fields(), kParams, FacilityKind::kManufacturing2A);
}

int AiTask1::PickShieldTarget()
{
  ObjectKey scratch;
  m_selector.Add(m_owner->RankSectors(0x100, 0, 0, 0, 0, 0, 7, 1));
  m_selector.Add(m_owner->RankSectors(0x100, 0, 0, 0, 0, 0, 10, 1));
  m_selector.Add(m_owner->RankSectors(0x100, 0, 0, 0, 0, 0, 9, 1));
  m_system = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0x80, 0, 0, 0, 0, 0, 0xe, 1));
  m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0x80, 0, 0, 0, 0, 0, 0, 1));
  m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0x80, 0, 0, 0, 0, 0, 0xf, 1));
  m_system = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  if (!IsSystemKey(m_system))
  {
    m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x800, 0, 0, 0, 0, 0, 2));
    m_system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSectorByDistance(&m_system, &m_manager, 0x40, 0, 0, 0, 0, 0, 2));
    m_system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
  }
  return IsSystemKey(m_system) ? 1 : 0;
}

int AiTask1::PickIonTarget()
{
  ObjectKey scratch;
  m_selector.Add(m_owner->RankSectors(0x200, 0, 0, 0, 0, 0, 7, 1));
  m_selector.Add(m_owner->RankSectors(0x200, 0, 0, 0, 0, 0, 9, 1));
  m_system = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0x100, 0, 1, 0x3e00003, 0, 0, 0xf, 1));
  m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0x100, 0, 1, 0x3e00003, 0, 0, 0, 1));
  m_system = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  if (!IsSystemKey(m_system))
  {
    m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x400, 0, 0, 0, 0, 0, 2));
    m_system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSectorByDistance(&m_system, &m_manager, 0x20, 0, 0, 0, 0, 0, 2));
    m_system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
  }
  return IsSystemKey(m_system) ? 1 : 0;
}

OrderBase* AiTask1::BuildAllScrapOrder()
{
  ObjectKeyValueSet team;
  OrderBase* order = nullptr;
  ObjectKeyValueNode* last = nullptr;
  if (IsSystemKey(m_system))
  {
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_system);
    if (eval && (eval->m_flags28.m_value & 0x3e00000) == 0)
    {
      GameObjNotifier* system = GetObjectInFamilies_90_98(m_view, &m_system);
      if (system)
      {
        last = AddAllOfKind<ManufacturingFacility28Iterator>(&team, system, last);
        last = AddAllOfKind<ManufacturingFacility29Iterator>(&team, system, last);
        last = AddAllOfKind<ManufacturingFacility2AIterator>(&team, system, last);
      }
      if (team.First())
        order = NewTeamOrder(kScrapOrder, m_view, &team);
    }
  }
  return order;
}

OrderBase* AiTask1::BuildDestinationOrder()
{
  return aitask::BuildDestinationOrder(Fields(), &m_system, 0xa0);
}

OrderBase* AiTask1::BuildManufactureOrder210()
{
  if ((m_classKey.m_value & 0xff000000) == 0)
    return nullptr;
  return aitask::BuildManufactureOrder(Fields(), 0x210, 0xa0);
}

void AiTask1::AddTargetSystem()
{
  aitask::AddTargetSystem(Fields(), kParams);
}

int AiTask1::NeedForTargets()
{
  return aitask::NeedForTargets(Fields(), kParams);
}

void AiTask1::RefreshTargets()
{
  aitask::RefreshTargets(Fields(), kParams);
}
