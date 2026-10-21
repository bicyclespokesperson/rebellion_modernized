#include "ai/ai_task_base.h"

#include "ai/ai_agent_bases.h"
#include "ai/ai_common.h"
#include "ai/ai_planner.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"
#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "core/stream_io.h"
#include "gameobj/factory_registries.h"

using ai::KeyInFamilies;

namespace
{

bool IsSectorKey(const ObjectKey& key)
{
  return KeyInFamilies(key, 0x80, 0x90);
}
bool IsSystemKey(const ObjectKey& key)
{
  return KeyInFamilies(key, 0x90, 0x98);
}

} // namespace

// ---- AiTaskNode / AiTaskBase
// ------------------------------------------------------------------------------------------

AiTaskNode::AiTaskNode(int id)
    : IdNode()
    , m_flags(0)
{
  SetId(id);
  m_flags.m_value = 0;
}

AiTaskNode::~AiTaskNode() = default;

void AiTaskNode::Read(istream* in)
{
  ReadId(in);
  ReadValuePersistentInt(in, &m_flags);
}

void AiTaskNode::Write(ostream* out)
{
  WriteId(out);
  WriteValuePersistentInt(out, &m_flags);
}

AiTaskBase::AiTaskBase(int id, int view)
    : AiTaskNode(id)
    , m_flags28(0)
{
  m_finished = 0;
  m_view = view;
  m_priority = 0;
  m_x24 = 0;
  m_owner = nullptr;
}

AiTaskBase::~AiTaskBase()
{
  m_owner = nullptr;
}

void AiTaskBase::Read(istream* in)
{
  AiTaskNode::Read(in);
  ReadValue32(in, &m_view);
  ReadValue32(in, &m_finished);
  ReadValue32(in, &m_priority);
  ReadValue32(in, &m_x24);
  m_flags28.Read(in);
}

void AiTaskBase::Write(ostream* out)
{
  AiTaskNode::Write(out);
  WriteValue32(out, &m_view);
  WriteValue32(out, &m_finished);
  WriteValue32(out, &m_priority);
  WriteValue32(out, &m_x24);
  m_flags28.Write(out);
}

// ---- AiNoticeGroup
// -----------------------------------------------------------------------------------------------------

AiNoticeGroup::AiNoticeGroup()
    : IdNode()
    , m_finished(0)
{
}

AiNoticeGroup::~AiNoticeGroup() = default;

void AiNoticeGroup::Clear()
{
  decltype(m_notices) detached;
  m_notices.swap(detached);
}

void AiNoticeGroup::Load(istream* in)
{
  Clear();
  ReadId(in);
  uint32_t count = 0;
  ReadValue32(in, &count);
  ReadValue32(in, &m_finished);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0) {
    return;
  }
  for (uint32_t index = 0; index < count; ++index) {
    int kind = 0;
    ReadValue32(in, &kind);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0) {
      return;
    }
    std::unique_ptr<NotifPayload> notice(static_cast<NotifPayload*>(CreateKeyedObject(kind)));
    if (!notice) {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    notice->Read(in);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0) {
      return;
    }
    Add(std::move(notice));
  }
}

void AiNoticeGroup::Save(ostream* out)
{
  WriteId(out);
  if (!WriteCount32(out, Count())) {
    return;
  }
  WriteValue32(out, &m_finished);
  for (const auto& entry : Entries()) {
    auto* notice = entry.get();
    int kind = notice->GetKind();
    WriteValue32(out, &kind);
    notice->Write(out);
  }
}

void AiNoticeGroup::Add(std::unique_ptr<NotifPayload> notice)
{
  if (!notice) {
    return;
  }
  if (m_notices.empty() && m_id == 0) {
    SetId(notice->m_value0c);
  }
  if (notice->GetKind() == 2) {
    m_finished = 1;
  }
  m_notices.emplace_back(std::move(notice));
}

// ---- AiTaskObject / AiTaskObjectSet
// ------------------------------------------------------------------------------------

AiTaskObject::AiTaskObject()
    : IdNode()
    , m_key()
{
  m_first = 0;
}

AiTaskObject::AiTaskObject(const ObjectKey& key, int first, int second)
    : IdNode()
    , m_key(key)
{
  m_first = first;
  m_second = second;
}

AiTaskObject::~AiTaskObject() = default;

void AiTaskObject::Read(istream* in)
{
  ReadId(in);
  m_key.Read(in);
  ReadValue32(in, &m_first);
  ReadValue32(in, &m_second);
}

void AiTaskObject::Write(ostream* out)
{
  WriteId(out);
  m_key.Write(out);
  WriteValue32(out, &m_first);
  WriteValue32(out, &m_second);
}

AiTaskObjectSet::AiTaskObjectSet() {}

AiTaskObjectSet::~AiTaskObjectSet()
{
  Clear();
}

void AiTaskObjectSet::Clear()
{
  m_objects.Clear();
}

uint32_t AiTaskObjectSet::AddWithId(AiTaskObject* object)
{
  return m_objects.AddWithId(object);
}

AiTaskObject* AiTaskObjectSet::DetachById(uint32_t id)
{
  return m_objects.DetachById(id);
}

void AiTaskObjectSet::Load(istream* in)
{
  Clear();
  uint32_t count = 0;
  ReadValue32(in, &count);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0) {
    return;
  }
  for (uint32_t index = 0; index < count; ++index) {
    auto* object = NewOrNull<AiTaskObject>();
    if (!object) {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    object->Read(in);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || AddWithId(object) == 0) {
      delete object;
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
  }
}

void AiTaskObjectSet::Save(ostream* out)
{
  if (!WriteCount32(out, Count())) {
    return;
  }
  for (const auto& entry : Entries()) {
    entry.second->Write(out);
  }
}

AiTaskObject* AiTaskObjectSet::FindByKey(const ObjectKey* key)
{
  for (const auto& entry : Entries()) {
    AiTaskObject* object = entry.second.get();
    if (object->m_key.m_value == key->m_value) {
      return object;
    }
  }
  return nullptr;
}

// ---- AiTaskEvaluator
// ---------------------------------------------------------------------------------------------------

AiTaskEvaluator::AiTaskEvaluator(int view, AiSideController* owner)
    : m_flags(0)
    , m_selector()
{
  m_owner = owner;
  m_view = view;
  m_state = 0;
  m_x18 = 0;
  m_x14 = 0;
}

AiTaskEvaluator::~AiTaskEvaluator()
{
  m_owner = nullptr;
}

void AiTaskEvaluator::Read(istream* in)
{
  ReadValue32(in, &m_view);
  ReadValue32(in, &m_state);
  m_flags.Read(in);
  ReadValue32(in, &m_x14);
  ReadValue32(in, &m_x18);
  m_selector.Read(in);
}

void AiTaskEvaluator::Write(ostream* out)
{
  WriteValue32(out, &m_view);
  WriteValue32(out, &m_state);
  m_flags.Write(out);
  WriteValue32(out, &m_x14);
  WriteValue32(out, &m_x18);
  m_selector.Write(out);
}

int AiTaskEvaluator::Run()
{
  int finished = 0;
  switch (m_state) {
    case 1:
      PhasePrepare();
      m_state = 2;
      return 0;
    case 2:
      PhaseUpdateTiers();
      m_state = 3;
      return 0;
    case 3:
      PhasePickPlanner();
      m_state = 4;
      return 0;
    case 4:
      PhasePickAgent();
      m_state = 5;
      return 0;
    case 5:
      Finish();
      m_state = 6;
      return 0;
    case 6:
      PhaseFinish();
      m_state = 0;
      finished = 1;
  }
  m_state = 1;
  return finished;
}

void AiTaskEvaluator::ClearRecordFlags()
{
  for (const auto& entry : m_owner->m_systemRecords.Entries()) {
    static_cast<AiSystemRecord*>(entry.get())->m_flagsA.m_value &= 0xf7ffffff;
  }
  m_flags.m_value = 0;
}

void AiTaskEvaluator::PhasePrepare()
{
  if ((m_owner->m_flags.Get() & 2) == 0) {
    return;
  }
  m_flags.m_value &= ~1;
  Prepare();
  m_owner->m_flags.m_value &= ~2;
}

void AiTaskEvaluator::PhaseUpdateTiers()
{
  bool allReady = true;
  bool anyBuilding = false;
  bool noMarked = false;
  bool lastBuilding = false;
  int buildingCount = 0;
  m_x14 = 0;
  m_x18 = 0;
  for (const auto& entry : m_objects->Entries()) {
    AiTaskObject* object = entry.second.get();
    auto* sector = static_cast<AiSectorEval*>(m_owner->m_sectors.Find(&object->m_key));
    if (!sector) {
      continue;
    }
    auto* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(sector->m_x38));
    if (record) {
      record->SetTier(object->m_id);
      if ((record->m_flagsA.m_value & 0x8000000) != 0 && (record->m_flagsA.m_value & 0x4000000) != 0) {
        continue;
      }
    }
    allReady = false;
  }
  if (!allReady) {
    return;
  }
  for (const auto& entry : m_objects->Entries()) {
    AiTaskObject* object = entry.second.get();
    auto* sector = static_cast<AiSectorEval*>(m_owner->m_sectors.Find(&object->m_key));
    if (!sector) {
      continue;
    }
    auto* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(sector->m_x38));
    if (!record) {
      continue;
    }
    const uint32_t flags = record->m_flagsA.m_value;
    if ((flags & 0x80000) == 0) {
      noMarked = true;
    }
    lastBuilding = (flags & 0x10000) != 0;
    if (lastBuilding) {
      anyBuilding = true;
      ++buildingCount;
    }
    if ((flags & 0x20000) == 0) {
      m_owner->m_flags.m_value |= 2;
    }
    record->m_flagsA.m_value &= 0xf7ffffff;
  }
  if (anyBuilding) {
    if (lastBuilding && 1 < buildingCount) {
      m_owner->m_flags.m_value |= 2;
      m_x18 = 1;
    }
  } else if (!noMarked && (m_flags.m_value & 1) == 0) {
    m_owner->m_flags.m_value |= 2;
    m_x14 = 1;
  }
}

void AiTaskEvaluator::PhasePickPlanner()
{
  int missionKind = 0;
  int missionSubtype = 0;
  int plannerKind = 0;
  int plannerSubtype = 0;
  int plannerId = 0;
  GetPlannerRequest(&missionKind, &missionSubtype, &plannerKind, &plannerSubtype);
  if (missionKind != 0) {
    m_owner->m_missionKind = missionKind;
    m_owner->m_missionSubtype = missionSubtype;
  }
  if (plannerKind == 0) {
    return;
  }
  int bestPriority = 2000000;
  for (const auto& entry : m_owner->m_planners.Entries()) {
    auto* node = entry.get();
    auto* planner = static_cast<AiMissionPlanner*>(node);
    if (static_cast<int>(planner->m_missionKey.Family()) == plannerKind && planner->GetSubtype() == plannerSubtype
        && (planner->m_flags.m_value & 0x10000000) == 0 && planner->GetPriority() < bestPriority) {
      plannerId = planner->m_id;
      bestPriority = planner->GetPriority();
    }
  }
  m_owner->m_missionTarget = plannerId;
}

void AiTaskEvaluator::PhasePickAgent()
{
  int agentTarget = 0;
  int wantAgent = 0;
  int agentId = 0;
  GetAgentRequest(&agentTarget, &wantAgent);
  if (agentTarget != 0) {
    m_owner->m_agentTarget = agentTarget;
  }
  if (wantAgent == 0) {
    return;
  }
  for (auto position = m_owner->m_agents.Entries().begin();
       position != m_owner->m_agents.Entries().end() && (agentId == 0); ++position) {
    auto* node = (*position).get();
    auto* agent = static_cast<AiAgentBase*>(node);
    if (agent->GetAgentClass() == 2 && (agent->m_flags.m_value & 2) == 0) {
      agentId = agent->m_id;
    }
  }
  m_owner->m_agentSystem = agentId;
}

void AiTaskEvaluator::PhaseFinish()
{
  ObjectKey scratch;
  ObjectKey picked;
  ObjectKey specialKey;
  specialKey.m_value = 0x90000109;
  AiSystemEval* special = m_owner->m_systems.FindEval(specialKey);
  if (special) {
    if ((special->m_flags30.m_value & 1) == 0) {
      special->m_flags30.m_value &= 0xefffffff;
    } else {
      special->m_flags30.m_value |= 0x10000000;
    }
  }
  if (m_view != 1) {
    return;
  }
  if (IsSystemKey(m_owner->m_key28)) {
    AiSystemEval* system = m_owner->m_systems.FindEval(m_owner->m_key28);
    if (system) {
      const uint32_t flags = system->m_flags30.m_value;
      if ((flags & 1) == 0 || (flags & 0x80000000) == 0) {
        system->m_flags30.m_value &= 0xefffffff;
        m_owner->m_key28.Reset();
      } else {
        system->m_flags30.m_value |= 0x10000000;
      }
    }
  }
  if (IsSystemKey(m_owner->m_key28)) {
    return;
  }
  m_selector.Add(m_owner->RankSectors(0, 0, 0x80000000, 0, 0, 0, 3, 1));
  picked = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(m_owner->RankSystemsOfSector(&picked, 0, 0, 0x80000000, 0, 0, 0, 0, 1));
  picked = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  if (!IsSystemKey(picked)) {
    return;
  }
  AiSystemEval* system = m_owner->m_systems.FindEval(picked);
  if (!system) {
    return;
  }
  if ((system->m_flags30.m_value & 1) == 0) {
    system->m_flags30.m_value &= 0xefffffff;
  } else {
    system->m_flags30.m_value |= 0x10000000;
    m_owner->m_key28 = picked;
  }
}

int AiTaskEvaluator::ChooseSector(int useAge, ObjectKey* sector)
{
  int kind = 0;
  ObjectKey origin;
  ObjectKey system;
  ObjectKey scratch;
  uint32_t systemStat = 0;
  uint32_t sectorStat = 0;
  AiRankedSelector systemSelector;
  const uint32_t bits = m_owner->m_bitsA;
  if (bits == 0) {
    return kind;
  }
  uint32_t bit = 0x80;
  while (!IsSectorKey(*sector) && bit != 0) {
    const uint32_t hit = bit & bits;
    bit >>= 1;
    if (hit == 0) {
      continue;
    }
    switch (hit) {
      case 1:
      case 0x40:
      case 0x80:
        systemStat = 0x2d;
        sectorStat = 6;
        kind = 3;
        break;
      case 2:
      case 8:
      case 0x10:
        systemStat = 0x2b;
        sectorStat = 4;
        kind = 1;
        break;
      case 4:
      case 0x20:
        systemStat = 0x2c;
        sectorStat = 5;
        kind = 2;
    }
    AiTaskObject* firstObject = m_objects->First();
    if (firstObject) {
      origin = firstObject->m_key;
    }
    m_selector.Add(m_owner->RankSectors(0, 0, 2, 0, 0, 0, 0xc, 2));
    if (IsSectorKey(origin)) {
      m_selector.Add(m_owner->RankSectorsByDistance(&origin, 0, 0, 2, 0, 0, 0, 2));
    }
    if (useAge != 0) {
      m_selector.Add(m_owner->RankSectorsByX3c(0, 0, 2, 0, 0, 0, 2));
      m_selector.Add(m_owner->RankSectorsByX3c(0, 0, 2, 0, 0, 0, 2));
    }
    *sector = *m_selector.PickBest(&scratch);
    system.Reset();
    while (IsSectorKey(*sector) && !IsSystemKey(system)) {
      auto* eval = static_cast<AiSectorEval*>(m_owner->m_sectors.Find(sector));
      if (eval && 0 < eval->m_stat[systemStat & 0xffff]) {
        systemSelector.Add(m_owner->RankSystemsOfSector(sector, 0, 0, 0x20, 0, 0, 0x101, sectorStat, 2));
        systemSelector.Add(m_owner->RankSystemsOfSector(sector, 0, 0, 0x20, 0, 0, 0x101, 0x14, 2));
        system = *systemSelector.PickBest(&scratch);
        bool found = false;
        while (IsSystemKey(system) && !found) {
          AiSystemEval* candidate = m_owner->m_systems.FindEval(system);
          if (!candidate || candidate->m_stat[sectorStat & 0xffff] < 1) {
            system = *systemSelector.PickBest(&scratch);
          } else {
            found = true;
          }
        }
        systemSelector.Reset();
      }
      if (!IsSystemKey(system)) {
        *sector = *m_selector.PickBest(&scratch);
      }
    }
    m_selector.Reset();
  }
  return kind;
}
