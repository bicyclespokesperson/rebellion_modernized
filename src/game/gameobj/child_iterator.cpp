#include "gameobj/child_iterator.h"

#include "gameobj/game_obj.h"
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"

ChildIterator::ChildIterator(GameObjNotifier* parent, const FamilyRange* range, uint32_t stateFilter)
: m_parent(parent),
  m_range(*range),
  m_hasSideFilter(0),
  m_side(0),
  m_current(nullptr)
{
  m_stateFilter = stateFilter;
}

ChildIterator::ChildIterator(GameObjNotifier* parent, uint32_t side, const FamilyRange* range, uint32_t stateFilter)
: m_parent(parent),
  m_range(*range),
  m_hasSideFilter(1),
  m_side(side),
  m_current(nullptr)
{
  m_stateFilter = stateFilter;
}

ChildIterator::~ChildIterator() = default;

ChildIterator* ChildIterator::Advance()
{
  bool atEnd;
  int accepted;
  do
  {
    if (m_current)
    {
      auto position = m_parent->m_children->PositionOf(m_current);
      if (position != m_parent->m_children->Entries().end())
        ++position;
      m_current =
        position == m_parent->m_children->Entries().end() ? nullptr : static_cast<GameObjNotifier*>((*position).get());
    }
    atEnd = m_current == nullptr;
    accepted = atEnd ? 1 : Accepts();
  } while (!accepted);
  if (atEnd)
    Reset();
  return this;
}

ChildIterator* ChildIterator::Begin()
{
  Reset();
  m_current = m_parent ? m_parent->FirstChild() : nullptr;
  if (!m_current)
  {
    Reset();
    return this;
  }
  if (!Accepts())
    Advance();
  return this;
}

void ChildIterator::Reset()
{
  m_current = nullptr;
}

int ChildIterator::Count()
{
  int count = 0;
  Begin();
  while (m_current)
  {
    ++count;
    Advance();
  }
  return count;
}

int ChildIterator::HasCurrent()
{
  Begin();
  return m_current != nullptr;
}

int ChildIterator::Accepts()
{
  int accepted = 1;
  if (m_current)
  {
    const uint32_t family = m_current->GetFamily();
    if (family < m_range.m_first || family >= m_range.m_second)
      accepted = 0;
    if (!static_cast<GameObj*>(m_current)->GameObj::MatchesStateFilter(m_stateFilter))
      accepted = 0;
    if (m_hasSideFilter && m_current->ControlSide() != m_side)
      accepted = 0;
  }
  return accepted;
}

UnitIterator::UnitIterator(GameObjNotifier* parent, uint32_t stateFilter)
: ChildIterator(parent, &family::kUnits, stateFilter)
{
}

UnitIterator::~UnitIterator() = default;
