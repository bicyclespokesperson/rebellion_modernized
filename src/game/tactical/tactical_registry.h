#pragma once
#include <cstdint>
#include <map>
#include <memory>

#include "core/entry_identity.h"
#include "tactical/tactical_units.h"

// The tactical registry (singleton DAT_006bc470, 0x48 bytes, created by GetTacticalRegistry, 0x596bd0): the battle's
// units by the handle the strategic game knows them by. Capital ships and fighters (class TacticalShipBase) are looked up through
// a TacticalUnit sub-object at +0xd4 of the unit, which is the node the registry holds; the groups of fighters and of
// capital ships are the nodes themselves (their kind, TacticalUnit::GetUnitKind, is 3 for a fighter group and 4 for a
// capital-ship group; characters and special forces, kind 7, are kept by the simulator instead).

class TacticalShipBase;
class TacticalHangar;
class FighterGroup;
class CapitalShipGroup;

class TacticalRegistry
{
public:
  TacticalRegistry(); // 0x5a6290
  virtual ~TacticalRegistry(); // 0x5a6590 (scalar deleting destructor 0x5a6320)

  // Next free handle on a side (0 or 1): side * 1,000,000 + a running count. Returns 0 for any other side.
  uint32_t NextUnitHandle(unsigned side); // 0x5a6620
  uint32_t NextGroupHandle(unsigned side); // 0x5a6640

  TacticalShipBase* FindShip(int handle); // 0x5a6340 capital ship or fighter
  TacticalHangar* FindLaunchInterface(int handle); // 0x5a6360 a carrier's launch interface, or the Death Star's
  IdNode* FindShipOrViewGroup(int handle); // 0x5a63e0
  IdNode* FindViewGroup(int handle); // 0x5a6450
  IdNode* FindFighterGroup(int handle); // 0x5a6460
  IdNode* FindCapitalShipGroup(int handle); // 0x5a6470
  IdNode* FindShipGroup(int handle); // 0x5a6480 a fighter group, else a capital-ship group
  IdNode* RemoveShipGroup(int handle); // 0x5a64b0 detaches (does not delete)
  uint32_t AddShip(TacticalShipBase* ship); // 0x5a64e0
  uint32_t AddViewGroup(TacticalUnit* group); // 0x5a6510
  uint32_t AddFighterGroup(FighterGroup* group); // 0x5a6520
  uint32_t AddCapitalShipGroup(CapitalShipGroup* group); // 0x5a6530
  uint32_t AddShipGroup(TacticalUnit* group); // 0x5a6540 by the group's kind (3 fighters, 4 capital ships)
  TacticalUnit* GetDeathStar(); // 0x5a66a0

  uint32_t m_handleCounter[2]; // +0x04 last handle given per side
  uint32_t m_groupCounter[2]; // +0x0c last group id given per side
  std::map<uint32_t, std::unique_ptr<TacticalShipBase>> m_ships; // keyed by tactical handle
  std::map<uint32_t, std::unique_ptr<TacticalUnit>> m_viewGroups; // groups of the 3D view
  std::map<uint32_t, std::unique_ptr<FighterGroup>> m_fighterGroups;
  std::map<uint32_t, std::unique_ptr<CapitalShipGroup>> m_capitalGroups;
  uint32_t m_nextShipId = 1;
  uint32_t m_nextViewGroupId = 1;
  uint32_t m_nextFighterGroupId = 1;
  uint32_t m_nextCapitalGroupId = 1;
  TacticalUnit* m_deathStar; // +0x44

private:
  bool m_clearing = false;
};

// The kind a TacticalUnit reports (its second virtual): 3 fighter group, 4 capital-ship group, 7 character / special force.
int TacticalUnitKind(TacticalUnit* unit);
