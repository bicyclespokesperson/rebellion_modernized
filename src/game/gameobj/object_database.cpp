#include "gameobj/object_database.h"

#include "core/checked_new.h"
#include "core/game_string.h"
#include "gameobj/object_family.h"
#include "gameobj/object_type_db.h"

namespace
{
constexpr uint32_t kGameRootId = 3;
constexpr FamilyRange kGameRootFamilies(family::kGameRoot, family::kGameRoot + 1);
constexpr FamilyRange kSideRootFamilies(family::kSideRoot, family::kSideRoot + 1);

constexpr uint32_t kStatusOk = 0xffffffff;
constexpr uint32_t kStatusRefused = 1;
constexpr uint32_t kStatusUnknownObject = 0x12;

void SetStatus(StatusCode* status, uint32_t code)
{
  *status = StatusCode(1, code);
}

} // namespace

// ---- ObjectEntry -------------------------------------------------------------------------------------------

ObjectEntry::ObjectEntry() : IdNode(), m_flag(1), m_family(0)
{
  m_objects[0] = nullptr;
  m_objects[1] = nullptr;
  m_objects[2] = nullptr;
}

ObjectEntry::~ObjectEntry() = default;

ObjectKey* ObjectEntry::GetKey(ObjectKey* out) const
{
  out->m_value = (m_family << 24) | (m_id & 0xffffff);
  return out;
}

int ObjectEntry::AddObject(GameObjNotifier* object)
{
  if (!object)
    return 0;
  bool matches = m_family == object->GetFamily();
  const uint32_t view = object->ViewSide();
  if (view > 2)
    matches = false;
  if (matches && m_objects[view] == nullptr)
  {
    m_objects[view] = object;
    object->m_entry = this;
    return 1;
  }
  return 0;
}

int ObjectEntry::RemoveObject(GameObjNotifier* object)
{
  if (!object)
    return 0;
  bool matches = m_family == object->GetFamily();
  const uint32_t view = object->ViewSide();
  if (view > 2)
    matches = false;
  if (matches && object == m_objects[view])
  {
    m_objects[view] = nullptr;
    object->m_entry = nullptr;
    return 1;
  }
  return 0;
}

GameObjNotifier* ObjectEntry::GetObject(int view) const
{
  if (view >= 0 && view <= 2)
    return m_objects[view];
  return nullptr;
}

int ObjectEntry::IsFullyLinked() const
{
  int linked = 1;
  for (int view = 0; view <= 2; ++view)
  {
    GameObjNotifier* object = m_objects[view];
    linked = 0;
    if (object && this == object->m_entry)
    {
      linked = !(object->m_parent == nullptr && object->IsNotGameRoot());
    }
    if (!linked)
      return 0;
  }
  return linked;
}

// ---- ObjectDatabase ----------------------------------------------------------------------------------------

ObjectDatabase::ObjectDatabase() : m_ok(1)
{
}

ObjectDatabase::~ObjectDatabase() = default;

uint32_t ObjectDatabase::AddWithId(ObjectEntry* node)
{
  if (!node)
    return 0;
  const uint32_t id = node->m_id == 0 ? m_nextId : node->m_id;
  if (!id)
    return 0;
  const uint32_t key = ObjectKey::Make(node->m_family, id).m_value;
  auto [position, inserted] = m_nodes.try_emplace(key);
  if (!inserted)
    return 0;
  position->second.reset(node);
  node->m_id = id;
  if (m_nextId != 0 && m_nextId <= id)
    m_nextId = id + 1;
  return id;
}

void ObjectDatabase::Remove(ObjectEntry* node)
{
  if (!node)
    return;
  const auto found = m_nodes.find(ObjectKey::Make(node->m_family, node->m_id).m_value);
  if (found != m_nodes.end() && found->second.get() == node)
    m_nodes.extract(found).mapped().release();
}

int ObjectDatabase::AddObject(GameObjNotifier* object)
{
  if (!object)
    return 0;
  ObjectKey key;
  object->GetKey(&key);
  ObjectEntry* entry = FindEntry(&key);
  if (!entry)
  {
    entry = NewOrNull<ObjectEntry>();
    if (!entry)
      return 0;
    entry->SetId(object->m_id);
    entry->m_family = object->GetFamily();
    AddWithId(entry);
  }
  return entry->AddObject(object);
}

int ObjectDatabase::RemoveObject(GameObjNotifier* object)
{
  if (!object)
    return 0;
  ObjectKey key;
  object->GetKey(&key);
  ObjectEntry* entry = FindEntry(&key);
  if (!entry)
    return 0;
  int result = entry->RemoveObject(object);
  if (result)
  {
    bool empty = true;
    for (int view = 0; view < 3; ++view)
    {
      if (entry->GetObject(view))
        empty = false;
    }
    if (empty)
      result = DeleteEntry(entry);
  }
  return result;
}

int ObjectDatabase::DetachEntry(ObjectEntry* entry)
{
  const int present = entry != nullptr;
  if (present)
    Remove(entry);
  return present;
}

int ObjectDatabase::DeleteEntry(ObjectEntry* entry)
{
  const int detached = DetachEntry(entry);
  if (detached && entry)
    delete entry;
  return detached;
}

int ObjectDatabase::AreAllLinked()
{
  int linked = 1;
  for (const auto& entry : Entries())
  {
    if (!linked)
      break;
    linked = static_cast<ObjectEntry*>(entry.get())->IsFullyLinked();
  }
  return linked;
}

ObjectEntry* ObjectDatabase::FindEntry(const ObjectKey* key)
{
  const auto found = m_nodes.find(key->m_value);
  return found == m_nodes.end() ? nullptr : found->second.get();
}

ObjectEntry* ObjectDatabase::FindEntryInRange(const FamilyRange* range)
{
  const auto found = m_nodes.lower_bound((range->m_first << 24) | 1);
  return found != m_nodes.end() && found->second->m_family < range->m_second ? found->second.get() : nullptr;
}

// ---- ChildList ---------------------------------------------------------------------------------------------

ChildList::ChildList() : m_ok(1)
{
}

ChildList::~ChildList() = default;

uint32_t ChildList::AddWithId(GameObjNotifier* node)
{
  if (!node)
    return 0;
  const uint32_t id = node->m_id == 0 ? m_nextId : node->m_id;
  if (!id)
    return 0;
  const uint32_t key = ObjectKey::Make(node->GetFamily(), id).m_value;
  auto [position, inserted] = m_nodes.try_emplace(key);
  if (!inserted)
    return 0;
  position->second.reset(node);
  node->m_id = id;
  if (m_nextId != 0 && m_nextId <= id)
    m_nextId = id + 1;
  return id;
}

void ChildList::Remove(GameObjNotifier* node)
{
  if (!node)
    return;
  const auto found = m_nodes.find(ObjectKey::Make(node->GetFamily(), node->m_id).m_value);
  if (found != m_nodes.end() && found->second.get() == node)
    m_nodes.extract(found).mapped().release();
}

int ChildList::Read(istream* in)
{
  Clear();
  ReadValue32(in, &m_ok);
  return 1;
}

int ChildList::Write(ostream* out)
{
  WriteValue32(out, &m_ok);
  return 1;
}

int ChildList::RemoveChild(GameObjNotifier* child)
{
  GameObjNotifier* found = Find(child->GetFamily(), child->m_id);
  if (found)
    Remove(found);
  return found != nullptr;
}

GameObjNotifier* ChildList::Find(uint32_t family, uint32_t id) const
{
  const auto found = m_nodes.find(ObjectKey::Make(family, id).m_value);
  return found == m_nodes.end() ? nullptr : found->second.get();
}

GameObjNotifier* ChildList::FindFirstInRange(const FamilyRange* range) const
{
  const auto found = m_nodes.lower_bound((range->m_first << 24) | 1);
  return found != m_nodes.end() && found->second->GetFamily() < range->m_second ? found->second.get() : nullptr;
}

// ---- lookups -----------------------------------------------------------------------------------------------

GameObjNotifier* GetObjectForView(const ObjectKey* key, int view)
{
  if (view >= 1 && view <= 2)
  {
    if (view == 1)
      return LookupObject(key, 1);
    return LookupObject(key, 2);
  }
  return nullptr;
}

GameObjNotifier* GetFirstObjectInRangeForView(const FamilyRange* range, int view, int resolve)
{
  if (view >= 1 && view <= 2)
  {
    if (view == 1)
      return FindObjectInRange(range, 1, resolve);
    return FindObjectInRange(range, 2, resolve);
  }
  return nullptr;
}

GameObjNotifier* GetMasterObject(const ObjectKey* key)
{
  return LookupObject(key, 0);
}

GameObjNotifier* GetFirstMasterObjectInRange(const FamilyRange* range)
{
  return FindObjectInRange(range, 0, 1);
}

GameObjNotifier* LookupObject(const ObjectKey* key, int view)
{
  if (!key)
    return nullptr;
  ObjectEntry* entry = g_objectDatabase ? g_objectDatabase->FindEntry(key) : nullptr;
  if (!entry)
  {
    if (key->IsSet() && g_unresolvedKeys)
      g_unresolvedKeys->Find(key);
    return nullptr;
  }
  return entry->GetObject(view);
}

GameObjNotifier* FindObjectInRange(const FamilyRange* range, int view, int resolve)
{
  GameObjNotifier* object = nullptr;
  if (g_objectDatabase)
  {
    ObjectEntry* entry = g_objectDatabase->FindEntryInRange(range);
    if (entry && entry->IsFullyLinked())
    {
      object = entry->GetObject(view);
      if (resolve && object && object->IsUnderSideRoot())
        object = FindNextObjectInRange(object, range, 1);
    }
  }
  return object;
}

GameObjNotifier* FindNextObjectInRange(GameObjNotifier* object, const FamilyRange* range, int resolve)
{
  GameObjNotifier* result = nullptr;
  if (object && object->HasAllViews())
  {
    ObjectEntry* entry = object->m_entry;
    auto position = g_objectDatabase->PositionOf(entry);
    if (position != g_objectDatabase->Entries().end())
      ++position;
    while (position != g_objectDatabase->Entries().end())
    {
      entry = static_cast<ObjectEntry*>((*position).get());
      if (entry->m_family < range->m_first || entry->m_family >= range->m_second)
        break;
      ++position;
      if (entry->IsFullyLinked())
      {
        result = entry->GetObject(static_cast<int>(object->ViewSide()));
        if (resolve && result->IsUnderSideRoot())
          result = nullptr;
      }
      if (result)
        return result;
    }
  }
  return result;
}

GameObjNotifier* CreateObjectForKey(const ObjectKey* key)
{
  GameObjNotifier* object = nullptr;
  if (key->Family() != 0)
  {
    DatRecord* record = FindDataRecord(AsLookupKey(const_cast<ObjectKey*>(key)));
    if (record)
    {
      object = GameObjRoot::CreateObjectOfFamily(key->Family());
      if (object)
        object->SetRecord(record);
    }
  }
  return object;
}

int DescribeKeyValueSet(ObjectKeyValueSet* keys, std::string* out)
{
  out->clear();
  *out += std::string("(IDs=");
  if (!keys->First())
    *out += std::string("<none>");
  bool first = true;
  for (const auto& entry : keys->Entries())
  {
    auto* node = static_cast<ObjectKeyNode*>(entry.get());
    if (first)
      first = false;
    else
      *out += std::string(";");
    std::string separator(",");
    const int value = static_cast<ObjectKeyValueNode*>(node)->m_value;
    ObjectKey key;
    node->GetKey(&key);
    out->append(std::to_string(key.Id())) += separator;
    out->append(std::to_string(value));
  }
  *out += std::string(")");
  return 1;
}

int CollectKeyDependencies(void* context, ObjectKeyValueSet* keys, ObjectKeyValueSet* collected, StatusCode* status)
{
  int ok = 1;
  SetStatus(status, kStatusOk);
  *collected = ObjectKeyValueSet();
  ObjectKeyValueSet dependencies;
  for (const auto& keyEntry : keys->Entries())
  {
    if (!ok || status->m_second != kStatusOk)
      break;
    auto* node = static_cast<ObjectKeyNode*>(keyEntry.get());
    dependencies.Clear();
    ObjectKey key;
    node->GetKey(&key);
    GameObjNotifier* object = GetMasterObject(&key);
    if (!object)
      SetStatus(status, kStatusUnknownObject);
    if (object && status->m_second == kStatusOk)
    {
      ok = object->CollectDependencies(context, static_cast<ObjectKeyValueNode*>(node)->m_value, &dependencies);
      for (const auto& depEntry : dependencies.Entries())
      {
        auto* dep = static_cast<ObjectKeyNode*>(depEntry.get());
        ObjectKey depKey;
        dep->GetKey(&depKey);
        if (!collected->Find(&depKey))
        {
          dep->GetKey(&depKey);
          collected->AddKeyValue(&depKey, 0);
        }
      }
    }
  }
  if (!ok)
    SetStatus(status, kStatusRefused);
  return ok;
}

// ---- database and unresolved-key set lifetime --------------------------------------------------------------

int RegisterObject(GameObjNotifier* object)
{
  const bool haveDatabase = g_objectDatabase != nullptr;
  if (haveDatabase)
    g_objectDatabase->AddObject(object);
  return haveDatabase;
}

int UnregisterObject(GameObjNotifier* object)
{
  const bool haveDatabase = g_objectDatabase != nullptr;
  if (haveDatabase)
    g_objectDatabase->RemoveObject(object);
  return haveDatabase;
}

void CreateObjectDatabase()
{
  DestroyObjectDatabase();
  g_objectDatabase = NewOrNull<ObjectDatabase>();
  if (!g_objectDatabase)
    DestroyObjectDatabase();
}

void DestroyObjectDatabase()
{
  if (g_objectDatabase)
  {
    delete g_objectDatabase;
    g_objectDatabase = nullptr;
  }
}

int CheckObjectDatabase()
{
  if (!g_objectDatabase)
    return 0;
  return g_objectDatabase->AreAllLinked();
}

int NoteUnresolvedKeyOf(GameObjNotifier* object)
{
  const bool haveSet = g_unresolvedKeys != nullptr;
  if (haveSet)
  {
    ObjectKey key;
    object->GetKey(&key);
    g_unresolvedKeys->AddKey(&key);
  }
  return haveSet;
}

void CreateUnresolvedKeySet()
{
  DestroyUnresolvedKeySet();
  g_unresolvedKeys = NewOrNull<ObjectKeySet>();
  if (!g_unresolvedKeys)
    DestroyUnresolvedKeySet();
}

void DestroyUnresolvedKeySet()
{
  if (g_unresolvedKeys)
  {
    delete g_unresolvedKeys;
    g_unresolvedKeys = nullptr;
  }
}

int LoadUnresolvedKeySet(istream* in)
{
  DestroyUnresolvedKeySet();
  g_unresolvedKeys = NewOrNull<ObjectKeySet>();
  if (!g_unresolvedKeys)
    DestroyUnresolvedKeySet();
  const bool haveSet = g_unresolvedKeys != nullptr;
  if (haveSet)
    g_unresolvedKeys->Load(in);
  return haveSet;
}

int SaveUnresolvedKeySet(ostream* out)
{
  const bool haveSet = g_unresolvedKeys != nullptr;
  if (haveSet)
    g_unresolvedKeys->Save(out);
  return haveSet;
}

// ---- placing objects in the tree ---------------------------------------------------------------------------

namespace
{

GameObjNotifier* FindParentFor(GameObjNotifier* object, int view)
{
  if (object->m_id == kGameRootId)
    return FindObjectInRange(&kGameRootFamilies, view, 1);
  if (object->IsNotGameRoot())
    return FindObjectInRange(&kSideRootFamilies, view, 1);
  return nullptr;
}

} // namespace

int AttachMasterObject(GameObjNotifier* object)
{
  const bool valid = object != nullptr;
  if (!valid)
    return false;
  object->ApplyDefaultId();
  object->SetViewSide(0);
  object->SetSideMask(1);
  if (GameObjNotifier* parent = FindParentFor(object, 0))
  {
    GameObjNotifier* previousParent;
    parent->AddChild(object, &previousParent);
  }
  return valid;
}

int AttachSideObject(GameObjNotifier* object, GameObjNotifier* master, int view)
{
  const bool valid = object != nullptr;
  if (valid)
  {
    object->SetId(master->m_id);
    object->SetRecord(master->m_record);
    object->SetViewSide(static_cast<uint32_t>(view));
    object->SetSideMask(2);
  }
  if (!valid)
    return false;
  if (GameObjNotifier* parent = FindParentFor(object, view))
  {
    GameObjNotifier* previousParent;
    parent->AddChild(object, &previousParent);
  }
  return valid;
}

int DetachObject(GameObjNotifier* object)
{
  if (!object)
    return false;
  if (FindParentFor(object, static_cast<int>(object->ViewSide())))
    GameObjNotifier::RemoveFromParent(object);
  return object != nullptr;
}
