#pragma once
#include "galaxy/mission_decl.h"

// Mission42 (family 0x42, VT 0x664388, secondary ObjectBase VT 0x664380, 0xa8 bytes): a mission type that sits in the
// galaxy-module address range (0x56a870). It adds no fields over its base. Slots 5-9 and 37 point at first-level
// forwarders (0x56da40, 0x56ebb0, 0x56da70, 0x56cc40, 0x576980, 0x5761b0) that are shared with three other mission
// types and belong to the mission module; the overrides below do the same work but are not hooked. The second-level
// forwarders 0x56aaa0/0x56aab0/0x56aac0 are this type's own.
class Mission42 : public MissionTypeBase
{
public:
  Mission42(); // 0x56a870
  ~Mission42() override; // 0x56a890 (deleting), 0x56a8b0

  uint32_t GetFamily() override; // 1, 0x56aa20: 0x42
  int Read(istream* in) override; // 5, shared 0x56da40
  int Write(ostream* out) override; // 6, shared 0x56ebb0
  int Checksum(Crc32* crc) override; // 7, shared 0x56da70
  int Validate() override; // 8, shared 0x56cc40
  int ApplyAll(NotifyContext* ctx) override; // 9, shared 0x576980
  int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x56a8d0
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x56a8f0
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, shared 0x5761b0
  GameObj* CreateViewObject() override; // 57, 0x56a9b0

  int ReadImpl(istream* in); // 0x56aaa0
  int ChecksumImpl(Crc32* crc); // 0x56aab0
  int ValidateImpl(); // 0x56aac0 (jump thunk to the mission base's Validate body 0x523c60)

  static int BuildEventKinds(); // 0x56aad0, g_mission42EventKinds at 0x6bb938
  static GameObjNotifier* Create(); // 0x56aa30, the object factory of family 0x42
};
