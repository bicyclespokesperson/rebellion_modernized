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

ObjectKeyNode* NextKeyNode(ObjectKeyNode* node)
{
  return static_cast<ObjectKeyNode*>(node->m_successor);
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

ObjectEntry* ObjectEntry::NextInRange(const FamilyRange* range) const
{
  ObjectEntry* next = static_cast<ObjectEntry*>(m_successor);
  if (next && range->m_first <= next->m_family && next->m_family < range->m_second)
    return next;
  return nullptr;
}

// ---- ObjectDatabase ----------------------------------------------------------------------------------------

ObjectDatabase::ObjectDatabase() : IdAvlTree(), m_ok(1)
{
}

ObjectDatabase::~ObjectDatabase() = default;

int ObjectDatabase::IsLess(AvlNode* a, AvlNode* b)
{
  bool less = false;
  if (a && b)
  {
    ObjectKey keyA;
    ObjectKey keyB;
    less = static_cast<ObjectEntry*>(a)->GetKey(&keyA)->m_value < static_cast<ObjectEntry*>(b)->GetKey(&keyB)->m_value;
  }
  return less;
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

int ObjectDatabase::DetachEntry(AvlNode* entry)
{
  const int present = entry != nullptr;
  if (present)
    Remove(entry);
  return present;
}

int ObjectDatabase::DeleteEntry(AvlNode* entry)
{
  const int detached = DetachEntry(entry);
  if (detached && entry)
    delete entry;
  return detached;
}

int ObjectDatabase::AreAllLinked()
{
  int linked = 1;
  for (AvlNode* node = First(); node && linked; node = node->m_successor)
    linked = static_cast<ObjectEntry*>(node)->IsFullyLinked();
  return linked;
}

ObjectEntry* ObjectDatabase::FindEntry(const ObjectKey* key)
{
  AvlNode* node = LowerBound(
    [key](const AvlNode* candidate)
    {
      ObjectKey nodeKey;
      static_cast<const ObjectEntry*>(candidate)->GetKey(&nodeKey);
      return nodeKey.m_value < key->m_value;
    });
  if (node)
  {
    ObjectKey nodeKey;
    static_cast<ObjectEntry*>(node)->GetKey(&nodeKey);
    if (nodeKey.m_value != key->m_value)
      node = nullptr;
  }
  return static_cast<ObjectEntry*>(node);
}

ObjectEntry* ObjectDatabase::FindEntryInRange(const FamilyRange* range)
{
  const uint32_t probe = (range->m_first << 24) | 1;
  AvlNode* node = LowerBound(
    [probe](const AvlNode* candidate)
    {
      ObjectKey nodeKey;
      static_cast<const ObjectEntry*>(candidate)->GetKey(&nodeKey);
      return nodeKey.m_value < probe;
    });
  if (node)
  {
    const uint32_t family = static_cast<ObjectEntry*>(node)->m_family;
    if (family < range->m_first || family >= range->m_second)
      node = nullptr;
  }
  return static_cast<ObjectEntry*>(node);
}

// ---- ChildList ---------------------------------------------------------------------------------------------

ChildList::ChildList() : IdAvlTree(), m_ok(1)
{
}

ChildList::~ChildList() = default;

int ChildList::IsLess(AvlNode* a, AvlNode* b)
{
  if (!a || !b)
    return 0;
  GameObjNotifier* objectA = static_cast<GameObjNotifier*>(a);
  GameObjNotifier* objectB = static_cast<GameObjNotifier*>(b);
  const uint32_t idA = objectA->m_id;
  const uint32_t idB = objectB->m_id;
  const uint32_t familyA = objectA->GetFamily();
  const uint32_t familyB = objectB->GetFamily();
  return familyA < familyB || (familyA == familyB && idA < idB);
}

int ChildList::Read(istream* in)
{
  IdAvlTree::Clear();
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
  AvlNode* found = Find(child->GetFamily(), child->m_id);
  if (found)
    AvlTree::Remove(found);
  return found != nullptr;
}

GameObjNotifier* ChildList::Find(uint32_t family, uint32_t id) const
{
  AvlNode* found = LowerBound(
    [family, id](const AvlNode* candidate)
    {
      auto* node = const_cast<GameObjNotifier*>(static_cast<const GameObjNotifier*>(candidate));
      const uint32_t nodeFamily = node->GetFamily();
      return nodeFamily < family || (nodeFamily == family && node->m_id < id);
    });
  auto* node = static_cast<GameObjNotifier*>(found);
  if (node && (node->GetFamily() != family || node->m_id != id))
    node = nullptr;
  return node;
}

GameObjNotifier* ChildList::FindFirstInRange(const FamilyRange* range) const
{
  const uint32_t first = range->m_first;
  AvlNode* found = LowerBound(
    [first](const AvlNode* candidate)
    {
      auto* node = const_cast<GameObjNotifier*>(static_cast<const GameObjNotifier*>(candidate));
      const uint32_t family = node->GetFamily();
      return family < first || (family == first && node->m_id < 1);
    });
  auto* node = static_cast<GameObjNotifier*>(found);
  if (node)
  {
    const uint32_t family = node->GetFamily();
    if (family < range->m_first || family >= range->m_second)
      node = nullptr;
  }
  return node;
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
    while ((entry = entry->NextInRange(range)) != nullptr)
    {
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
  for (ObjectKeyNode* node = keys->First(); node; node = NextKeyNode(node))
  {
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
  for (ObjectKeyNode* node = keys->First(); node && ok && status->m_second == kStatusOk; node = NextKeyNode(node))
  {
    dependencies.Clear();
    ObjectKey key;
    node->GetKey(&key);
    GameObjNotifier* object = GetMasterObject(&key);
    if (!object)
      SetStatus(status, kStatusUnknownObject);
    if (object && status->m_second == kStatusOk)
    {
      ok = object->CollectDependencies(context, static_cast<ObjectKeyValueNode*>(node)->m_value, &dependencies);
      for (ObjectKeyNode* dep = dependencies.First(); dep; dep = NextKeyNode(dep))
      {
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
