#pragma once

#include <memory>

#include "ai/ai_task_base.h"
#include "core/id_registry.h"
#include "core/stream_io.h"

// The notes the AI player was sent, in AiNoticeGroups by group id. The tree holds the IdNode part of the groups.
class AiNoticeQueue
{
public:
  AiNoticeQueue();
  ~AiNoticeQueue();
  void Clear();
  size_t Count() const
  {
    return m_groups.Count();
  }
  const auto& Entries() const
  {
    return m_groups.Storage();
  }
  auto PositionOf(const AiNoticeGroup* group) const
  {
    const auto& entries = m_groups.Storage();
    auto position = entries.cbegin();
    for (; position != entries.cend(); ++position)
      if (position->second.get() == group)
        return position;
    return position;
  }
  uint32_t AddWithId(AiNoticeGroup* group);
  AiNoticeGroup* FindById(uint32_t id) const;
  AiNoticeGroup* DetachById(uint32_t id);
  void Load(istream* in);
  void Save(ostream* out);
  void Add(std::unique_ptr<NotifPayload> notice);
  AiNoticeGroup* DetachFinished();
  void NewGroup(std::unique_ptr<NotifPayload> notice);

  int m_x0c;

private:
  IdRegistry<AiNoticeGroup> m_groups;
};
