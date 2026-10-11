#pragma once
#include <cstdint>

#include "gameobj/game_obj_data.h"
#include "gameobj/object_key.h"
#include "gameobj/tracked_stat.h"
#include "gameobj/unit_obj_base.h"

class Crc32;

// The data block of Role objects (VT 0x66a0f0, 0x18 bytes, ctor 0x583df0): a UnitObjData without fields of its own
// that has its own vtable. Role::CreateData builds one; CharacterData derives from it.
class RoleData : public UnitObjData
{
public:
  RoleData(); // 0x583df0
  ~RoleData() override; // 0x583e10 (deleting), 0x583e30
  int Read(istream* in) override; // 0x583e40
  int Write(ostream* out) override; // 0x583e50
  int Checksum(Crc32* crc) override; // 0x583e60
  // Slot 4 (IsValid) is a jump to the shared "return 1" stub 0x40f340.
};

// The data block of Character master views (VT 0x661ac0, 0x38 bytes, ctor 0x540750). The three stats are raised as
// events 0x388-0x38a by OnHealingChanged and friends.
class CharacterData : public RoleData
{
public:
  CharacterData(); // 0x540750
  ~CharacterData() override; // 0x5407d0 (deleting), 0x5407f0
  int Read(istream* in) override; // 0x540870
  int Write(ostream* out) override; // 0x5408e0
  int Checksum(Crc32* crc) override; // 0x540950

  TrackedStat m_healingStat; // +0x18, event 0x388
  TrackedStat m_forceDiscoveryStat; // +0x20, event 0x389
  TrackedStat m_escapeStat; // +0x28, event 0x38a
  ObjectKey m_holderKey; // +0x30, reset whenever the State changes
  int m_field34; // +0x34
};
