#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <ranges>

#include "core/entry_identity.h"
#include "core/game_string.h"
#include "core/stream_io.h"
#include "gameobj/game_obj_root.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"

// The object database maps ObjectKeys to ObjectEntries; an entry holds the (up to) three views of one object
// (view 0 = master, 1 = Alliance, 2 = Empire). Parents keep their children in a ChildList sorted by (family, id).
//
// Lookups come in two flavours: by key, and "first object of a family range" (FamilyRange = [first, last)). The
// `view` argument selects which copy of the object to return; with `resolve` set, an object that is hidden
// (IsUnderSideRoot) is skipped in favour of the next one in the range.

class ObjectDatabase;

// One database entry; the IdNode id is the key's id, m_family its family.
class ObjectEntry : public IdNode
{
  public:
  ObjectEntry();
  ~ObjectEntry() override;

  ObjectKey* GetKey(ObjectKey* out) const;
  // Stores `object` in the slot of its view side if the family matches and the slot is free; 1 if it was stored.
  int AddObject(GameObjNotifier* object);
  // Clears the slot if it holds `object`; 1 if it did.
  int RemoveObject(GameObjNotifier* object);
  GameObjNotifier* GetObject(int view) const;
  // All three slots are filled with objects that point back at this entry and are attached (have a parent) or are
  // the game root.
  int IsFullyLinked() const;
  // The next entry in key order if its family lies in `range`.

  int m_flag;                    // +0x1c, always 1
  uint32_t m_family;             // +0x20
  GameObjNotifier* m_objects[3]; // +0x24, indexed by view
};

// A parent's children, sorted by (family, id). m_ok is saved with the list.
class ChildList
{
  public:
  void Clear()
  {
    decltype(m_nodes) detached;
    m_nodes.swap(detached);
    m_nextId = 1;
  }
  size_t Count() const
  {
    return m_nodes.size();
  }
  bool Empty() const
  {
    return m_nodes.empty();
  }
  auto Entries() const
  {
    return std::views::values(m_nodes);
  }
  auto PositionOf(const GameObjNotifier* node) const
  {
    return std::ranges::find(Entries(), node, [](const auto& owned) {
      return owned.get();
    });
  }
  GameObjNotifier* First() const
  {
    return Empty() ? nullptr : m_nodes.begin()->second.get();
  }
  void Remove(GameObjNotifier* node);
  uint32_t AddWithId(GameObjNotifier* node);

  ChildList();
  ~ChildList();

  int Read(istream* in);
  int Write(ostream* out);
  // Unlinks `child` (matched by family and id); it is not deleted.
  int RemoveChild(GameObjNotifier* child);
  GameObjNotifier* Find(uint32_t family, uint32_t id) const;
  // Lowest child whose family is inside `range`.
  GameObjNotifier* FindFirstInRange(const FamilyRange* range) const;

  int m_ok; // +0x0c
  private:
  std::map<uint32_t, std::unique_ptr<GameObjNotifier>> m_nodes;
  uint32_t m_nextId = 1;
};

// The database itself (VT 0x66a220, 0x585b20-0x585f80): an ID registry of ObjectEntries ordered by key.
class ObjectDatabase
{
  public:
  void Clear()
  {
    decltype(m_nodes) detached;
    m_nodes.swap(detached);
    m_nextId = 1;
  }
  size_t Count() const
  {
    return m_nodes.size();
  }
  bool Empty() const
  {
    return m_nodes.empty();
  }
  auto Entries() const
  {
    return std::views::values(m_nodes);
  }
  auto PositionOf(const ObjectEntry* node) const
  {
    return std::ranges::find(Entries(), node, [](const auto& owned) {
      return owned.get();
    });
  }
  ObjectEntry* First() const
  {
    return Empty() ? nullptr : m_nodes.begin()->second.get();
  }
  void Remove(ObjectEntry* node);
  uint32_t AddWithId(ObjectEntry* node);

  ObjectDatabase();  // 0x585b20
  ~ObjectDatabase(); // 0: 0x585b40 (deleting), 0x585b60

  // Puts the object in the entry of its key (creating the entry); 0 for a null object.
  int AddObject(GameObjNotifier* object); // 0x585b70
  // Takes the object out of its entry and deletes the entry once it holds no object; 1 if the object was in it.
  int RemoveObject(GameObjNotifier* object);    // 0x585c70
  int DetachEntry(ObjectEntry* entry);          // 0x585d40: unlinks the node; 1 unless it is null
  int DeleteEntry(ObjectEntry* entry);          // 0x585d60
  int AreAllLinked();                           // 0x585d90
  ObjectEntry* FindEntry(const ObjectKey* key); // 0x585dc0
  // The entry with the lowest key whose family is the range's first family or, failing that, the next entry if it lies
  // in the range.
  ObjectEntry* FindEntryInRange(const FamilyRange* range); // 0x585e40

  int m_ok; // +0x0c
  private:
  std::map<uint32_t, std::unique_ptr<ObjectEntry>> m_nodes;
  uint32_t m_nextId = 1;
};

extern ObjectDatabase* g_objectDatabase; // 0x6b90e4
extern ObjectKeySet* g_unresolvedKeys;   // 0x6b90e8: keys whose lookup found no object, for debugging

// ---- lookups (view 0 = master copy) ------------------------------------------------------------------------

// Object named by `key` in the given view (1 or 2); null for any other view.
GameObjNotifier* GetObjectForView(const ObjectKey* key, int view);
GameObjNotifier* GetFirstObjectInRangeForView(const FamilyRange* range, int view, int resolve);
GameObjNotifier* GetMasterObject(const ObjectKey* key);
GameObjNotifier* GetFirstMasterObjectInRange(const FamilyRange* range);
GameObjNotifier* LookupObject(const ObjectKey* key, int view);
GameObjNotifier* FindObjectInRange(const FamilyRange* range, int view, int resolve);
// The next object after `object` (same view) in its family range, skipping hidden ones if `resolve`.
GameObjNotifier* FindNextObjectInRange(GameObjNotifier* object, const FamilyRange* range, int resolve);
// Creates an object for `key` from its data record (null if the key has no family or no record).
GameObjNotifier* CreateObjectForKey(const ObjectKey* key);

// Formats "(IDs=id,value;id,value...)" ("<none>" when empty) into *out. Returns 1.
int DescribeKeyValueSet(ObjectKeyValueSet* keys, std::string* out);
// For every key in `keys` asks its master object for the keys it depends on, collecting them in `collected`.
// *status becomes {1,-1} on success, {1,0x12} for an unknown key, {1,1} if an object refused.
int CollectKeyDependencies(void* context, ObjectKeyValueSet* keys, ObjectKeyValueSet* collected, StatusCode* status);

// ---- database and unresolved-key set lifetime ----------------------------------------------------------------

int RegisterObject(GameObjNotifier* object);
int UnregisterObject(GameObjNotifier* object);
void CreateObjectDatabase();
void DestroyObjectDatabase();
int CheckObjectDatabase();

int NoteUnresolvedKeyOf(GameObjNotifier* object);
void CreateUnresolvedKeySet();
void DestroyUnresolvedKeySet();
int LoadUnresolvedKeySet(istream* in);
int SaveUnresolvedKeySet(ostream* out);

// ---- placing objects in the tree ---------------------------------------------------------------------------

// Resets the object's flags, finds its parent (the game root for id 3, else the master side root) and attaches it.
int AttachMasterObject(GameObjNotifier* object);
// Like AttachMasterObject for a side view: copies id and record from `master` and uses `view`'s tree.
int AttachSideObject(GameObjNotifier* object, GameObjNotifier* master, int view);
int DetachObject(GameObjNotifier* object);
