#include "ai/ai_posture_util.h"
#include "ai/ai_postures.h"

using namespace ai_posture;
using ai::IsSectorKey;
using ai::IsSystemKey;

AiPostureKind4::AiPostureKind4(int view, AiSideController* owner, AiSystemRecord* record)
: AiPosture(view, owner, record),
  m_target()
{
}

AiPostureKind4::~AiPostureKind4() = default;

void AiPostureKind4::Read(istream* in)
{
  AiPosture::Read(in);
  m_target.Read(in);
}

void AiPostureKind4::Write(ostream* out)
{
  AiPosture::Write(out);
  m_target.Write(out);
}

int AiPostureKind4::Virtual03()
{
  int result = 0;
  AiSystemEval* target = m_owner->FindSystemEval(&m_target);
  AiSystemEval* system = m_owner->FindSystemEval(&m_record->m_systemKey);
  if (system && target)
    result = (system->Stat(0x98) - system->Stat(0x88)) + target->Stat(0x118);
  return result;
}

void AiPostureKind4::GetPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype)
{
  DefaultPlannerRequest(createKind, createSubtype, releaseKind, releaseSubtype);
}

void AiPostureKind4::GetAgentRequest(int* agentKind)
{
  AgentRequestB(agentKind);
}

void AiPostureKind4::Virtual08(PersistentInt* flagsA, PersistentInt*)
{
  flagsA->m_value |= 0x4000000;
  if (FindSector(this))
  {
    flagsA->m_value |= 0x8000000;
    AiSystemEval* target = m_owner->FindSystemEval(&m_target);
    if (target && (target->m_flags30.m_value & 1) == 0)
      flagsA->m_value |= 0x20000;
  }
}

void AiPostureKind4::Virtual09(ObjectKey* a, ObjectKey* b)
{
  static const SystemQuery queries[] = {
    ByStat(0, 0x400, 0x1000, 0, 0, 0, 0x1d, 2, 0xc00, 0x7000000),
    ByStat(0, 0x800, 0x1000, 0, 0, 0, 0x1d, 2, 0x800, 0x7000000),
    ByStat(0, 0x400, 0, 0, 0, 0, 0x1d, 2, 0xc00, 4),
    ByStat(0, 0x800, 0, 0, 0, 0, 0x1d, 2, 0x800, 4),
  };
  uint32_t kinds = 0xc00;
  uint32_t characterFlags = 4;
  a->Reset();
  b->Reset();
  if (IsSectorKey(m_record->m_sectorKey))
  {
    AiSystemEval* target = m_owner->FindSystemEval(&m_target);
    if (target && (target->m_flags2c.m_value & 0xc00) != 0)
      *a = m_target;
    if (const SystemQuery* last = ChooseSystem(this, a, queries))
    {
      kinds = last->kinds;
      characterFlags = last->characterFlags;
    }
  }
  if (IsSystemKey(*a))
    PickRandomCharacter(a, b, kinds, characterFlags);
}

void AiPostureKind4::Virtual10(ObjectKey* a, ObjectKey* b)
{
  SelectSystemByAgeOrChance(this, a, b);
}

void AiPostureKind4::Virtual11(ObjectKey* a, ObjectKey* b)
{
  SelectSystemByAge(this, a, b);
}

void AiPostureKind4::Virtual12(ObjectKey* a, ObjectKey* b)
{
  static const SystemQuery queries[] = {
    ByStat(0, 0x40000, 0x100, 0, 0, 0, 0x1d, 2, 0x40000),   ByStat(0, 0x80000, 0, 0, 0, 0, 0x1d, 2, 0x80000),
    ByStat(0, 0x100000, 0, 0, 0, 0, 0x1d, 2, 0x100000),     ByStat(0, 0x40000, 0, 0, 0, 0, 0x1d, 2, 0x40000),
    ByStat(0, 0x200000, 0, 0, 0, 0, 0x1d, 2, 0x200000),     ByStat(0, 0x400000, 0, 0, 0, 0, 0x1d, 2, 0x400000),
    ByStat(0, 0x800000, 0x200, 0, 0, 0, 0x1d, 2, 0x800000),
  };
  SelectObjectSystem(this, a, b, queries);
}

int AiPostureKind4::Virtual13()
{
  return ChooseMarkedTarget(this, 0x10000, 0xfffaffff, 0xffffffff, 0x50000);
}

OrderBase* AiPostureKind4::Virtual14()
{
  AiSystemEval* system = m_owner->m_systems.FindEval(m_record->m_systemKey);
  if (!system)
    return nullptr;
  return OrderTroopsOfSystem(this, system, nullptr);
}

int AiPostureKind4::Virtual15()
{
  int assigned = 0;
  AiFleetEval* fleet = FindTargetFleet(this);
  if (!fleet || fleet->Stat(0x58) <= 0)
    return assigned;
  AiSystemEval* target = m_owner->FindSystemEval(&m_target);
  if (target && (target->m_flags30.m_value & 0x200) != 0 && CanTakeSystem(fleet, target) != 0)
  {
    assigned = 1;
    target->m_flags30.m_value |= 0x100000;
    fleet->m_x2c = m_target;
    fleet->m_flags.m_value |= 0x10000000;
  }
  if (assigned == 0)
    assigned = AssignTakeFleet(this);
  return assigned;
}

int AiPostureKind4::Virtual16()
{
  int assigned = 0;
  AiFleetEval* fleet = m_owner->GetFleetRecord(&m_targetFleet);
  if (fleet && 0 < fleet->Stat(0x80))
  {
    AiSystemEval* target = m_owner->FindSystemEval(&m_target);
    if (target && CanUseFleet(fleet, target) != 0)
    {
      assigned = 1;
      target->m_flags30.m_value |= 0x200000;
      fleet->m_x2c = m_target;
      fleet->m_flags.m_value |= 0x20000000;
    }
  }
  return assigned;
}

void AiPostureKind4::Virtual20()
{
  ChooseTargetSystem();
  AiSystemEval* target = m_owner->FindSystemEval(&m_target);
  AiSectorEval* sector = FindSector(this);
  if (!sector)
    return;
  AiClassTable& classes = m_owner->m_classes;
  int budget = (sector->Stat(0x190) * 3) / 2;
  if (budget < classes.m_x2c)
    budget = classes.m_x2c;
  if (m_budget[0] < budget)
    m_budget[0] = budget;
  budget = (sector->Stat(0x194) * 3) / 2;
  if (target)
    budget += (target->Stat(0xb4) * 3) / 2;
  if (budget < classes.m_x30)
    budget = classes.m_x30;
  if (m_budget[1] < budget)
    m_budget[1] = budget;
  budget = ((sector->Stat(0x198) + sector->Stat(0x138)) * 11) / 10;
  if (budget < classes.m_x34)
    budget = classes.m_x34;
  if (m_budget[3] < budget)
    m_budget[3] = budget;
  if (target)
  {
    budget = target->Stat(0x118);
    if (m_budget[2] < budget)
      m_budget[2] = budget;
  }
  if ((m_owner->m_flags.Get() & 4) != 0 && 0 < sector->Stat(0x19c))
    m_budget[4] = 1;
  if ((m_owner->m_flags.Get() & 0x1000000) != 0)
    m_budget[6] = 1;
  m_budget[7] = m_record->m_x94;
  m_budget[8] = m_record->m_x98;
  m_budget[9] = 1;
}

ObjectKey* AiPostureKind4::Virtual21(ObjectKey* out)
{
  ObjectKey chosen;
  ObjectKey scratch;
  AiFleetEval* fleet = FindTargetFleet(this);
  if (fleet)
  {
    if (!IsSystemKey(chosen))
      chosen = m_record->m_systemKey;
    if (!IsSystemKey(chosen))
    {
      m_selector.Add(m_owner->RankSystemsOfSector(&m_record->m_sectorKey, 0x80000000, 0, 1, 0, 0, 0, 7, 2));
      chosen = *m_selector.PickBest(&scratch);
      m_selector.Reset();
    }
    if (!IsSystemKey(chosen))
    {
      m_selector.Add(m_owner->RankSystemsOfSector(&m_record->m_sectorKey, 0x3e00000, 0, 1, 0, 0, 0, 0x21, 1));
      chosen = *m_selector.PickBest(&scratch);
      m_selector.Reset();
    }
    if (!IsSystemKey(chosen))
    {
      AiSystemEval* origin = m_owner->m_systems.FindEval(fleet->m_systemKey);
      if (origin && (origin->m_flags30.m_value & 4) != 0)
      {
        m_selector.Add(
          m_owner->RankSystemsOfSectorByDistance(&m_record->m_sectorKey, &m_targetFleet, 0, 0, 0, 0, 0, 0x204, 2));
        chosen = *m_selector.PickBest(&scratch);
        m_selector.Reset();
      }
    }
  }
  *out = chosen;
  return out;
}

void AiPostureKind4::ChooseTargetSystem()
{
  int statIndex = 0;
  bool found = false;
  ObjectKey scratch;
  if (!IsSectorKey(m_record->m_sectorKey))
    return;
  AiSectorEval* sector = FindSector(this);
  if (!sector)
    return;
  if (sector->m_x34 == 1)
    statIndex = 4;
  else if (sector->m_x34 == 2)
    statIndex = 5;
  else if (sector->m_x34 == 3)
    statIndex = 6;
  m_selector.Add(m_owner->RankSystemsOfSector(&m_record->m_sectorKey, 0, 0, 0x20, 0, 0, 0x101, statIndex, 2));
  m_selector.Add(m_owner->RankSystemsOfSector(&m_record->m_sectorKey, 0, 0, 0x20, 0, 0, 0x101, 0x14, 2));
  m_target = *m_selector.PickBest(&scratch);
  while (IsSystemKey(m_target) && !found)
  {
    AiSystemEval* system = m_owner->m_systems.FindEval(m_target);
    if (system && 1 <= system->m_stat[statIndex])
    {
      found = true;
    }
    else
    {
      m_target = *m_selector.PickBest(&scratch);
    }
  }
  m_selector.Reset();
}
