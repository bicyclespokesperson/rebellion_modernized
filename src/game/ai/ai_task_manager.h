#pragma once
#include <cstdint>
#include <map>
#include <memory>

#include "ai/ai_notice_queue.h"
#include "ai/ai_task_base.h"
#include "app/player_controller_decl.h"
#include "core/id_registry.h"
#include "core/stream_io.h"
#include "gameobj/object_key.h"

// The collections an AiPlayer keeps (0x4be580-0x4bf0b0): the history of the missions it ran, its tasks and the notes it has
// been sent.

class AiSideController;

// ---- AiMissionHistory ---------------------------------------------------------------------------------------------------

// One remembered mission (VT 0x65c6b4, 0x2c bytes; lies with the task types at 0x4c8000).
class AiMissionHistoryEntry : public AvlNode
{
public:
  AiMissionHistoryEntry(); // 0x4c8000
  ~AiMissionHistoryEntry() override; // 0: 0x4c8060 (deleting), 0x4c8080
  void Read(istream* in); // 0x4c80e0
  void Write(ostream* out); // 0x4c8130
  // Fills the entry from a finished mission (an object of the mission module: id +0x20, kind +0x24, time +0x28, key +0x2c);
  // true if it names a mission against a system.
  int Fill(const void* mission); // 0x4c8180

  uint32_t m_id; // +0x18
  int m_x1c; // +0x1c
  int m_x20; // +0x20
  GameTime m_time; // +0x24
  ObjectKey m_key; // +0x28
};

// The registry of AiMissionHistoryEntry (VT 0x65c450): ordered by id.
class AiMissionHistory
{
public:
  AiMissionHistory(); // 0x4be580
  ~AiMissionHistory(); // 0: 0x4be5a0 (deleting), 0x4be5c0
  void Clear();
  size_t Count() const
  {
    return m_entries.size();
  }
  const auto& Entries() const
  {
    return m_entries;
  }
  int Insert(AiMissionHistoryEntry* entry);
  void Remove(AiMissionHistoryEntry* entry);
  void Load(istream* in); // 0x4be610
  void Save(ostream* out); // 0x4be6b0
  int Add(const void* mission); // 0x4be700: remembers a mission; 1 if it was added
  AiMissionHistoryEntry* Find(const int* key, uint32_t id, int x20); // 0x4be780

private:
  std::multimap<uint32_t, std::unique_ptr<AiMissionHistoryEntry>> m_entries;
};

// ---- AiTaskManager ------------------------------------------------------------------------------------------------------

// The AI player's tasks (VT 0x65c460, 0x1c bytes): one task of each of the 14 types, ids 1-14. AiPlayer asks them in turn.
class AiTaskManager
{
public:
  static constexpr int kTaskTypes = 14;

  AiTaskManager(); // 0x4be7e0
  ~AiTaskManager(); // 0: 0x4be800 (deleting), 0x4be820
  void Clear();
  size_t Count() const
  {
    return m_tasks.Count();
  }
  const auto& Entries() const
  {
    return m_tasks.Storage();
  }
  auto PositionOf(const AiTaskBase* task) const
  {
    const auto& entries = m_tasks.Storage();
    auto position = entries.cbegin();
    for (; position != entries.cend(); ++position)
      if (position->second.get() == task)
        return position;
    return position;
  }
  uint32_t AddWithId(AiTaskBase* task);
  AiTaskBase* FindById(uint32_t id) const;
  AiTaskBase* First() const
  {
    return m_tasks.First();
  }
  void Load(istream* in, AiSideController* owner); // 0x4be840
  void Save(ostream* out); // 0x49de40
  uint32_t
  CreateTask(int type,
             AiSideController* owner); // 0x4be930: adds the task of that type unless there is one; returns its id
  // The next task that wants to run: the one with the highest priority, else the next one in turn that wants to run.
  AiTaskBase* SelectNext(); // 0x4be980
  AiTaskBase* MakeTask(int type); // 0x4bea20: a new task of the type (ids 1-14), null otherwise
  AiTaskBase* PickBest(); // 0x4bede0: the task with the highest priority that wants to run

  int m_view; // +0x0c
  AiTaskBase* m_current; // +0x10 the task AiPlayer is running
  AiTaskBase* m_cursor; // +0x14 where PickBest continues
  AiTaskBase* m_last; // +0x18 the task SelectNext returned

private:
  IdRegistry<AiTaskBase> m_tasks;
};
