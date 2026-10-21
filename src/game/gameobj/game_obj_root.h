#pragma once
#include <cstddef>
#include <cstdint>

#include "core/entry_identity.h"
#include "core/stream_io.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"

// The two bottom layers of every game object (GameObj, 0x4f5ea0, is the third):
//
//   GameObjRoot      IdNode + parent link + database entry + packed flags; knows only its id and family
//   GameObjNotifier  adds the child tree and the data-table record (the base class the setter->notify code works on)
//
// Game objects form a tree (game root family 0xf1, side roots family 0xf2, everything else below them). Every object
// exists up to three times, one per "view" (0 = master, 1 and 2 = the Alliance's and the Empire's copy, selected by
// the object's ViewSide flag bits); the ObjectEntry that the object database keeps per key links the views together.
//
// The 8 pure virtuals of GameObjRoot and the 3 more of GameObjNotifier are filled in by GameObj and its subclasses.

class ChildList;
class NotifyContext;
class DatRecord;
class GameObjNotifier;
class ObjectEntry;
class Crc32;

class GameObjRoot : public IdNode
{
  public:
  GameObjRoot();
  ~GameObjRoot() override;
  // Family: top byte of the object's key (object_family.h).
  virtual uint32_t GetFamily() = 0;
  virtual int IncludeInChecksum() = 0;
  virtual int GetOwnerKey(ObjectKey* out) = 0;
  virtual int IsUnderSideRoot() = 0;
  virtual int Read(istream* in) = 0;
  virtual int Write(ostream* out) = 0;
  virtual int Checksum(Crc32* crc) = 0;
  // True if the object is internally consistent (GameObj 0x4f9700).
  virtual int Validate() = 0;

  int GetFlag31() const;
  void SetFlag31(int on);
  int GetParentKey(ObjectKey* out);
  int GetFlag8() const;
  // True if `side` equals the object's side mask, or either is 3 (both sides).
  int MatchesSideMask(uint32_t side) const;
  int ReadFields(istream* in);
  int WriteFields(ostream* out);
  int ChecksumFields(Crc32* crc);
  void SetSideMask(uint32_t mask);
  void SetViewSide(uint32_t side);
  void SetControlSide(uint32_t side);
  void SetFlag8(uint32_t on);
  // An object still carrying the default id 1 asks the game for a real one.
  void ApplyDefaultId();
  // True unless the object is the game root (family 0xf1).
  int IsNotGameRoot();

  uint32_t ViewSide() const
  {
    return (m_flags >> 4) & 3;
  }
  uint32_t ControlSide() const
  {
    return (m_flags >> 6) & 3;
  }
  ObjectKey* GetKey(ObjectKey* out)
  {
    out->m_value = (GetFamily() << 24) | (m_id & 0xffffff);
    return out;
  }

  // Creates an object of the given family through the registered factory (null if none is registered).
  static GameObjNotifier* CreateObjectOfFamily(uint32_t family);
  // Registers a factory for `family` (ignored if there is one already or the table is full).
  static int RegisterFactory(uint32_t family, GameObjNotifier* (*factory)());
  template <class T>
  static int RegisterFactory(uint32_t family, T* (*factory)())
  {
    return RegisterFactory(family, reinterpret_cast<GameObjNotifier* (*)()>(factory));
  }

  GameObjNotifier* m_parent; // +0x1c
  ObjectEntry* m_entry;      // +0x20, set by ObjectEntry::AddObject
  // bits 0-3 side mask, 4-5 view side, 6-7 control side, 8 flag8, 31 flag31
  uint32_t m_flags; // +0x24
};

class GameObjNotifier : public GameObjRoot
{
  public:
  GameObjNotifier();
  ~GameObjNotifier() override;
  // Applies every property of the master to this side view (GameObj 0x4f9540).
  virtual int ApplyAll(NotifyContext* ctx) = 0;
  // 1 if the record was stored (a null record or one of this object's family), as AssignRecord says.
  virtual int SetRecord(DatRecord* record);
  // Fills `out` with the keys of the objects this one depends on (the default implementation: just itself).
  virtual int CollectDependencies(void* context, int value, ObjectKeyValueSet* out) = 0;

  GameObjNotifier* FirstChild();
  GameObjNotifier* FindFirstChildInRange(const FamilyRange* range);
  // The next sibling in key order if its family is inside `range`.
  GameObjNotifier* NextSiblingInRange(const FamilyRange* range);
  uint32_t GetRecordId();
  GameObjNotifier* GetViewObject(int view);
  int HasAllViews();
  // View 0 (the master copy) / views 1 and 2.
  int IsMasterView() const;
  int IsSideView() const;
  int HasRecord() const;
  int CanAdopt(GameObjNotifier* child);
  int ReadObject(istream* in);
  int WriteObject(ostream* out);
  int ChecksumObject(Crc32* crc);
  // True if the object has a record and all three views.
  int NotifyIfComplete();
  int ReadTree(istream* in);
  int WriteTree(ostream* out);
  int ChecksumTree(Crc32* crc);
  int AssignRecord(DatRecord* record);
  int AddChild(GameObjNotifier* child, GameObjNotifier** previousParent);
  static int RemoveFromParent(GameObjNotifier* child);

  ChildList* m_children; // +0x28
  DatRecord* m_record;   // +0x2c
};

// Edits `mask` bits of *flags to `value`'s, writing only if they differ. Returns 1 if the bits changed.
int UpdateFlagBits(uint32_t mask, uint32_t value, uint32_t* flags);
int UpdateFlagBit(uint32_t mask, int on, uint32_t* flags);
