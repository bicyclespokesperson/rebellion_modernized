#pragma once
#include <cstdint>

#include "gameobj/child_iterator.h"
#include "gameobj/game_obj.h"

// Sector (family 0x80-0x8f, ctor 0x5264b0, VT 0x65f0b0, secondary ObjectBase VT 0x65f0a8, 0x58 bytes): a sector of the
// galaxy map. It sits directly on GameObj, adds no fields and no virtuals (111 slots) and keeps its data in its
// SectorSD record (group, position).
//
// Overrides: 1 GetFamily, 5-9 (persistence, all forwarding to GameObj's bodies), 10 SetRecord (shared with
// Character), 16 (returns 1), 37 CreateViews (shared stub 0x5774f0) and 57 CreateViewObject.
class Sector : public GameObj
{
  public:
  Sector();           // 0x5264b0
  ~Sector() override; // 0x526510 (scalar deleting), 0x526530

  uint32_t GetFamily() override;             // 1, 0x526670: 0x80
  int Read(istream* in) override;            // 5, shared stub 0x569840
  int Write(ostream* out) override;          // 6, shared stub 0x569850
  int Checksum(Crc32* crc) override;         // 7, shared stub 0x569860
  int Validate() override;                   // 8, shared jump stub 0x569870
  int ApplyAll(NotifyContext* ctx) override; // 9, shared with GameRoot (0x513f20)
  int SetRecord(DatRecord* record) override; // 10, 0x50e1c0 (shared with Character)
  int ShouldRetainWhenDestroyed() override
  {
    return 1;
  } // 16, shared 0x40f340
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, shared stub 0x5774f0
  GameObj* CreateViewObject() override;                          // 57, 0x526600

  // The sector's group number (SectorSD::m_group); 1 without a record.
  int GetGroup(); // 0x526550
  // Stores the record's packed X (low 16 bits) and Y (high 16 bits) map position in *out; 0 without a record.
  void GetPosition(uint32_t* out); // 0x526560

  static GameObjNotifier* Create(); // 0x526590, the object factory of family 0x80
};

// Iterator over the children of a game object in the sector families (vtable 0x65f26c).
class SectorIterator : public ChildIterator
{
  public:
  SectorIterator(GameObjNotifier* parent, uint32_t stateFilter); // 0x5266a0
  ~SectorIterator() override;                                    // 0x526680 (deleting), 0x526720
};
