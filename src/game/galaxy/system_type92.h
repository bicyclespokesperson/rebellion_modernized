#pragma once
#include "galaxy/system_type90.h"

// SystemType92 (family 0x92, VT 0x664098, secondary ObjectBase VT 0x664090, 0x8c bytes, no fields of its own): the
// other leaf of System. Its InitializeNewObject starts the system populated and explored for the Alliance side (control side 1)
// and for the special system with id 0x121, and otherwise rolls the populated state (GNPRTB rules 0x559970-0x559a60).
// Slots 5-8 and 37 are shared with SystemType90.
class SystemType92 : public System
{
public:
  SystemType92(); // 0x56a430
  ~SystemType92() override; // 0x56a450 (deleting), 0x56a470

  uint32_t GetFamily() override; // 1, 0x56a490: 0x92
  int Read(istream* in) override; // 5, shared 0x566ce0
  int Write(ostream* out) override; // 6, shared 0x566cf0
  int Checksum(Crc32* crc) override; // 7, shared 0x566d20
  int Validate() override; // 8, shared 0x566d30
  int ApplyAll(NotifyContext* ctx) override; // 9, 0x56a530
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, 0x56a510
  int InitializeNewObject(intptr_t a) override; // 47, 0x56a550
  GameObj* CreateViewObject() override; // 57, 0x56a800
  int CreateStartingUnits(NotifyContext* ctx) override; // 115, 0x56a6f0

  int CreateViewsImpl(int controlKind, NotifyContext* ctx); // 0x56a7d0
  int ApplyAllImpl(NotifyContext* ctx); // 0x56a7f0

  static GameObjNotifier* Create(); // 0x56a4a0, the object factory of family 0x92
};
