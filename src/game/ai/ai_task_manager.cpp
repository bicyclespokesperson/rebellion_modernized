#include "ai/ai_task_manager.h"

#include "ai/ai_order_util.h"
#include "ai/ai_task_types.h"
#include "core/checked_new.h"
#include "core/stream_io.h"
#include "events/game_clock_hooks.h"
#include "iostream/iostream.h"

#include <limits>
#include <memory>

// ---- AiMissionHistoryEntry ----------------------------------------------------------------------------------------------

AiMissionHistoryEntry::AiMissionHistoryEntry() : AvlNode(), m_time(), m_key()
{
  m_id = 0;
  m_x1c = 0;
  m_x20 = 0;
}

AiMissionHistoryEntry::~AiMissionHistoryEntry() = default;

void AiMissionHistoryEntry::Read(istream* in)
{
  ReadValue32(in, &m_id);
  ReadValue32(in, &m_x1c);
  ReadValue32(in, &m_x20);
  ReadGameTime(&m_time, in);
  m_key.Read(in);
}

void AiMissionHistoryEntry::Write(ostream* out)
{
  WriteValue32(out, &m_id);
  WriteValue32(out, &m_x1c);
  WriteValue32(out, &m_x20);
  StreamWrite32(out, &m_time);
  m_key.Write(out);
}

int AiMissionHistoryEntry::Fill(const void* mission)
{
  const uint8_t* raw = static_cast<const uint8_t*>(mission);
  m_key = *reinterpret_cast<const ObjectKey*>(raw + 0x2c);
  const bool againstSystem = ai::IsSystemKey(m_key);
  m_id = *reinterpret_cast<const uint32_t*>(raw + 0x20);
  const uint32_t id = m_id;
  const int kind = *reinterpret_cast<const int*>(raw + 0x24);
  m_x1c = kind;
  if (kind == 1 || kind == 2)
    m_x20 = 1;
  else if (kind == 3)
    m_x20 = 2;
  else
    m_x20 = 0;
  AssignGameTime(&m_time, reinterpret_cast<const GameTime*>(raw + 0x28));
  return id != 0 && againstSystem;
}

// ---- AiMissionHistory ---------------------------------------------------------------------------------------------------

AiMissionHistory::AiMissionHistory() : AvlTree()
{
}

AiMissionHistory::~AiMissionHistory()
{
  Clear();
}

int AiMissionHistory::IsLess(AvlNode* a, AvlNode* b)
{
  if (a && b && static_cast<AiMissionHistoryEntry*>(a)->m_id < static_cast<AiMissionHistoryEntry*>(b)->m_id)
    return 1;
  return 0;
}

void AiMissionHistory::Load(istream* in)
{
  Clear();
  uint32_t count = 0;
  ReadValue32(in, &count);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return;
  for (uint32_t index = 0; index < count; ++index)
  {
    AiMissionHistoryEntry* entry = NewOrNull<AiMissionHistoryEntry>();
    if (!entry)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    entry->Read(in);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    {
      delete entry;
      return;
    }
    Insert(entry);
  }
}

void AiMissionHistory::Save(ostream* out)
{
  if (Count() > std::numeric_limits<uint32_t>::max())
  {
    iostream_access::SetState(iostream_access::IosOf(out), iostream_flags::kFail);
    return;
  }
  uint32_t count = static_cast<uint32_t>(Count());
  WriteValue32(out, &count);
  for (const auto& node : Entries())
    static_cast<AiMissionHistoryEntry*>(node.get())->Write(out);
}

int AiMissionHistory::Add(const void* mission)
{
  if (!mission)
    return 0;
  AiMissionHistoryEntry* entry = NewOrNull<AiMissionHistoryEntry>();
  if (!entry)
    return 0;
  if (!entry->Fill(mission))
  {
    delete entry;
    return 0;
  }
  Insert(entry);
  return 1;
}

AiMissionHistoryEntry* AiMissionHistory::Find(const int* key, uint32_t id, int x20)
{
  bool found = false;
  auto position = Entries().begin();
  while (position != Entries().end() && !found)
  {
    AvlNode* node = position->get();
    AiMissionHistoryEntry* entry = static_cast<AiMissionHistoryEntry*>(node);
    found = entry->m_id == id && entry->m_key.m_value == static_cast<uint32_t>(*key) && entry->m_x20 == x20;
    if (found)
      return entry;
    if (id < entry->m_id)
      break;
    ++position;
  }
  return nullptr;
}

// ---- AiTaskManager ------------------------------------------------------------------------------------------------------

AiTaskManager::AiTaskManager() : IdAvlTree()
{
  m_view = 0;
  m_current = nullptr;
  m_cursor = nullptr;
  m_last = nullptr;
}

AiTaskManager::~AiTaskManager()
{
  m_current = nullptr;
  m_cursor = nullptr;
  m_last = nullptr;
}

void AiTaskManager::Load(istream* in, AiSideController* owner)
{
  Clear();
  ReadValue32(in, &m_view);
  int16_t count;
  ReadValue16(in, &count);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || count < 0)
  {
    iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
    return;
  }
  while (count != 0)
  {
    int type = 0;
    ReadValue32(in, &type);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
      return;
    AiTaskBase* task = MakeTask(type);
    if (!task)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    task->SetOwner(owner);
    task->Read(in);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || AddWithId(task) == 0)
    {
      delete task;
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    --count;
  }
  uint32_t id;
  ReadValue32(in, &id);
  m_current = static_cast<AiTaskBase*>(FindById(id));
  ReadValue32(in, &id);
  m_cursor = static_cast<AiTaskBase*>(FindById(id));
  ReadValue32(in, &id);
  m_last = static_cast<AiTaskBase*>(FindById(id));
}

void AiTaskManager::Save(ostream* out)
{
  WriteValue32(out, &m_view);
  int16_t count = Count();
  WriteValue16(out, &count);
  for (const auto& entry : Entries())
  {
    AvlNode* node = entry.get();
    AiTaskBase* task = static_cast<AiTaskBase*>(node);
    uint32_t id = task->m_id;
    WriteValue32(out, &id);
    task->Write(out);
  }
  uint32_t id = m_current ? m_current->m_id : 0;
  WriteValue32(out, &id);
  id = m_cursor ? m_cursor->m_id : 0;
  WriteValue32(out, &id);
  id = m_last ? m_last->m_id : 0;
  WriteValue32(out, &id);
}

uint32_t AiTaskManager::CreateTask(int type, AiSideController* owner)
{
  uint32_t id = 0;
  if (FindById(type))
    return id;
  AiTaskBase* task = MakeTask(type);
  if (task)
  {
    task->SetOwner(owner);
    task->m_flags.m_value = 0;
    id = AddWithId(task);
  }
  return id;
}

AiTaskBase* AiTaskManager::SelectNext()
{
  m_last = PickBest();
  if (m_last != nullptr)
    return m_last;
  auto position = PositionOf(m_current);
  if (position == Entries().end() || ++position == Entries().end())
    position = Entries().begin();
  m_current = position == Entries().end() ? nullptr : static_cast<AiTaskBase*>(position->get());
  AiTaskBase* start = m_current;
  bool wrapped = false;
  int wants = m_current->m_flags.Get();
  while (wants != 1)
  {
    if (wrapped)
    {
      m_last = nullptr;
      return m_last;
    }
    if (++position == Entries().end())
      position = Entries().begin();
    m_current = position == Entries().end() ? nullptr : static_cast<AiTaskBase*>(position->get());
    wrapped = start == m_current;
    wants = m_current->m_flags.Get();
  }
  if (!wrapped)
  {
    m_last = m_current;
    return m_current;
  }
  m_last = nullptr;
  return m_last;
}

AiTaskBase* AiTaskManager::MakeTask(int type)
{
  switch (type)
  {
    case 1:
      return NewOrNull<AiTask1>(m_view);
    case 2:
      return NewOrNull<AiTask2>(m_view);
    case 3:
      return NewOrNull<AiTask3>(m_view);
    case 4:
      return NewOrNull<AiTask4>(m_view);
    case 5:
      return NewOrNull<AiTask5>(m_view);
    case 6:
      return NewOrNull<AiTask6>(m_view);
    case 7:
      return NewOrNull<AiTask7>(m_view);
    case 8:
      return NewOrNull<AiTask8>(m_view);
    case 9:
      return NewOrNull<AiTask9>(m_view);
    case 10:
      return NewOrNull<AiTask10>(m_view);
    case 11:
      return NewOrNull<AiTask11>(m_view);
    case 12:
      return NewOrNull<AiTask12>(m_view);
    case 13:
      return NewOrNull<AiTask13>(m_view);
    case 14:
      return NewOrNull<AiTask14>(m_view);
  }
  return nullptr;
}

AiTaskBase* AiTaskManager::PickBest()
{
  AiTaskBase* best = nullptr;
  uint32_t bestPriority = 0;
  auto position = PositionOf(m_cursor);
  if (position == Entries().end() || ++position == Entries().end())
    position = Entries().begin();
  m_cursor = position == Entries().end() ? nullptr : static_cast<AiTaskBase*>(position->get());
  AiTaskBase* task = m_cursor;
  while (task)
  {
    if (task->m_flags.Get() == 1 && bestPriority < static_cast<uint32_t>(task->m_priority))
    {
      bestPriority = task->m_priority;
      best = task;
    }
    if (++position == Entries().end())
      position = Entries().begin();
    task = position == Entries().end() ? nullptr : static_cast<AiTaskBase*>(position->get());
    if (task == m_cursor)
      task = nullptr;
  }
  return best;
}

// ---- AiNoticeQueue ------------------------------------------------------------------------------------------------------

AiNoticeQueue::AiNoticeQueue() : IdAvlTree()
{
  m_x0c = 1;
}

AiNoticeQueue::~AiNoticeQueue() = default;

void AiNoticeQueue::Load(istream* in)
{
  Clear();
  uint32_t count = 0;
  ReadValue32(in, &count);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return;
  for (; count != 0; --count)
  {
    AiNoticeGroup* group = NewOrNull<AiNoticeGroup>();
    if (!group)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    group->Load(in);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || AddWithId(group) == 0)
    {
      delete group;
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
  }
}

void AiNoticeQueue::Save(ostream* out)
{
  if (!WriteCount32(out, Count()))
    return;
  for (const auto& entry : Entries())
    static_cast<AiNoticeGroup*>(entry.get())->Save(out);
}

void AiNoticeQueue::Add(std::unique_ptr<NotifPayload> notice)
{
  if (!notice)
    return;
  if (notice->m_value0c == 0)
  {
    return;
  }
  IdNode* node = FindById(notice->m_value0c);
  if (node)
    static_cast<AiNoticeGroup*>(node)->Add(std::move(notice));
  else
    NewGroup(std::move(notice));
}

AiNoticeGroup* AiNoticeQueue::DetachFinished()
{
  bool detached = false;
  AiNoticeGroup* group = nullptr;
  AvlNode* node = First();
  if (node)
    group = static_cast<AiNoticeGroup*>(static_cast<IdNode*>(node));
  while (group && !detached)
  {
    if (group->m_finished != 0)
    {
      DetachById(static_cast<IdNode*>(group)->m_id);
      detached = true;
    }
    else
    {
      auto position = PositionOf(group);
      ++position;
      group = position == Entries().end() ? nullptr : static_cast<AiNoticeGroup*>(position->get());
    }
  }
  return group;
}

void AiNoticeQueue::NewGroup(std::unique_ptr<NotifPayload> notice)
{
  std::unique_ptr<AiNoticeGroup> group(NewOrNull<AiNoticeGroup>());
  if (group)
  {
    group->Add(std::move(notice));
    if (AddWithId(group.get()) != 0)
      group.release();
  }
}
