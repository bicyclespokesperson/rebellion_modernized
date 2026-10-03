#pragma once
#include <cstddef>
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
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

class String;
class ObjectDatabase;

// One database entry; the IdNode id is the key's id, m_family its family.
class ObjectEntry : public IdNode {
public:
    ObjectEntry();
    virtual ~ObjectEntry();

    ObjectKey* GetKey(ObjectKey* out) const;
    // Stores `object` in the slot of its view side if the family matches and the slot is free.
    void AddObject(GameObjNotifier* object);
    void RemoveObject(GameObjNotifier* object);
    GameObjNotifier* GetObject(int view) const;
    // All three slots are filled with objects that point back at this entry and are attached (have a parent) or are
    // the game root.
    int IsFullyLinked() const;
    // The next entry in key order if its family lies in `range`.
    ObjectEntry* NextInRange(const FamilyRange* range) const;

    int m_flag;                    // +0x1c, always 1
    uint32_t m_family;             // +0x20
    GameObjNotifier* m_objects[3];  // +0x24, indexed by view
};

// A parent's children, sorted by (family, id). m_ok is saved with the list.
class ChildList : public IdAvlTree {
public:
    ChildList();
    virtual ~ChildList();
    virtual int IsLess(AvlNode* a, AvlNode* b) override;

    int Read(istream* in);
    int Write(ostream* out);
    // Unlinks `child` (matched by family and id); it is not deleted.
    bool RemoveChild(GameObjNotifier* child);
    GameObjNotifier* Find(uint32_t family, uint32_t id) const;
    // Lowest child whose family is inside `range`.
    GameObjNotifier* FindFirstInRange(const FamilyRange* range) const;

    int m_ok;  // +0x0c
};

// The database itself (0x585b20, owned by the timer/event module): an IdAvlTree of ObjectEntries. Only its layout is
// declared here; its methods are called through the FUN_ declarations in object_database.cpp.
class ObjectDatabase : public IdAvlTree {
public:
    virtual ~ObjectDatabase();

    int m_ok;  // +0x0c
};

extern ObjectDatabase* g_objectDatabase;  // 0x6b90e4
extern ObjectKeySet* g_unresolvedKeys;    // 0x6b90e8: keys whose lookup found no object, for debugging

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
int DescribeKeyValueSet(ObjectKeyValueSet* keys, String* out);
// For every key in `keys` asks its master object for the keys it depends on, collecting them in `collected`.
// *status becomes {1,-1} on success, {1,0x12} for an unknown key, {1,1} if an object refused.
int CollectKeyDependencies(void* context, ObjectKeyValueSet* keys, ObjectKeyValueSet* collected, StatusCode* status);

// ---- database and unresolved-key set lifetime ----------------------------------------------------------------

bool RegisterObject(GameObjNotifier* object);
bool UnregisterObject(GameObjNotifier* object);
void CreateObjectDatabase();
void DestroyObjectDatabase();
void CheckObjectDatabase();

bool NoteUnresolvedKeyOf(GameObjNotifier* object);
void CreateUnresolvedKeySet();
void DestroyUnresolvedKeySet();
bool LoadUnresolvedKeySet(istream* in);
bool SaveUnresolvedKeySet(ostream* out);

// ---- placing objects in the tree ---------------------------------------------------------------------------

// Resets the object's flags, finds its parent (the game root for id 3, else the master side root) and attaches it.
int AttachMasterObject(GameObjNotifier* object);
// Like AttachMasterObject for a side view: copies id and record from `master` and uses `view`'s tree.
int AttachSideObject(GameObjNotifier* object, GameObjNotifier* master, int view);
bool DetachObject(GameObjNotifier* object);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ObjectEntry, 0x30);
REB_ASSERT_OFFSET(ObjectEntry, m_family, 0x20);
REB_ASSERT_OFFSET(ObjectEntry, m_objects, 0x24);
REB_ASSERT_SIZE(ChildList, 0x10);
REB_ASSERT_SIZE(ObjectDatabase, 0x10);
#pragma clang diagnostic pop
