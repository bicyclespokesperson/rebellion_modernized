#include "ai/ai_common.h"
#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"
#include "ai/ai_task_types.h"
#include "orders/order_base.h"

using ai::KeyInFamilies;

namespace
{

constexpr int kSectorRecordClaimStat = 0x18c;
constexpr uint32_t kRecordHasNoDemandFlag = 0x4000000;

bool IsSectorKey(const ObjectKey& key)
{
  return KeyInFamilies(key, 0x80, 0x90);
}

} // namespace

// ---- type 8 -------------------------------------------------------------------------------------------------------------

AiTask8::AiTask8(int view) : AiStagedTask(8, view), m_selector()
{
  m_recordId = 0;
  m_recordCursor = 0;
}

AiTask8::~AiTask8() = default;

void AiTask8::Read(istream* in)
{
  AiTaskBase::Read(in);
  ReadValue32(in, &m_stage);
  ReadValue32(in, &m_step);
  ReadValue32(in, &m_recordId);
  ReadValue32(in, &m_recordCursor);
  m_selector.Read(in);
}

void AiTask8::Write(ostream* out)
{
  AiTaskBase::Write(out);
  WriteValue32(out, &m_stage);
  WriteValue32(out, &m_step);
  WriteValue32(out, &m_recordId);
  WriteValue32(out, &m_recordCursor);
  m_selector.Write(out);
}

OrderBase* AiTask8::Step()
{
  return m_flags.Get() == 1 ? StepRun() : nullptr;
}

OrderBase* AiTask8::StepRun()
{
  int done = 1;
  switch (m_step)
  {
    case 1:
      PrepareRecords();
      m_step = 2;
      return nullptr;
    case 2:
      if (NextRecord() != 0)
      {
        m_step = 3;
        return nullptr;
      }
      break;
    case 3:
      if (ChooseSector() != 0)
      {
        m_step = 4;
        return nullptr;
      }
      break;
    case 4:
    {
      OrderBase* order = RunRecord(&done);
      if (order)
      {
        if (done == 0)
          ++m_priority;
        m_finished = 1;
      }
      if (done == 0)
        return order;
      m_step = 0;
      m_finished = 1;
      return order;
    }
    default:
      m_step = 1;
      return nullptr;
  }
  m_step = 0;
  m_finished = 1;
  return nullptr;
}

void AiTask8::PrepareRecords()
{
  bool anyWithDemand = false;
  AiSideController* owner = m_owner;
  if (owner->Stat(0x204) < 0)
  {
    for (AvlNode* node = owner->m_systemRecords.First(); node; node = node->m_successor)
    {
      if ((static_cast<AiSystemRecord*>(node)->m_flagsA.m_value & kRecordHasNoDemandFlag) == 0)
        anyWithDemand = true;
    }
    if (!anyWithDemand)
      m_owner->m_systemRecords.CreateRecord();
  }
  else if (0 < owner->Stat(0x204))
  {
    owner->m_systemRecords.DeleteFirstIdle();
  }
}

int AiTask8::NextRecord()
{
  int found = 0;
  AiSystemRecord* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(m_recordCursor));
  if (!record)
  {
    record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.First());
    if (!record)
    {
      m_recordId = 0;
      m_recordCursor = 0;
      return found;
    }
  }
  found = 1;
  m_recordId = record->m_id;
  AiSystemRecord* next = static_cast<AiSystemRecord*>(record->m_successor);
  m_recordCursor = next ? next->m_id : 0;
  return found;
}

int AiTask8::ChooseSector()
{
  int result = 0;
  ObjectKey picked;
  ObjectKey scratch;
  AiSideController* owner = m_owner;
  AiSystemRecord* record = static_cast<AiSystemRecord*>(owner->m_systemRecords.FindById(m_recordId));
  if (!record)
    return result;

  if (IsSectorKey(record->m_sectorKey))
  {
    AiSectorEval* eval = owner->m_sectors.FindEval(record->m_sectorKey);
    if (eval && eval->Stat(kSectorRecordClaimStat) == 0 && eval->m_x38 == static_cast<int>(record->m_id))
      result = 1;
    if (result == 0)
    {
      m_selector.Add(owner->RankSectorsByDistance(&record->m_sectorKey, 0, 0, 0x10000, 0, 0, 0, 2));
      picked = *m_selector.PickBest(&scratch);
      m_selector.Reset();
      record->Reset();
      if (IsSectorKey(picked))
      {
        record->SetSector(&picked);
        result = 1;
      }
    }
  }
  if (result == 0)
  {
    m_selector.Add(owner->RankSectorsRandomly(0, 0, 0x8010000, 0, 0, 0, 2));
    picked = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (IsSectorKey(picked))
    {
      record->SetSector(&picked);
      result = 1;
    }
    if (result == 0)
    {
      m_selector.Add(owner->RankSectorsRandomly(0, 0, 0x10000, 0, 0, 0, 2));
      picked = *m_selector.PickBest(&scratch);
      m_selector.Reset();
      if (IsSectorKey(picked))
      {
        record->SetSector(&picked);
        result = 1;
      }
      if (result == 0)
        return 0;
    }
  }
  return record->UpdatePosture();
}

OrderBase* AiTask8::RunRecord(int* done)
{
  OrderBase* order = nullptr;
  *done = 1;
  AiSystemRecord* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(m_recordId));
  if (record)
    order = record->Step(done);
  return order;
}

// ---- type 9 -------------------------------------------------------------------------------------------------------------

AiTask9::AiTask9(int view) : AiStagedTask(9, view), m_selector()
{
  m_recordId = 0;
  m_recordCursor = 0;
}

AiTask9::~AiTask9() = default;

void AiTask9::Read(istream* in)
{
  AiTaskBase::Read(in);
  ReadValue32(in, &m_stage);
  ReadValue32(in, &m_step);
  ReadValue32(in, &m_recordId);
  ReadValue32(in, &m_recordCursor);
  m_selector.Read(in);
}

void AiTask9::Write(ostream* out)
{
  AiTaskBase::Write(out);
  WriteValue32(out, &m_stage);
  WriteValue32(out, &m_step);
  WriteValue32(out, &m_recordId);
  WriteValue32(out, &m_recordCursor);
  m_selector.Write(out);
}

OrderBase* AiTask9::Step()
{
  return m_flags.Get() == 1 ? StepRun() : nullptr;
}

OrderBase* AiTask9::StepRun()
{
  OrderBase* order = nullptr;
  int done = 1;
  if (m_step == 2)
  {
    if (NextRecord() != 0)
    {
      m_step = 3;
      return nullptr;
    }
    m_step = 0;
    m_finished = 1;
    return nullptr;
  }
  if (m_step != 3)
  {
    m_step = 2;
    return nullptr;
  }
  order = RunRecord(&done);
  if (order)
  {
    if (done == 0)
      ++m_priority;
    m_finished = 1;
  }
  if (done != 0)
  {
    m_step = 0;
    m_finished = 1;
  }
  return order;
}

int AiTask9::NextRecord()
{
  int found = 0;
  AiTaskRecord* record = static_cast<AiTaskRecord*>(m_owner->m_tasks.FindById(m_recordCursor));
  if (!record)
  {
    record = static_cast<AiTaskRecord*>(m_owner->m_tasks.First());
    if (!record)
    {
      m_recordId = 0;
      m_recordCursor = 0;
      return found;
    }
  }
  found = 1;
  m_recordId = record->m_id;
  AiTaskRecord* next = static_cast<AiTaskRecord*>(record->m_successor);
  m_recordCursor = next ? next->m_id : 0;
  return found;
}

OrderBase* AiTask9::RunRecord(int* done)
{
  OrderBase* order = nullptr;
  *done = 1;
  AiTaskRecord* record = static_cast<AiTaskRecord*>(m_owner->m_tasks.FindById(m_recordId));
  if (record)
    order = record->Step(done);
  return order;
}
