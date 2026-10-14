#include "character/character.h"
#include "data/dat_record.h"
#include "data/sd_records.h"
#include "economy/manu_mgr.h"
#include "fleet/combat_unit.h"
#include "fleet/fleet.h"
#include "galaxy/system.h"
#include "gameobj/object_family.h"

// Non-virtual accessors and child counters of System. The per-family iterators of the original (FleetIterator, ...)
// belong to other modules; plain ChildIterators with the same family ranges are used here.

namespace
{

constexpr uint32_t kDestroyedStateFlag = 0x8;
constexpr uint32_t kFleetFlag02 = 0x2;
constexpr uint32_t kFleetFlag04 = 0x4;
constexpr uint32_t kSystemDataFlagBit0 = 0x1;
constexpr uint32_t kSystemDataFlagBit3 = 0x8;
constexpr uint32_t kSideAlliance = 1;
constexpr uint32_t kSideEmpire = 2;
constexpr uint32_t kSideNone = 3;
constexpr uint32_t kStateUsable = 1;
constexpr uint32_t kStateExisting = 3;
constexpr uint32_t kStateAlways = 4;
constexpr uint32_t kRegimentKeyCounted = 0x10000006;
constexpr FamilyRange kDeathStars(0x18, 0x1c);

bool isSideOneToThree(int side)
{
  return side >= 1 && side <= 3;
}
bool isSideOneOrTwo(int side)
{
  return isSideOneToThree(side) && side != static_cast<int>(kSideNone);
}

int sideWord(const int (&words)[3], int side)
{
  return isSideOneToThree(side) ? words[side - 1] : 0;
}

int countChildren(System* system, uint32_t side, const FamilyRange* range, uint32_t stateFilter)
{
  ChildIterator it(system, side, range, stateFilter);
  return it.Count();
}

int countChildrenOfFleets(System* system, uint32_t side, uint32_t stateFilter, const FamilyRange* range)
{
  int total = 0;
  ChildIterator fleets(system, side, &family::kFleets, stateFilter);
  for (fleets.Begin(); fleets.Current(); fleets.Advance())
  {
    ChildIterator ships(fleets.Current(), side, range, stateFilter);
    total += ships.Count();
  }
  return total;
}

uint32_t recordKeyOf(GameObjNotifier* object)
{
  const DatRecord* record = object->m_record;
  return record ? (record->m_third << 24) | (record->m_id & 0xffffff) : 0;
}

// Keeps the higher-valued object in *best: the candidate replaces it unless the current one is at least as good.
template <uint32_t (CombatUnit::*Value)()>
void keepBetter(GameObjNotifier** best, GameObjNotifier* candidate)
{
  if (*best)
  {
    const int current = static_cast<int>((static_cast<CombatUnit*>(*best)->*Value)());
    const int offered = static_cast<int>((static_cast<CombatUnit*>(candidate)->*Value)());
    if (current >= offered)
      return;
  }
  *best = candidate;
}

int findFleetWithFlag(System* system, uint32_t flag, uint32_t side, GameObjNotifier** out)
{
  *out = nullptr;
  ChildIterator fleets(system, side, &family::kFleets, kStateAlways);
  for (fleets.Begin(); fleets.Current() && !*out; fleets.Advance())
  {
    auto* fleet = static_cast<Fleet*>(fleets.Current());
    if (!(fleet->m_stateFlags & kDestroyedStateFlag) && (fleet->m_fleetFlags & flag))
      *out = fleet;
  }
  return 1;
}

} // namespace

int System::GetLoyaltyFor(int side)
{
  return side == static_cast<int>(kSideAlliance) ? m_loyalty : 100 - m_loyalty;
}

int System::SetLoyaltyFor(int value, int side, NotifyContext* ctx)
{
  return SetLoyalty(side == static_cast<int>(kSideAlliance) ? value : 100 - value, ctx);
}

int System::CountFighterSquadrons(uint32_t side, uint32_t stateFilter)
{
  return countChildren(this, side, &family::kFighterSquadrons, stateFilter);
}

int System::CountTroopRegiments(uint32_t side, uint32_t stateFilter)
{
  return countChildren(this, side, &family::kTroopRegiments, stateFilter);
}

int System::CountCapitalShips(uint32_t side, uint32_t stateFilter)
{
  return countChildrenOfFleets(this, side, stateFilter, &family::kCapitalShips);
}

int System::CountDeathStars(uint32_t side, uint32_t stateFilter)
{
  return countChildrenOfFleets(this, side, stateFilter, &kDeathStars);
}

int System::CountEmpireRegimentsOfKey6()
{
  int count = 0;
  ChildIterator regiments(this, kSideEmpire, &family::kTroopRegiments, kStateUsable);
  for (regiments.Begin(); regiments.Current(); regiments.Advance())
    count += recordKeyOf(regiments.Current()) == kRegimentKeyCounted;
  return count;
}

TrackedStat* System::GetStat0()
{
  return m_data ? &GetData()->m_stats[0] : nullptr;
}
TrackedStat* System::GetStat3()
{
  return m_data ? &GetData()->m_stats[3] : nullptr;
}
TrackedStat* System::GetStat4()
{
  return m_data ? &GetData()->m_stats[4] : nullptr;
}
TrackedStat* System::GetStat5()
{
  return m_data ? &GetData()->m_stats[5] : nullptr;
}

uint32_t System::GetDataFlagBit0()
{
  return m_data ? GetData()->m_flags80 & kSystemDataFlagBit0 : 0;
}
uint32_t System::GetDataFlagBit3()
{
  return m_data ? GetData()->m_flags80 & kSystemDataFlagBit3 : 1;
}

int System::FindCharacterByCommandKind(int kind, GameObjNotifier** out)
{
  *out = nullptr;
  CharacterIterator characters(this, kStateExisting, ControlSide());
  for (characters.Begin(); characters.Current() && !*out; characters.Advance())
  {
    auto* character = static_cast<Character*>(characters.Current());
    if (character->m_commandKind == kind)
      *out = character;
  }
  return 1;
}

int System::FindCharacterUnused(GameObjNotifier** out, int, int)
{
  *out = nullptr;
  CharacterIterator characters(this, kStateExisting);
  for (characters.Begin(); characters.Current(); characters.Advance())
  {
  }
  return 1;
}

int System::FindBestShipOfCommandKind2(uint32_t side, GameObjNotifier** out)
{
  int found = 1;
  *out = nullptr;
  ChildIterator fleets(this, side, &family::kFleets, kStateUsable);
  for (fleets.Begin(); fleets.Current(); fleets.Advance())
  {
    GameObjNotifier* ship = nullptr;
    found = static_cast<Fleet*>(fleets.Current())->FindShipByNumber(2, &ship);
    if (found && ship)
      keepBetter<&CombatUnit::GetGridTotal>(out, ship);
    if (!found)
      break;
  }
  return found;
}

int System::FindBestShipOfCommandKind1(uint32_t side, GameObjNotifier** out)
{
  int found = FindCharacterByCommandKind(1, out);
  ChildIterator fleets(this, side, &family::kFleets, kStateUsable);
  for (fleets.Begin(); found && fleets.Current(); fleets.Advance())
  {
    GameObjNotifier* ship = nullptr;
    found = static_cast<Fleet*>(fleets.Current())->FindShipByNumber(1, &ship);
    if (found && ship)
      keepBetter<&CombatUnit::GetField70>(out, ship);
  }
  return found;
}

uint32_t System::GetRecordField44()
{
  return m_record ? static_cast<uint32_t>(static_cast<SystemSD*>(m_record)->m_pictureId) : 0;
}

uint32_t* System::GetRecordField4c(uint32_t* out)
{
  if (m_record)
  {
    const SystemSD* record = static_cast<SystemSD*>(m_record);
    *out = static_cast<uint32_t>(static_cast<uint16_t>(record->m_xPosition)) |
           (static_cast<uint32_t>(static_cast<uint16_t>(record->m_yPosition)) << 16);
  }
  else
  {
    *out = 0;
  }
  return out;
}

int System::GetEnergy()
{
  return m_energy;
}

int System::GetRawMaterial()
{
  return m_rawMaterial;
}

GameObjNotifier* System::FindManufacturingManagerByKind(int value)
{
  ChildIterator managers(this, &family::kManufacturingManagers, kStateAlways);
  for (managers.Begin(); managers.Current(); managers.Advance())
    if (static_cast<ManuMgr*>(managers.Current())->GetFacilityKind() == value)
      break;
  return managers.Current();
}

int System::HasFleetsOrFighterSquadrons(uint32_t side)
{
  const int fleets = ChildIterator(this, side, &family::kFleets, kStateUsable).HasCurrent();
  const int squadrons = ChildIterator(this, side, &family::kFighterSquadrons, kStateUsable).HasCurrent();
  return fleets || squadrons ? 1 : 0;
}

int System::FindFleetWithFlag02(uint32_t side, GameObjNotifier** out)
{
  return findFleetWithFlag(this, kFleetFlag02, side, out);
}

int System::FindFleetWithFlag04(uint32_t side, GameObjNotifier** out)
{
  return findFleetWithFlag(this, kFleetFlag04, side, out);
}

int System::GetBlockadingSide(uint32_t* out)
{
  *out = kSideNone;
  int found = (m_flags & system_flag::kBlockade) ? 1 : 0;
  if (found)
  {
    ChildIterator fleets(this, &family::kFleets, kStateUsable);
    fleets.Begin();
    found = fleets.Current() != nullptr;
    if (found)
      *out = fleets.Current()->ControlSide();
  }
  return found;
}

int System::GetMoreLoyalSide()
{
  if (GetLoyaltyFor(kSideAlliance) > GetLoyaltyFor(kSideEmpire))
    return 1;
  return GetLoyaltyFor(kSideEmpire) <= GetLoyaltyFor(kSideAlliance) ? 3 : 2;
}

int System::IsBlockaderMoreLoyal(uint32_t* out)
{
  uint32_t blockader;
  *out = 0;
  const int found = GetBlockadingSide(&blockader);
  if (found)
    *out = static_cast<uint32_t>(GetMoreLoyalSide()) == blockader;
  return found;
}

int System::GetDataWord48(int side)
{
  return sideWord(GetData()->m_word48, side);
}
int System::GetDataWord54(int side)
{
  return sideWord(GetData()->m_word54, side);
}
int System::GetDataWord68(int side)
{
  return sideWord(GetData()->m_word68, side);
}

int System::SumWords48And54(int side)
{
  int total = 0;
  if (!isSideOneOrTwo(side))
    return total;
  const int owner = GetDataWord60();
  if (!isSideOneOrTwo(owner))
    return total;
  total = GetDataWord48(side) + GetDataWord54(side);
  if (owner != side)
    total = GetDataWord54(kSideNone) + total + GetDataWord48(kSideNone);
  return total;
}

int System::GetDataWord60()
{
  return GetData()->m_word60;
}
int System::GetDataWord64()
{
  return m_data ? GetData()->m_word64 : 0;
}
int System::GetDataWord74()
{
  return m_data ? GetData()->m_word74 : 0;
}
int System::GetDataWord78()
{
  return m_data ? GetData()->m_word78 : 0;
}
int System::GetDataWord7c()
{
  return m_data ? GetData()->m_word7c : 0;
}
