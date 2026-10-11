#include "ai/ai_galaxy_analysis.h"

#include "ai/ai_common.h"
#include "core/checked_new.h"
#include "economy/side.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"

using ai::InFamilies;

namespace
{

constexpr uint32_t kTotalsStale = 0x80000000;
constexpr uint32_t kSumsWanted = 0x20000000;
constexpr uint32_t kSectorsChanged = 0xa0000000;

int DistanceSquared(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
  return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
}

// The families whose objects have no system or sector to re-scan and no parent worth following.
bool IsUntracked(uint32_t family)
{
  return InFamilies(family, 0x14, 0x1c) || InFamilies(family, 8, 0x10) || InFamilies(family, 0x30, 0x40) ||
         InFamilies(family, 0x40, 0x80) || family == 0xf2 || family == 0xf8 || family == 0xf1 ||
         InFamilies(family, 0x98, 0xa0);
}

} // namespace

AiGalaxyAnalysis::AiGalaxyAnalysis() : m_flags(0)
{
  m_view = 0;
  m_step = 0;
  m_phase = 0;
  m_cursor = nullptr;
  for (int& value : m_stat)
    value = 0;
  m_xf8 = 0;
}

AiGalaxyAnalysis::~AiGalaxyAnalysis()
{
  m_cursor = nullptr;
}

void AiGalaxyAnalysis::Init(int view)
{
  m_view = view;
  m_systemScans.m_view = view;
  m_sectorScans.m_view = view;
  m_systemScans.m_classes = &m_classes;
}

void AiGalaxyAnalysis::Load(istream* in)
{
  ObjectKey key;
  ReadValue32(in, &m_view);
  m_classes.Clear();
  m_classes.Build(m_view);
  ReadValue32(in, &m_step);
  ReadValue32(in, &m_phase);
  m_flags.Read(in);
  m_systemScans.m_classes = &m_classes;
  m_systemScans.Load(in);
  m_sectorScans.LoadAll(in, &m_systemScans, this);
  for (int& value : m_stat)
    ReadValue32(in, &value);
  ReadValue32(in, &m_xf8);
  uint32_t flag;
  ReadValue32(in, &flag);
  if (flag == 0)
  {
    m_cursor = nullptr;
  }
  else
  {
    key.Read(in);
    m_cursor = m_sectorScans.FindScan(key);
  }
  m_flags.m_value |= kTotalsStale;
}

void AiGalaxyAnalysis::Save(ostream* out)
{
  WriteValue32(out, &m_view);
  WriteValue32(out, &m_step);
  WriteValue32(out, &m_phase);
  m_flags.Write(out);
  m_systemScans.Save(out);
  m_sectorScans.Save(out);
  for (int& value : m_stat)
    WriteValue32(out, &value);
  WriteValue32(out, &m_xf8);
  if (!m_cursor)
  {
    uint32_t none = 0;
    WriteValue32(out, &none);
  }
  else
  {
    ObjectKey key;
    m_cursor->GetKey(&key);
    uint32_t family = key.m_value >> 24;
    WriteValue32(out, &family);
    key.Write(out);
  }
}

int AiGalaxyAnalysis::Step()
{
  switch (m_step)
  {
    case 1:
    {
      m_classes.Build(m_view);
      Side* side = static_cast<Side*>(GetSideObject(m_view, m_view));
      if (side)
      {
        IntPair pair;
        SetFocusKey(reinterpret_cast<const uint32_t*>(GetSidePairAt58(side, &pair)));
        SetFocusValue(side->m_maintRequired);
      }
      m_step = 2;
      return 0;
    }
    case 2:
      if (m_systemScans.BuildStep())
      {
        m_step = 3;
        return 0;
      }
      break;
    case 3:
      if (m_sectorScans.BuildStep(&m_systemScans, this))
      {
        m_step = 4;
        return 0;
      }
      break;
    case 4:
      if (AnalyzeStep())
        return 1;
      break;
    default:
      m_step = 1;
  }
  return 0;
}

int AiGalaxyAnalysis::AnalyzeStep()
{
  int result = 0;
  if (m_phase != 1)
  {
    if (m_phase == 2)
    {
      if (m_flags.m_value & kTotalsStale)
      {
        if (m_flags.m_value & kSumsWanted)
        {
          for (int index = 4; index < 0x1e; ++index)
            m_stat[index] = 0;
          for (AvlNode* node = m_sectorScans.First(); node; node = node->m_successor)
          {
            m_cursor = static_cast<AiSectorScan*>(node);
            AddSectorTotals(m_cursor->m_stat, &m_cursor->m_flagsA.m_value);
          }
          m_cursor = nullptr;
        }
        m_flags.m_value &= 0x5fffffff;
      }
      m_phase = 1;
      return 1;
    }
    m_phase = 1;
    return 0;
  }
  if (m_cursor == nullptr)
  {
    m_cursor = static_cast<AiSectorScan*>(m_sectorScans.First());
    if (m_cursor == nullptr)
    {
      m_phase = 2;
      return 0;
    }
  }
  do
  {
    if (result != 0)
      break;
    result = m_cursor->Update(m_view);
    if (result != 0)
      m_flags.m_value |= kSectorsChanged;
    m_cursor = static_cast<AiSectorScan*>(m_cursor->m_successor);
  } while (m_cursor != nullptr);
  if (m_cursor != nullptr)
    return 0;
  m_phase = 2;
  return 0;
}

void AiGalaxyAnalysis::SetFocusKey(const uint32_t* key)
{
  m_stat[0] = *key;
  m_flags.m_value |= kTotalsStale;
}

void AiGalaxyAnalysis::SetFocusValue(int value)
{
  m_stat[1] = value;
  m_flags.m_value |= kTotalsStale;
}

void AiGalaxyAnalysis::NoteChanged(const ObjectKey* keyPointer, int unused)
{
  (void)unused;
  ObjectKey key = *keyPointer;
  for (;;)
  {
    const uint32_t family = key.Family();
    if (InFamilies(family, 0x90, 0x98))
    {
      m_systemScans.NoteSystem(&key);
      return;
    }
    if (IsUntracked(family))
      return;
    if (InFamilies(family, 0x80, 0x90))
    {
      m_sectorScans.NoteSector(&key);
      return;
    }
    GameObjNotifier* object = GetObjectInFamilies_01_ff(m_view, &key);
    if (!object || !object->m_parent)
      return;
    ObjectKey parentKey;
    key = *GetObjectKey(object->m_parent, &parentKey);
  }
}

void AiGalaxyAnalysis::NoteChangedAlt(const ObjectKey* keyPointer)
{
  ObjectKey key = *keyPointer;
  for (;;)
  {
    const uint32_t family = key.Family();
    if (InFamilies(family, 0x90, 0x98))
    {
      m_systemScans.NoteSystem(&key);
      return;
    }
    if (IsUntracked(family))
      return;
    if (InFamilies(family, 0x80, 0x90))
    {
      m_sectorScans.NoteSector(&key);
      return;
    }
    GameObjNotifier* object = GetObjectInFamilies_01_ff(m_view, &key);
    if (!object || !object->m_parent)
      return;
    ObjectKey parentKey;
    key = *GetObjectKey(object->m_parent, &parentKey);
  }
}

void AiGalaxyAnalysis::NoteMoved(const ObjectKey* key, const ObjectKey* from, const ObjectKey* to)
{
  const uint32_t family = key->Family();
  if (InFamilies(family, 0x90, 0x98))
    m_systemScans.NoteSystem(key);
  else if (InFamilies(family, 0x80, 0x90))
    m_sectorScans.NoteSector(key);
  NoteChanged(from, 0);
  NoteChanged(to, 0);
}

AiRankedList* AiGalaxyAnalysis::RankSectors(uint32_t requireA,
                                            uint32_t requireB,
                                            uint32_t forbidA,
                                            uint32_t forbidB,
                                            uint32_t statIndex,
                                            int descending)
{
  AiRankedList* list = NewOrNull<AiRankedList>(descending);
  if (!list)
    return list;
  for (AvlNode* node = m_sectorScans.First(); node; node = node->m_successor)
  {
    AiSectorScan* scan = static_cast<AiSectorScan*>(node);
    const uint32_t a = scan->m_flagsA.m_value;
    const uint32_t b = scan->m_flagsB.m_value;
    if ((requireA & a) != requireA || (forbidA & a) != 0 || (requireB & b) != requireB || (forbidB & b) != 0)
      continue;
    ObjectKey key;
    AiRankedItem* item = NewOrNull<AiRankedItem>(*scan->GetKey(&key), scan->m_stat[statIndex & 0xffff]);
    list->AddItem(item);
  }
  list->Number();
  return list;
}

AiRankedList* AiGalaxyAnalysis::RankSectorsByDistance(const ObjectKey* target,
                                                      uint32_t requireA,
                                                      uint32_t requireB,
                                                      uint32_t forbidA,
                                                      uint32_t forbidB,
                                                      int descending)
{
  AiRankedList* list = nullptr;
  ObjectKey key = *target;
  for (;;)
  {
    if (InFamilies(key.Family(), 0x80, 0x90))
      break;
    GameObjNotifier* object = GetObjectInFamilies_01_ff(m_view, &key);
    if (!object || !object->m_parent)
      break;
    ObjectKey parentKey;
    key = *GetObjectKey(object->m_parent, &parentKey);
  }
  if (InFamilies(key.Family(), 0x80, 0x90))
  {
    AiSectorScan* targetScan = m_sectorScans.FindScan(key);
    if (targetScan)
    {
      list = NewOrNull<AiRankedList>(descending);
      if (list)
      {
        for (AvlNode* node = m_sectorScans.First(); node; node = node->m_successor)
        {
          AiSectorScan* scan = static_cast<AiSectorScan*>(node);
          const uint32_t a = scan->m_flagsA.m_value;
          const uint32_t b = scan->m_flagsB.m_value;
          if ((requireA & a) != requireA || (forbidA & a) != 0 || (requireB & b) != requireB || (forbidB & b) != 0)
            continue;
          const int distance = DistanceSquared(targetScan->m_x, targetScan->m_y, scan->m_x, scan->m_y);
          ObjectKey scanKey;
          AiRankedItem* item = NewOrNull<AiRankedItem>(*scan->GetKey(&scanKey), distance);
          list->AddItem(item);
        }
        list->Number();
      }
    }
  }
  return list;
}

AiRankedList* AiGalaxyAnalysis::RankSystemsOfSector(const ObjectKey* sector,
                                                    uint32_t requireA,
                                                    uint32_t requireB,
                                                    uint32_t forbidA,
                                                    uint32_t forbidB,
                                                    uint32_t statIndex,
                                                    int descending)
{
  ObjectKey sectorCopy = *sector;
  AiSectorScan* scan = m_sectorScans.FindScan(sectorCopy);
  if (!scan)
    return nullptr;
  return scan->RankSystemsByStat(requireA, requireB, forbidA, forbidB, statIndex, descending);
}

AiRankedList* AiGalaxyAnalysis::RankSystemsOfSectorByDistance(const ObjectKey* sector,
                                                              const ObjectKey* target,
                                                              uint32_t requireA,
                                                              uint32_t requireB,
                                                              uint32_t forbidA,
                                                              uint32_t forbidB,
                                                              int descending)
{
  ObjectKey sectorCopy = *sector;
  AiSectorScan* scan = m_sectorScans.FindScan(sectorCopy);
  if (!scan)
    return nullptr;
  return scan->RankSystemsByDistance(target, requireA, requireB, forbidA, forbidB, descending);
}

int AiGalaxyAnalysis::ChooseFromSide(ObjectKey* out, int recordType, uint32_t requiredFlags, int mode, int descending)
{
  GameObjNotifier* side = GetSideObject(m_view, m_view);
  return m_classes.ChooseFromSide(out, side, recordType, requiredFlags, mode, descending);
}

void AiGalaxyAnalysis::AddSectorTotals(const int* stats, const int* flags)
{
  (void)flags;
  static const int kSource[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 22, 23, 24};
  int target = 0x90;
  for (int source : kSource)
  {
    Total(target) += stats[source];
    target += 4;
  }
}

AiSystemScan* AiGalaxyAnalysis::FindSystemScan(const ObjectKey* key)
{
  ObjectKey copy = *key;
  return m_systemScans.FindScan(copy);
}
