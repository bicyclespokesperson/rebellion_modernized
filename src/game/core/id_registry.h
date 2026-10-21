#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <ranges>

template <class Entry>
class IdRegistry
{
  public:
  IdRegistry() = default;
  IdRegistry(const IdRegistry&) = delete;
  IdRegistry& operator=(const IdRegistry&) = delete;
  ~IdRegistry()
  {
    Clear();
  }

  void Clear()
  {
    if (m_clearing) {
      return;
    }
    m_clearing = true;
    decltype(m_entries) detached;
    m_entries.swap(detached);
    m_nextId = 1;
    detached.clear();
    m_clearing = false;
  }

  size_t Count() const
  {
    return m_entries.size();
  }
  bool Empty() const
  {
    return m_entries.empty();
  }
  uint32_t NextId() const
  {
    return m_nextId;
  }
  const auto& Storage() const
  {
    return m_entries;
  }
  auto Entries() const
  {
    return std::views::values(m_entries);
  }
  auto PositionOf(const Entry* entry) const
  {
    auto entries = Entries();
    if (!entry) {
      return entries.end();
    }
    const auto found = m_entries.find(entry->m_id);
    if (found == m_entries.end() || found->second.get() != entry) {
      return entries.end();
    }
    return decltype(entries.begin())(found);
  }
  Entry* First() const
  {
    return Empty() ? nullptr : m_entries.begin()->second.get();
  }
  Entry* Last() const
  {
    return Empty() ? nullptr : m_entries.rbegin()->second.get();
  }
  Entry* FindById(uint32_t id) const
  {
    const auto found = m_entries.find(id);
    return found == m_entries.end() ? nullptr : found->second.get();
  }

  // Adoption succeeds only for a free ID. Rejected entries remain with the caller.
  uint32_t AddWithId(Entry* entry)
  {
    if (!entry || m_clearing) {
      return 0;
    }
    const uint32_t id = entry->m_id == 0 ? m_nextId : entry->m_id;
    if (!id) {
      return 0;
    }
    auto [position, inserted] = m_entries.try_emplace(id);
    if (!inserted) {
      return 0;
    }
    position->second.reset(entry);
    entry->m_id = id;
    if (m_nextId != 0 && m_nextId <= id) {
      m_nextId = id + 1;
    }
    return id;
  }

  Entry* DetachById(uint32_t id)
  {
    auto detached = m_entries.extract(id);
    return detached.empty() ? nullptr : detached.mapped().release();
  }
  bool Remove(Entry* entry)
  {
    if (!entry || FindById(entry->m_id) != entry) {
      return false;
    }
    DetachById(entry->m_id);
    return true;
  }
  void DeleteById(uint32_t id)
  {
    auto detached = m_entries.extract(id);
  }

  private:
  std::map<uint32_t, std::unique_ptr<Entry>> m_entries;
  uint32_t m_nextId = 1;
  bool m_clearing = false;
};
