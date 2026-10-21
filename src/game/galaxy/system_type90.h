#pragma once
#include "galaxy/system.h"
#include "gameobj/child_iterator.h"

// SystemType90 (family 0x90, VT 0x663698, secondary ObjectBase VT 0x663694, 0x8c bytes, no fields of its own): the
// ordinary star system. Over System it implements the pure slots (persistence slots 5-8 are second-level forwarders
// shared with SystemType92) and starts its properties with the "populated" defaults (InitializeNewObject rolls them:
// loyalty, energy, raw material from the GNPRTB rules at 0x559670-0x559700) and CreateStartingUnits creates the
// system's facilities. Each OnXChanged override below forwards to System and then pushes the new value to both side
// views.
class SystemType90 : public System
{
  public:
  SystemType90();           // 0x566b90
  ~SystemType90() override; // 0x566bb0 (deleting), 0x566bd0

  uint32_t GetFamily() override;                                           // 1, 0x566bf0: 0x90
  int Read(istream* in) override;                                          // 5, 0x566ce0
  int Write(ostream* out) override;                                        // 6, 0x566cf0
  int Checksum(Crc32* crc) override;                                       // 7, 0x566d20
  int Validate() override;                                                 // 8, 0x566d30 (jump thunk)
  int ApplyAll(NotifyContext* ctx) override;                               // 9, 0x566d00
  int CreateViews(int controlKind, NotifyContext* ctx) override;           // 37, shared 0x56a510 (SystemType92)
  int InitializeNewObject(intptr_t a) override;                            // 47, 0x566d40
  int ApplyAllToViewOf(int view, int unused, NotifyContext* ctx) override; // 54, 0x566eb0
  GameObj* CreateViewObject() override;                                    // 57, 0x566fd0
  int OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) override; // 60, 0x567040
  int CreateStartingUnits(NotifyContext* ctx) override;                                      // 115, 0x566de0
  int OnLoyaltyChanged(int oldValue, int newValue, NotifyContext* ctx) override;             // 116, 0x567110
  int OnPopulatedChanged(int newValue, NotifyContext* ctx) override;                         // 131, 0x567270
  int OnUprisingChanged(int newValue, NotifyContext* ctx) override;                          // 133, 0x5671c0

  // Bodies of the persistence slots (the slots forward to these, which forward to System's bodies).
  int ReadImpl(istream* in);            // 0x566f60
  int WriteImpl(ostream* out);          // 0x566f70
  int ApplyAllImpl(NotifyContext* ctx); // 0x566f80
  int ChecksumImpl(Crc32* crc);         // 0x566fb0
  int ValidateImpl();                   // 0x566fc0 (jump thunk to System's Validate body 0x50e820)

  static GameObjNotifier* Create(); // 0x566c70, the object factory of family 0x90
};

// Iterator over the children of a game object in family 0x90-0x91 (vtable 0x663980).
class SystemType90Iterator : public ChildIterator
{
  public:
  SystemType90Iterator(GameObjNotifier* parent, uint32_t stateFilter); // 0x567340
  ~SystemType90Iterator() override;                                    // 0x567320 (deleting), 0x5673c0
};

// Applies the children of families 0x90-0x91 of `parent` (GameObj::UpdateChildren over that range), 0x566c00.
int UpdateSystemType90Children(GameObj* parent, NotifyContext* ctx);

// A side view (1 or 2) of a system, as the OnXChanged events push values into them.
inline System* SystemView(System* system, int view)
{
  return static_cast<System*>(system->GetViewObject(view));
}
