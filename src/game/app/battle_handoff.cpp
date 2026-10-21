#include "app/battle_handoff.h"
#include "app/battle_display_names.h"
#include "app/battle_flow.h"
#include "gameobj/game_root.h"

#include <algorithm>
#include <new>
#include <set>

#include "character/character.h"
#include "character/special_forces.h"
#include "data/dat_record.h"
#include "economy/defense_facility.h"
#include "economy/manu_mgr.h"
#include "events/game_clock_hooks.h"
#include "fleet/capship.h"
#include "fleet/capship_subclasses.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "gameobj/object_queries.h"
#include "gameobj/object_lookup.h"
#include "strategy_ui/game_ui_facade.h"

// The strategic -> tactical hand-over (0x40afa0-0x40c730): BattleSetup walks the battle's system, describes every
// ship, squadron, character and ground unit in a BattleSnapshot, loads it into the battle engine and remembers each
// returned handle in the strategic object's data block.


namespace
{

constexpr uint32_t kBothSidesFilter = 3;
constexpr uint32_t kUsableFilter = 1;
constexpr uint32_t kFirstShipFamily = 0x14;
constexpr uint32_t kEndShipFamily = 0x1c;
constexpr uint32_t kFirstDeathStarFamily = 0x18;
constexpr uint32_t kFirstMajorCharacterFamily = 0x30;
constexpr uint32_t kEndMajorCharacterFamily = 0x38;
constexpr uint32_t kSideControlMask = 0xc0;
constexpr uint32_t kSide1Control = 0x40;
constexpr uint32_t kSide2Control = 0x80;
constexpr uint32_t kCompletedFlag = 0x4;
constexpr int kFirstManufacturingSlot = 0;

bool inFamilyRange(uint32_t family, uint32_t first, uint32_t end)
{
  return family >= first && family < end;
}

struct SideQuery {
  uint32_t m_a = 0;
  uint32_t m_b = 0;
  int m_c = 0;
  uint32_t m_d = 0;
};

} // namespace

// ---- small accessors the conversion uses
// -----------------------------------------------------------------------------------

int NotAlliance(int side)
{
  return side != 1;
} // 0x40d660

uint32_t KeyFamilyOf(const ObjectKey* key)
{
  return key->m_value >> 24;
} // 0x40c490

uint32_t KeyIdOf(const ObjectKey* key)
{
  return key->m_value & 0xffffff;
} // 0x40c4a0

uint32_t ObjectControlSide(GameObjRoot* object)
{
  return object->ControlSide();
} // 0x40d690

uint32_t ObjectDestroyedBit(GameObj* object)
{
  return (object->m_stateFlags >> 3) & 1;
} // 0x40d6a0

uint32_t CapShipShieldRechargeCH(CapShip* ship)
{
  return ship->GetCriticalHit(CapShip::kShieldRechargeShift);
} // 0x40d6b0

uint32_t CapShipWeaponRechargeCH(CapShip* ship)
{
  return ship->GetCriticalHit(CapShip::kWeaponRechargeShift);
} // 0x40d6c0

uint32_t CapShipTractorBeamCH(CapShip* ship)
{
  return ship->GetCriticalHit(CapShip::kTractorBeamShift);
} // 0x40d6d0

uint32_t CapShipSpeedCH(CapShip* ship)
{
  return ship->GetCriticalHit(CapShip::kSpeedShift);
} // 0x40d6e0

uint32_t CapShipPrimaryHyperdriveCH(CapShip* ship)
{
  return ship->GetCriticalHit(CapShip::kPrimaryHyperdriveShift);
} // 0x40d6f0

uint32_t CapShipBackupHyperdriveCH(CapShip* ship)
{
  return ship->GetCriticalHit(CapShip::kBackupHyperdriveShift);
} // 0x40d700

int CapShipCurrentHull(CapShip* ship)
{
  return ship->GetHull() - ship->GetHullValueDamage();
} // 0x40d710

int FightSquadCurrentSize(FightSquad* squad)
{
  return squad->GetSquadSize() - squad->GetSquadSizeDamage();
} // 0x40d730

uint32_t SystemBattleControlKind(System* system)
{
  return ControlBitsField(system->m_controlBits, 6);
} // 0x40d750

ObjectKey* ManufacturingManagerProductKey(ManuMgr* manager, ObjectKey* out)
{ // 0x40d760
  new (out) ObjectKey(manager->m_productKey);
  return out;
}

// ---- BattleSetup
// ------------------------------------------------------------------------------------------------------------------

namespace
{

struct ShipTypeIndex {
  int m_index;
  int m_value;
};

constexpr ShipTypeIndex kShipTypeIndices[38] = {
    {0, 0x740}, {1, 0x741}, {2, 0x742}, {3, 0x743}, {4, 0x744}, {5, 0x745},
    {6, 0x746}, {7, 0x747}, {8, 0x748}, {9, 0x749}, {10, 0x74a}, {11, 0x74d},
    {12, 0x74b}, {13, 0x74c}, {14, 0x74e}, {15, 0x780}, {16, 0x781}, {17, 0x782},
    {18, 0x783}, {19, 0x784}, {20, 0x785}, {21, 0x786}, {22, 0x787}, {37, 0x788},
    {23, 0x789}, {24, 0x78a}, {25, 0x78b}, {26, 0x78c}, {27, 0x78d}, {28, 0x78e},
    {29, 0x642}, {30, 0x643}, {31, 0x640}, {32, 0x641}, {33, 0x680}, {34, 0x681},
    {35, 0x682}, {36, 0x683}};

int32_t ShipTypeIndexOf(uint32_t typeBits)
{
  const auto& entries = kShipTypeIndices;
  const auto found = std::ranges::find(entries, static_cast<int>(typeBits & 0xfff), &ShipTypeIndex::m_value);
  return found == std::ranges::end(entries) ? -1 : found->m_index;
}

reb::battle::Side SideOf(uint32_t notAlliance)
{
  return notAlliance != 0 ? reb::battle::Side::Empire : reb::battle::Side::Alliance;
}

std::string ClassNameOf(GameObj* object)
{
  const DatRecord* record = object->m_record;
  if (!record) {
    return {};
  }
  if (!record->m_name.empty()) {
    return record->m_name;
  }
  const ResourceStringId resource{static_cast<int16_t>(record->m_small), static_cast<int16_t>(record->m_small2)};
  return LoadBattleDisplayString(resource);
}

std::string DisplayNameOf(GameObj* object, const std::string& className)
{
  const std::string& name = *object->GetName();
  return name.empty() ? className : name;
}

reb::battle::UnitIdentity IdentityOf(GameObj* object, uint32_t typeBits, int32_t typeIndex)
{
  ObjectKey key;
  object->GetKey(&key);
  const std::string className = ClassNameOf(object);
  return {KeyFamilyOf(&key), KeyIdOf(&key), SideOf(NotAlliance(ObjectControlSide(object))), typeBits, typeIndex,
          DisplayNameOf(object, className), className};
}

reb::battle::UnitIdentity CombatIdentityOf(GameObj* object)
{
  const uint32_t typeBits = object->GetRecordTypeBits() & 0xffff;
  return IdentityOf(object, typeBits, ShipTypeIndexOf(typeBits));
}

template <class Unit>
reb::battle::WeaponGrid WeaponsOf(Unit* unit, uint32_t (Unit::*points)(int, int))
{
  reb::battle::WeaponGrid grid;
  for (int arc = 0; arc < reb::battle::kArcCount; ++arc) {
    for (int kind = 0; kind < reb::battle::kWeaponKinds; ++kind) {
      grid.points[arc][kind] = static_cast<int32_t>((unit->*points)(arc, kind));
    }
  }
  for (int kind = 0; kind < reb::battle::kWeaponKinds; ++kind) {
    grid.range[kind] = static_cast<int32_t>(unit->GetGridFieldA4(kind));
  }
  return grid;
}

reb::battle::SideParams SideParamsOf(const SideQuery& query, GameTime threshold)
{
  return {threshold.m_value, query.m_c != 0, static_cast<float>(static_cast<int>(query.m_d)), false};
}

// Not drawn from the campaign RNG, so building the snapshot leaves the campaign's random sequence alone.
uint32_t BattleSeed(System* system)
{
  ObjectKey key;
  system->GetKey(&key);
  uint64_t mixed = (static_cast<uint64_t>(key.m_value) << 32) | static_cast<uint32_t>(GameHookTickCount());
  mixed += 0x9e3779b97f4a7c15ULL;
  mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9ULL;
  mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebULL;
  return static_cast<uint32_t>(mixed ^ (mixed >> 31));
}

} // namespace

BattleSetup::BattleSetup()
{
  m_driver = nullptr;
  m_system = nullptr;
  m_timeA.m_value = 0;
  m_timeB.m_value = 0;
}

BattleSetup::~BattleSetup() = default;

unsigned BattleSetup::Convert()
{
  m_system = nullptr;
  NotifyContext context;
  GameObjNotifier* root = GetGameRootObjectCached();
  unsigned result = root != nullptr;
  if (result) {
    context = *static_cast<GameRoot*>(root)->GetBattleContext();
    ObjectKey rootKey;
    m_driver->GetRootKey(&rootKey);
    m_system = static_cast<System*>(GetMasterInFamilies_90_98(&rootKey));
    result = m_system != nullptr;
  }
  if (m_driver != nullptr && m_system != nullptr) {
    GameObjNotifier* best = nullptr;
    if (m_system->FindBestShipOfCommandKind2(1, &best), best) {
      ObjectKey key;
      m_keyKind2Side1 = *static_cast<GameObj*>(best)->GetKey(&key);
    }
    if (m_system->FindBestShipOfCommandKind1(1, &best), best) {
      ObjectKey key;
      m_keyKind1Side1 = *static_cast<GameObj*>(best)->GetKey(&key);
    }
    if (m_system->FindBestShipOfCommandKind2(2, &best), best) {
      ObjectKey key;
      m_keyKind2Side2 = *static_cast<GameObj*>(best)->GetKey(&key);
    }
    if (m_system->FindBestShipOfCommandKind1(2, &best), best) {
      ObjectKey key;
      m_keyKind1Side2 = *static_cast<GameObj*>(best)->GetKey(&key);
    }

    SideQuery sideOne;
    SideQuery sideTwo;
    m_driver->QueryBattle(1, &sideOne.m_a, &sideOne.m_b, &sideOne.m_c, &sideOne.m_d);
    m_driver->QueryBattle(2, &sideTwo.m_a, &sideTwo.m_b, &sideTwo.m_c, &sideTwo.m_d);

    m_snapshot = {};
    m_unitObjects.clear();
    reb::battle::BattleParams& params = m_snapshot.params;
    params.systemName = DisplayNameOf(m_system, ClassNameOf(m_system));
    params.systemPictureId = m_system->GetRecordField44();
    params.systemDestroyed = ObjectDestroyedBit(m_system) != 0;
    // The battle control field names the side whose fleet or fighters arrived last; the other side held the system
    // first, the side BattleResults gives a contested system to.
    params.heldFirstBy = SystemBattleControlKind(m_system) == 1 ? reb::battle::Side::Empire : reb::battle::Side::Alliance;
    params.controlSide = static_cast<int32_t>(ObjectControlSide(m_system));
    params.opponentIsAi = UiIsOpponentAI() != 0;
    params.humanIsNotAlliance = NotAlliance(UiGetHumanSide());
    params.seed = BattleSeed(m_system);
    params.sides = {SideParamsOf(sideOne, m_timeA), SideParamsOf(sideTwo, m_timeB)};
    if (const int humanSide = UiGetHumanSide(); humanSide == 1 || humanSide == 2) {
      params.sides[humanSide - 1].human = true;
      params.sides[2 - humanSide].human = !params.opponentIsAi;
    }

    FleetIterator fleets(m_system, kBothSidesFilter);
    for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
      GameObjNotifier* fleet = fleets.Current();
      {
        CapitalShipIterator ships(fleet, kUsableFilter);
        for (ships.Begin(); ships.Current(); ships.Advance()) {
          result = AddCapitalShip(static_cast<CapShip*>(ships.Current()));
        }
      }
      {
        DeathStarIterator deathStars(fleet, kUsableFilter);
        for (deathStars.Begin(); deathStars.Current(); deathStars.Advance()) {
          result = AddDeathStar(static_cast<CapShip*>(deathStars.Current()));
        }
      }
      // Repeated for every fleet, as in the original: with two fleets present a Death Star under construction is
      // registered twice, and its last registration is the one its handle refers to.
      if (GameObjNotifier* manager = m_system->FindManufacturingManagerByKind(kFirstManufacturingSlot)) {
        ObjectKey productKey;
        ManufacturingManagerProductKey(static_cast<ManuMgr*>(manager), &productKey);
        if (productKey.IsSet()) {
          const bool deathStarFamily = inFamilyRange(KeyFamilyOf(&productKey), kFirstDeathStarFamily, kEndShipFamily);
          if (deathStarFamily) {
            auto* object = static_cast<GameObj*>(GetMasterInFamilies_18_1c(&productKey));
            if (object && !(object->m_stateFlags & kCompletedFlag)) {
              result = AddDeathStar(static_cast<CapShip*>(object));
            }
          }
        }
      }
      {
        CapShipIterator capShips(fleet, kUsableFilter);
        for (capShips.Begin(); capShips.Current(); capShips.Advance()) {
          GameObjNotifier* carrier = capShips.Current();
          {
            FightSquadIterator squads(carrier, kUsableFilter);
            for (squads.Begin(); squads.Current(); squads.Advance()) {
              result = AddFightSquad(static_cast<FightSquad*>(squads.Current()));
            }
          }
          {
            CharacterIterator characters(carrier, kUsableFilter);
            for (characters.Begin(); characters.Current(); characters.Advance()) {
              result = AddCharacter(static_cast<Character*>(characters.Current()));
            }
          }
          {
            SpecialForceIterator forces(carrier, kUsableFilter);
            for (forces.Begin(); forces.Current(); forces.Advance()) {
              result = AddSpecialForce(static_cast<GameObj*>(forces.Current()));
            }
          }
          {
            TroopRegIterator troops(carrier, kUsableFilter);
            for (troops.Begin(); troops.Current(); troops.Advance()) {
              result = AddTroopRegiment(static_cast<GameObj*>(troops.Current()));
            }
          }
        }
      }
    }
    FightSquadIterator systemSquads(m_system, kUsableFilter);
    for (systemSquads.Begin(); systemSquads.Current(); systemSquads.Advance()) {
      result = AddFightSquad(static_cast<FightSquad*>(systemSquads.Current()));
    }
    AddDefenses();

    // VoteWindow walks the observing side's espionage view, rather than the master objects.
    ObjectKey systemKey;
    m_system->GetKey(&systemKey);
    for (int observer = 1; observer <= 2; ++observer) {
      std::set<uint32_t> known;
      const auto remember = [&](GameObjNotifier* object) {
        ObjectKey key;
        known.insert(object->GetKey(&key)->m_value);
      };
      if (auto* visibleSystem = GetObjectInFamilies_90_98(observer, &systemKey)) {
        FleetIterator visibleFleets(visibleSystem, kUsableFilter);
        for (visibleFleets.Begin(); visibleFleets.Current(); visibleFleets.Advance()) {
          CapShipIterator ships(visibleFleets.Current(), kUsableFilter);
          for (ships.Begin(); ships.Current(); ships.Advance()) {
            remember(ships.Current());
            FightSquadIterator squads(ships.Current(), kUsableFilter);
            for (squads.Begin(); squads.Current(); squads.Advance()) {
              remember(squads.Current());
            }
          }
        }
        FightSquadIterator squads(visibleSystem, kUsableFilter);
        for (squads.Begin(); squads.Current(); squads.Advance()) {
          remember(squads.Current());
        }
        DefenseFacility22Iterator ionCannons(visibleSystem, kUsableFilter);
        for (ionCannons.Begin(); ionCannons.Current(); ionCannons.Advance()) {
          remember(ionCannons.Current());
        }
        DefenseFacility23Iterator turbolasers(visibleSystem, kUsableFilter);
        for (turbolasers.Begin(); turbolasers.Current(); turbolasers.Advance()) {
          remember(turbolasers.Current());
        }
      }
      const auto mark = [&](reb::battle::UnitIdentity& identity) {
        identity.briefingKnown[observer - 1] = identity.side == SideOf(observer != 1)
            || known.contains(ObjectKey::Make(identity.family, identity.id).m_value);
      };
      for (auto& unit : m_snapshot.units) {
        std::visit([&](auto& entry) { mark(entry.identity); }, unit);
      }
      for (auto& defense : m_snapshot.defenses) {
        mark(defense.identity);
      }
    }

    const std::vector<int> handles = m_engine->Load(m_snapshot);
    for (size_t unit = 0; unit < handles.size(); ++unit) {
      if (handles[unit] != 0) {
        m_unitObjects[unit]->SetDataField04(handles[unit]);
      }
    }
  }
  return result;
}

int BattleSetup::AddUnit(GameObj* object, reb::battle::UnitEntry entry)
{
  m_snapshot.units.push_back(std::move(entry));
  m_unitObjects.push_back(object);
  return 1;
}

// The unit that registered `object` last, as GetDataField04 would have said during the original registration.
reb::battle::UnitIndex BattleSetup::UnitOf(GameObjNotifier* object) const
{
  for (auto unit = static_cast<reb::battle::UnitIndex>(m_unitObjects.size()) - 1; unit >= 0; --unit) {
    if (m_unitObjects[unit] == object) {
      return unit;
    }
  }
  return reb::battle::kNoUnit;
}

int BattleSetup::AddCapitalShip(CapShip* ship)
{
  int fighterCount = 0;
  {
    FightSquadIterator squads(ship, kUsableFilter);
    fighterCount = squads.Count();
  }
  reb::battle::CapitalShipEntry entry;
  entry.identity = CombatIdentityOf(ship);
  entry.hullMax = ship->GetHull();
  entry.hull = CapShipCurrentHull(ship);
  entry.shieldMax = static_cast<int32_t>(ship->GetField60());
  entry.sublight = static_cast<int32_t>(ship->GetField64());
  entry.maneuverability = static_cast<int32_t>(ship->GetField68());
  entry.detection = static_cast<int32_t>(ship->GetRecordField5c());
  entry.hyperdrive = static_cast<int32_t>(ship->GetField6c());
  entry.hyperdriveBackup = static_cast<int32_t>(ship->GetField70());
  entry.shieldRecharge = ship->GetShieldRechargeRate();
  entry.weaponRecharge = ship->GetWeaponRechargeRate();
  entry.damageControl = ship->GetDamageControl();
  entry.tractorPower = ship->GetTractorBeamPower();
  entry.tractorRange = ship->GetTractorBeamRange();
  entry.gravityWell = ship->GetGravityWellProjector();
  entry.interdiction = ship->GetInterdictionStrength();
  entry.fighterCapacity = ship->GetFighterCapacity();
  // Capacity minus every squadron aboard, plus the usable ones (which fight from this hangar).
  entry.hangarFree = ship->GetFighterCapacity() - ship->CountFighterSquadrons() + fighterCount;
  entry.troopCapacity = ship->GetTroopCapacity();
  entry.gridTotal = static_cast<int32_t>(ship->GetGridTotal());
  entry.weapons = WeaponsOf<CombatUnit>(ship, &CombatUnit::GetGridValue);
  entry.criticals = {static_cast<int32_t>(CapShipSpeedCH(ship)),
                     static_cast<int32_t>(CapShipShieldRechargeCH(ship)),
                     static_cast<int32_t>(CapShipWeaponRechargeCH(ship)),
                     static_cast<int32_t>(CapShipTractorBeamCH(ship)),
                     static_cast<int32_t>(CapShipPrimaryHyperdriveCH(ship)),
                     static_cast<int32_t>(CapShipBackupHyperdriveCH(ship))};
  return AddUnit(ship, std::move(entry));
}

int BattleSetup::AddDeathStar(CapShip* deathStar)
{
  int facilityCount;
  {
    DefenseFacility25Iterator facilities(m_system, kUsableFilter);
    facilityCount = facilities.Count();
  }
  int squadCount;
  {
    FightSquadIterator squads(deathStar, kUsableFilter);
    squadCount = squads.Count();
  }
  reb::battle::DeathStarEntry entry;
  entry.identity = CombatIdentityOf(deathStar);
  entry.completed = ((deathStar->m_stateFlags >> 2) & 1) != 0;
  entry.shieldFacility = facilityCount != 0;
  entry.hullMax = deathStar->GetHull();
  entry.hull = CapShipCurrentHull(deathStar);
  entry.hangarFree = deathStar->GetFighterCapacity() - deathStar->CountFighterSquadrons() + squadCount;
  return AddUnit(deathStar, std::move(entry));
}

// The strategic squadron reports its shield, grid and torpedoes for the whole squadron (the record's per-craft value
// times the full squadron size, fight_squad.cpp); the snapshot keeps the per-craft record values.
int BattleSetup::AddFightSquad(FightSquad* squad)
{
  GameObjNotifier* carrier = nullptr;
  if (!squad->GetParent(&carrier)) {
    return 0;
  }
  reb::battle::SquadronEntry entry;
  entry.identity = CombatIdentityOf(squad);
  if (inFamilyRange(carrier->GetFamily(), kFirstShipFamily, kEndShipFamily)) {
    entry.carrier = UnitOf(carrier);
  }
  entry.squadSize = static_cast<int32_t>(squad->GetSquadSize());
  entry.currentSize = FightSquadCurrentSize(squad);
  entry.shieldPerCraft = static_cast<int32_t>(squad->CombatUnit::GetField60());
  entry.sublight = static_cast<int32_t>(squad->GetField64());
  entry.maneuverability = static_cast<int32_t>(squad->GetField68());
  entry.detection = static_cast<int32_t>(squad->GetRecordField5c());
  entry.hyperdrive = static_cast<int32_t>(squad->GetField6c());
  entry.hyperdriveBackup = static_cast<int32_t>(squad->GetField70());
  entry.torpedoesPerCraft = static_cast<int32_t>(squad->GetRecordTorpedoes());
  entry.torpedoRange = static_cast<int32_t>(squad->GetTorpedoRange());
  entry.weaponsPerCraft = WeaponsOf<CombatUnit>(squad, &CombatUnit::GetRecordGridValue);
  return AddUnit(squad, std::move(entry));
}

int BattleSetup::AddCharacter(Character* character)
{
  GameObjNotifier* ship = nullptr;
  if (!character->GetParent(&ship)) {
    return 0;
  }
  reb::battle::CharacterEntry entry;
  entry.identity = IdentityOf(character, character->GetRecordTypeBits(), -1);
  entry.ship = UnitOf(ship);
  ObjectKey key;
  const uint32_t sideControl = character->m_flags & kSideControlMask;
  if (sideControl == kSide1Control) {
    entry.admiral = character->GetKey(&key)->m_value == m_keyKind2Side1.m_value;
    entry.commander = character->GetKey(&key)->m_value == m_keyKind1Side1.m_value;
  } else if (sideControl == kSide2Control) {
    entry.admiral = character->GetKey(&key)->m_value == m_keyKind2Side2.m_value;
    entry.commander = character->GetKey(&key)->m_value == m_keyKind1Side2.m_value;
  }
  entry.major = inFamilyRange(character->GetFamily(), kFirstMajorCharacterFamily, kEndMajorCharacterFamily);
  entry.injured = character->m_injury != 0;
  entry.leadership = character->GetLeadership();
  entry.combat = character->GetCombat();
  return AddUnit(character, std::move(entry));
}

// Planetary batteries fire in the battle (§6.13); shields, troops and other facilities stay strategic. A facility of
// neither side (an uncontrolled system's) does not take part.
void BattleSetup::AddDefenses()
{
  const auto add = [&](GameObj* facility, reb::battle::WeaponKind weapon) {
    const uint32_t control = ObjectControlSide(facility);
    if (control != 1 && control != 2) {
      return;
    }
    reb::battle::DefenseEntry entry;
    entry.identity = IdentityOf(facility, facility->GetRecordTypeBits() & 0xffff, -1);
    entry.weapon = weapon;
    entry.attackStrength = static_cast<int32_t>(static_cast<DefenseFacility*>(facility)->GetAttackStrength());
    m_snapshot.defenses.push_back(std::move(entry));
  };
  DefenseFacility22Iterator ionCannons(m_system, kUsableFilter);
  for (ionCannons.Begin(); ionCannons.Current(); ionCannons.Advance()) {
    add(static_cast<GameObj*>(ionCannons.Current()), reb::battle::WeaponKind::Ion);
  }
  DefenseFacility23Iterator turbolasers(m_system, kUsableFilter);
  for (turbolasers.Begin(); turbolasers.Current(); turbolasers.Advance()) {
    add(static_cast<GameObj*>(turbolasers.Current()), reb::battle::WeaponKind::Turbolaser);
  }
}

int BattleSetup::AddPassenger(GameObj* passenger, bool troopRegiment)
{
  GameObjNotifier* ship = nullptr;
  if (!passenger->GetParent(&ship)) {
    return 0;
  }
  reb::battle::PassengerEntry entry;
  entry.identity = IdentityOf(passenger, passenger->GetRecordTypeBits(), -1);
  entry.ship = UnitOf(ship);
  entry.troopRegiment = troopRegiment;
  return AddUnit(passenger, std::move(entry));
}

int BattleSetup::AddSpecialForce(GameObj* force)
{
  return AddPassenger(force, false);
}

int BattleSetup::AddTroopRegiment(GameObj* troops)
{
  return AddPassenger(troops, true);
}
