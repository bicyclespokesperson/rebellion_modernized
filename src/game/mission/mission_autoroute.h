#pragma once
#include "galaxy/mission_decl.h"
#include "mission/mission_leaf.h"

// MissionAutoroute (family 0x43, ctor 0x576000, VT 0x668850, secondary ObjectBase VT 0x668848, 0xa8 bytes): a plain
// mission type on MissionTypeBase without fields, built like Mission42 (galaxy/mission_type42.h). Slots 5-9 are the
// forwarders shared with Mission42; slot 37 is the shared 0x5761b0.
class MissionAutoroute : public MissionTypeBase
{
  public:
  MissionAutoroute();           // 0x576000
  ~MissionAutoroute() override; // 0x576020 (deleting), 0x576040

  uint32_t GetFamily() override;                                  // 1, 0x5761d0: 0x43
  int Read(istream* in) override;                                 // 5, shared 0x56da40
  int Write(ostream* out) override;                               // 6, shared 0x56ebb0
  int Checksum(Crc32* crc) override;                              // 7, shared 0x56da70
  int Validate() override;                                        // 8, shared 0x56cc40
  int ApplyAll(NotifyContext* ctx) override;                      // 9, shared 0x576980
  int GetEventKinds(int unused, EventKindSet* out) override;      // 15, 0x576060
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x576080
  int CreateViews(int controlKind, NotifyContext* ctx) override;  // 37, shared 0x5761b0
  GameObj* CreateViewObject() override;                           // 57, 0x576140

  static int BuildEventKinds();     // 0x576270
  static GameObjNotifier* Create(); // 0x5761e0
};
