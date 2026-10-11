#include "gameobj/game_obj_root.h"

#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "data/dat_record.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_database.h"
#include "gameobj/object_type_db.h"

// The game's table of object factories, filled by the registration function at 0x51ebf0.
struct GameObjFactoryEntry
{
  uint32_t family;
  GameObjNotifier* (*create)();
};
extern int g_gameObjFactoryCount;
extern GameObjFactoryEntry g_gameObjFactories[];

namespace
{

constexpr uint32_t kGameRootFamily = 0xf1;
constexpr uint32_t kSideMaskBits = 0xf;
constexpr uint32_t kViewSideShift = 4;
constexpr uint32_t kControlSideShift = 6;
constexpr uint32_t kFlag8Shift = 8;
constexpr uint32_t kFlag31Shift = 31;
constexpr int kMaxFactories = 0x3c;
} // namespace

// ---- GameObjRoot -------------------------------------------------------------------------------------------

GameObjRoot::GameObjRoot() : IdNode(), m_parent(nullptr), m_entry(nullptr), m_flags(0)
{
  SetId(1);
  SetSideMask(1);
  SetViewSide(0);
  SetControlSide(3);
  SetFlag8(0);
  SetFlag31(1);
  SetFlag31(1);
}

GameObjRoot::~GameObjRoot() = default;

int GameObjRoot::GetFlag31() const
{
  return static_cast<int>(m_flags >> kFlag31Shift);
}

void GameObjRoot::SetFlag31(int on)
{
  m_flags = (m_flags & 0x7fffffff) | (static_cast<uint32_t>(on) << kFlag31Shift);
}

int GameObjRoot::GetParentKey(ObjectKey* out)
{
  if (!m_parent)
  {
    out->Reset();
  }
  else
  {
    ObjectKey parentKey;
    m_parent->GetKey(&parentKey);
    *out = parentKey;
  }
  return 1;
}

int GameObjRoot::GetFlag8() const
{
  return static_cast<int>((m_flags >> kFlag8Shift) & 1);
}

int GameObjRoot::MatchesSideMask(uint32_t side) const
{
  const uint32_t mask = m_flags & kSideMaskBits;
  if (mask != side && side != 3 && mask != 3)
    return 0;
  return 1;
}

GameObjNotifier* GameObjRoot::CreateObjectOfFamily(uint32_t family)
{
  GameObjNotifier* (*create)() = nullptr;
  for (int i = 0; i < g_gameObjFactoryCount && !create; ++i)
  {
    if (g_gameObjFactories[i].family == family)
      create = g_gameObjFactories[i].create;
  }
  if (create)
    return create();
  return nullptr;
}

int GameObjRoot::RegisterFactory(uint32_t family, GameObjNotifier* (*factory)())
{
  bool absent = true;
  for (int i = 0; i < g_gameObjFactoryCount && absent; ++i)
    absent = g_gameObjFactories[i].family != family;
  if (absent && static_cast<uint32_t>(g_gameObjFactoryCount) + 1U < kMaxFactories + 1U)
  {
    g_gameObjFactories[g_gameObjFactoryCount].family = family;
    g_gameObjFactories[g_gameObjFactoryCount].create = factory;
    ++g_gameObjFactoryCount;
  }
  return 0;
}

int GameObjRoot::ReadFields(istream* in)
{
  ReadId(in);
  m_parent = nullptr;
  m_entry = nullptr;
  ReadValue32(in, &m_flags);
  return 1;
}

int GameObjRoot::WriteFields(ostream* out)
{
  WriteId(out);
  WriteValue32(out, &m_flags);
  return 1;
}

int GameObjRoot::ChecksumFields(Crc32* crc)
{
  crc->AddInt(static_cast<int>(m_id));
  crc->AddInt(static_cast<int>(m_flags));
  return 1;
}

void GameObjRoot::SetSideMask(uint32_t mask)
{
  m_flags = ((mask ^ m_flags) & kSideMaskBits) ^ m_flags;
}

void GameObjRoot::SetViewSide(uint32_t side)
{
  m_flags = ((side & 3) << kViewSideShift) | (m_flags & 0xffffffcf);
}

void GameObjRoot::SetControlSide(uint32_t side)
{
  m_flags = ((side & 3) << kControlSideShift) | (m_flags & 0xffffff3f);
}

void GameObjRoot::SetFlag8(uint32_t on)
{
  m_flags = ((on & 1) << kFlag8Shift) | (m_flags & 0xfffffeff);
}

void GameObjRoot::ApplyDefaultId()
{
  if (m_id == 1)
    SetId(static_cast<uint32_t>(CallIdHook()));
}

int GameObjRoot::IsNotGameRoot()
{
  return GetFamily() != kGameRootFamily;
}

// ---- GameObjNotifier ---------------------------------------------------------------------------------------

GameObjNotifier::GameObjNotifier() : GameObjRoot(), m_children(nullptr)
{
  SetFlag31(1);
}

GameObjNotifier::~GameObjNotifier()
{
  if (m_children)
  {
    delete m_children;
    m_children = nullptr;
  }
}

uint32_t GameObjNotifier::GetRecordId()
{
  if (HasRecord())
    return m_record->m_id;
  return 0;
}

GameObjNotifier* GameObjNotifier::FirstChild()
{
  if (m_children)
    return static_cast<GameObjNotifier*>(m_children->First());
  return nullptr;
}

GameObjNotifier* GameObjNotifier::FindFirstChildInRange(const FamilyRange* range)
{
  if (m_children)
    return m_children->FindFirstInRange(range);
  return nullptr;
}

GameObjNotifier* GameObjNotifier::NextSiblingInRange(const FamilyRange* range)
{
  if (!m_parent)
    return nullptr;
  auto position = m_parent->m_children->PositionOf(this);
  if (position == m_parent->m_children->Entries().end())
    return nullptr;
  ++position;
  GameObjNotifier* next =
    position == m_parent->m_children->Entries().end() ? nullptr : static_cast<GameObjNotifier*>(position->get());
  if (next)
  {
    const uint32_t family = next->GetFamily();
    if (range->m_first <= family && family < range->m_second)
      return next;
  }
  return nullptr;
}

GameObjNotifier* GameObjNotifier::GetViewObject(int view)
{
  if (m_entry)
    return m_entry->GetObject(view);
  return nullptr;
}

int GameObjNotifier::HasAllViews()
{
  if (m_entry)
    return m_entry->IsFullyLinked();
  return 0;
}

int GameObjNotifier::IsMasterView() const
{
  return (m_flags & 0x30) == 0;
}

int GameObjNotifier::IsSideView() const
{
  return (m_flags & 0x30) != 0;
}

int GameObjNotifier::HasRecord() const
{
  return m_record != nullptr;
}

int GameObjNotifier::CanAdopt(GameObjNotifier* child)
{
  bool ok = false;
  if (child)
  {
    ok = child->IsNotGameRoot();
    if (ok)
    {
      ok = ((child->m_flags ^ m_flags) & 0x30) == 0;
      if (ok && child->m_parent)
        ok = child->m_parent->m_children != nullptr;
    }
  }
  return ok;
}

int GameObjNotifier::ReadObject(istream* in)
{
  int ok = ReadFields(in);
  if (ok)
  {
    m_record = nullptr;
    uint32_t id;
    ReadValue32(in, &id);
    if (id != 0)
    {
      ObjectKey key = ObjectKey::Make(GetFamily(), id);
      m_record = FindDataRecord(AsLookupKey(&key));
    }
  }
  return ok;
}

int GameObjNotifier::WriteObject(ostream* out)
{
  int ok = WriteFields(out);
  if (ok)
  {
    uint32_t id = m_record ? m_record->m_id : 0;
    WriteValue32(out, &id);
  }
  return ok;
}

int GameObjNotifier::ChecksumObject(Crc32* crc)
{
  int ok = ChecksumFields(crc);
  if (ok)
  {
    crc->AddInt(m_record ? static_cast<int>(m_record->m_id) : 0);
  }
  return ok;
}

int GameObjNotifier::NotifyIfComplete()
{
  int result = HasRecord();
  if (result)
    result = HasAllViews();
  return result;
}

int GameObjNotifier::ReadTree(istream* in)
{
  int ok = 1;
  int hasChildren = 0;
  ReadValue32(in, &hasChildren);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return 0;
  if (hasChildren == 0)
    return ok;

  m_children = NewOrNull<ChildList>();
  ok = 0;
  if (!m_children)
  {
    iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
    return 0;
  }
  if (m_children->m_ok != 0)
    ok = m_children->Read(in);
  else
    iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);

  uint32_t count = 0;
  if (ok)
  {
    ReadValue32(in, &count);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
      return 0;
    if (count != 0)
      ok = CheckPositionMarker(in);
  }
  for (uint32_t i = 0; ok && i < count; ++i)
  {
    uint32_t family = 0;
    ReadValue32(in, &family);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
      return 0;
    GameObjNotifier* child = CreateObjectOfFamily(family);
    ok = 0;
    if (child)
    {
      ok = child->Read(in);
      if (ok)
      {
        ok = m_children->AddWithId(child) != 0;
        if (!ok)
          delete child;
        else
        {
          child->m_parent = this;
          ok = child->ReadTree(in);
        }
      }
      else
      {
        delete child;
      }
    }
    else
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
    }
  }
  return ok;
}

int GameObjNotifier::WriteTree(ostream* out)
{
  uint32_t hasChildren = m_children != nullptr;
  WriteValue32(out, &hasChildren);
  int ok = 1;
  if (m_children)
  {
    ok = m_children->Write(out);
    if (ok)
    {
      if (!WriteCount32(out, m_children->Count()))
        return 0;
      if (m_children->Count() != 0)
        WritePositionMarker(out);
    }
    for (const auto& entry : m_children->Entries())
    {
      if (!ok)
        break;
      auto* child = static_cast<GameObjNotifier*>(entry.get());
      uint32_t family = child->GetFamily();
      WriteValue32(out, &family);
      ok = 0;
      if (child->Write(out))
        ok = child->WriteTree(out);
    }
  }
  return ok;
}

int GameObjNotifier::ChecksumTree(Crc32* crc)
{
  int ok = 1;
  if (m_children)
  {
    crc->AddInt(m_children->m_ok);
    int included = 0;
    for (const auto& entry : m_children->Entries())
    {
      auto* child = static_cast<GameObjNotifier*>(entry.get());
      if (child->IncludeInChecksum())
      {
        ++included;
        crc->AddInt(static_cast<int>(child->GetFamily()));
        ok = 0;
        if (child->Checksum(crc))
          ok = child->ChecksumTree(crc);
      }
    }
    crc->AddInt(included);
  }
  return ok;
}

int GameObjNotifier::SetRecord(DatRecord* record)
{
  return AssignRecord(record);
}

int GameObjNotifier::AssignRecord(DatRecord* record)
{
  bool matches = true;
  if (record)
    matches = record->m_third == GetFamily();
  if (matches)
    m_record = record;
  return matches;
}

int GameObjNotifier::AddChild(GameObjNotifier* child, GameObjNotifier** previousParent)
{
  int ok = 1;
  if (!m_children)
  {
    m_children = NewOrNull<ChildList>();
    ok = m_children != nullptr;
    if (ok)
      ok = m_children->m_ok;
  }
  if (ok)
    ok = CanAdopt(child);
  GameObjNotifier* oldParent = child->m_parent;
  *previousParent = oldParent;
  if (oldParent)
    ok = oldParent->m_children->RemoveChild(child);
  if (ok)
  {
    ok = m_children->AddWithId(child) != 0;
    if (ok)
      child->m_parent = this;
  }
  return ok;
}

int GameObjNotifier::RemoveFromParent(GameObjNotifier* child)
{
  int ok = 1;
  if (child->m_parent)
    ok = child->m_parent->m_children->RemoveChild(child);
  if (ok)
    child->m_parent = nullptr;
  return ok;
}

int UpdateFlagBits(uint32_t mask, uint32_t value, uint32_t* flags)
{
  const int changed = (value & mask) != (mask & *flags);
  if (changed)
    *flags = (~mask & *flags) | (value & mask);
  return changed;
}

int UpdateFlagBit(uint32_t mask, int on, uint32_t* flags)
{
  return UpdateFlagBits(mask, on ? 0xffffffff : 0, flags);
}

GameObjFactoryEntry g_gameObjFactories[0x3c]{};
int g_gameObjFactoryCount{};
