#include "gameobj/game_root.h"
#include "app/battle_flow.h"
#include "app/battle_handoff.h"

#include <new>

#include "character/character.h"
#include "character/special_forces.h"
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
#include "strategy_ui/game_ui_facade.h"
#include "tactical/tactical_facade.h"

// The strategic -> tactical hand-over (0x40afa0-0x40c730): BattleSetup walks the battle's system and registers every
// ship, squadron, character and ground unit with the tactical engine through the facade, then remembers the returned
// tactical handle in the strategic object's data block.


extern int g_characterScaleDivisorA;  // 0x6a80e4
extern int g_characterScaleDivisorB;  // 0x6a80e8

namespace {

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

bool inFamilyRange(uint32_t family, uint32_t first, uint32_t end) { return family >= first && family < end; }

struct SideQuery {
    uint32_t m_a = 0;
    uint32_t m_b = 0;
    int m_c = 0;
    uint32_t m_d = 0;
};

}  // namespace

// ---- small accessors the conversion uses -----------------------------------------------------------------------------------

int NotAlliance(int side) { return side != 1; }  // 0x40d660

double ScaleCharacterValueA(int value) { return static_cast<double>(value) / static_cast<double>(g_characterScaleDivisorA); }  // 0x40d670

double ScaleCharacterValueB(int value) { return static_cast<double>(value) / static_cast<double>(g_characterScaleDivisorB); }  // 0x40d680

uint32_t KeyFamilyOf(const ObjectKey* key) { return key->m_value >> 24; }  // 0x40c490

uint32_t KeyIdOf(const ObjectKey* key) { return key->m_value & 0xffffff; }  // 0x40c4a0

uint32_t ObjectControlSide(GameObjRoot* object) { return object->ControlSide(); }  // 0x40d690

uint32_t ObjectDestroyedBit(GameObj* object) { return (object->m_stateFlags >> 3) & 1; }  // 0x40d6a0

uint32_t CapShipShieldRechargeCH(CapShip* ship) { return ship->GetCriticalHit(CapShip::kShieldRechargeShift); }  // 0x40d6b0

uint32_t CapShipWeaponRechargeCH(CapShip* ship) { return ship->GetCriticalHit(CapShip::kWeaponRechargeShift); }  // 0x40d6c0

uint32_t CapShipTractorBeamCH(CapShip* ship) { return ship->GetCriticalHit(CapShip::kTractorBeamShift); }  // 0x40d6d0

uint32_t CapShipSpeedCH(CapShip* ship) { return ship->GetCriticalHit(CapShip::kSpeedShift); }  // 0x40d6e0

uint32_t CapShipPrimaryHyperdriveCH(CapShip* ship) { return ship->GetCriticalHit(CapShip::kPrimaryHyperdriveShift); }  // 0x40d6f0

uint32_t CapShipBackupHyperdriveCH(CapShip* ship) { return ship->GetCriticalHit(CapShip::kBackupHyperdriveShift); }  // 0x40d700

int CapShipCurrentHull(CapShip* ship) { return ship->GetHull() - ship->GetHullValueDamage(); }  // 0x40d710

int FightSquadCurrentSize(FightSquad* squad) { return squad->GetSquadSize() - squad->GetSquadSizeDamage(); }  // 0x40d730

uint32_t SystemBattleControlKind(System* system) { return ControlBitsField(system->m_controlBits, 6); }  // 0x40d750

ObjectKey* ManufacturingManagerProductKey(ManuMgr* manager, ObjectKey* out) {  // 0x40d760
    new (out) ObjectKey(manager->m_productKey);
    return out;
}

// ---- BattleSetup ------------------------------------------------------------------------------------------------------------------

BattleSetup::BattleSetup() {
    m_driver = nullptr;
    m_system = nullptr;
    m_abstract = 0;
    m_timeA.m_value = 0;
    m_timeB.m_value = 0;
}

BattleSetup::~BattleSetup() {}

unsigned BattleSetup::Convert() {
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

        const uint32_t controlSide = ObjectControlSide(m_system);
        TacticalBeginBattle(NotAlliance(UiGetHumanSide()), UiIsOpponentAI(), m_system->GetRecordField44(),
                            *m_system->GetName(), static_cast<float>(static_cast<int>(sideOne.m_d)),
                            static_cast<float>(static_cast<int>(sideTwo.m_d)), sideOne.m_c, sideTwo.m_c, m_timeA,
                            m_timeB, NotAlliance(SystemBattleControlKind(m_system)),
                            ObjectDestroyedBit(m_system), controlSide == 2, controlSide == 1, m_abstract);

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
            if (GameObjNotifier* manager = m_system->FindManufacturingManagerBySlot123(kFirstManufacturingSlot)) {
                ObjectKey productKey;
                ManufacturingManagerProductKey(static_cast<ManuMgr*>(manager), &productKey);
                if (productKey.IsSet()) {
                    const bool deathStarFamily =
                        inFamilyRange(KeyFamilyOf(&productKey), kFirstDeathStarFamily, kEndShipFamily);
                    if (deathStarFamily) {
                        GameObj* object = static_cast<GameObj*>(GetMasterInFamilies_18_1c(&productKey));
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
    }
    return result;
}

int BattleSetup::AddCapitalShip(CapShip* ship) {
    int handle = 0;
    int fighterCount = 0;
    bool ok = m_driver != nullptr && m_system != nullptr;
    if (ok) {
        {
            FightSquadIterator squads(ship, kUsableFilter);
            fighterCount = squads.Count();
        }
        const int hangarFree = ship->GetFighterCapacity() - ship->CountFighterSquadrons() + fighterCount;
        ObjectKey firstKey;
        ObjectKey secondKey;
        ship->GetKey(&firstKey);
        ship->GetKey(&secondKey);
        handle = TacticalRegisterCapitalShip(
            KeyFamilyOf(&secondKey), KeyIdOf(&firstKey), NotAlliance(ObjectControlSide(ship)),
            ship->GetRecordTypeBits() & 0xffff, *ship->GetName(), static_cast<float>(ship->GetHull()),
            static_cast<float>(CapShipCurrentHull(ship)), static_cast<float>(ship->GetDamageControl()), hangarFree,
            static_cast<float>(ship->GetTractorBeamPower()), static_cast<float>(ship->GetTractorBeamRange()),
            ship->GetGravityWellProjector(), static_cast<float>(ship->GetInterdictionStrength()),
            static_cast<float>(ship->GetField64()), static_cast<float>(ship->GetField68()),
            static_cast<float>(ship->GetField60()),
            static_cast<float>((ship->GetField70() != 0) + (ship->GetField6c() != 0)),
            static_cast<float>(ship->GetGridTotal()), static_cast<float>(ship->GetWeaponRechargeRate()),
            static_cast<float>(ship->GetShieldRechargeRate()), ship->GetGridValue(0, 2), ship->GetGridValue(1, 2),
            ship->GetGridValue(2, 2), ship->GetGridValue(3, 2), ship->GetGridValue(0, 1), ship->GetGridValue(1, 1),
            ship->GetGridValue(2, 1), ship->GetGridValue(3, 1), ship->GetGridValue(0, 0), ship->GetGridValue(1, 0),
            ship->GetGridValue(2, 0), ship->GetGridValue(3, 0), CapShipSpeedCH(ship), CapShipShieldRechargeCH(ship),
            CapShipWeaponRechargeCH(ship), CapShipPrimaryHyperdriveCH(ship) + CapShipBackupHyperdriveCH(ship),
            CapShipTractorBeamCH(ship), static_cast<float>(ship->GetGridFieldA4(2)),
            static_cast<float>(ship->GetGridFieldA4(1)), static_cast<float>(ship->GetGridFieldA4(0)));
        ok = handle != 0;
    }
    if (ok) ship->SetDataField04(handle);
    return ok;
}

int BattleSetup::AddDeathStar(CapShip* deathStar) {
    int handle = 0;
    bool ok = m_driver != nullptr && m_system != nullptr;
    if (ok) {
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
        const int hangarFree = deathStar->GetFighterCapacity() - deathStar->CountFighterSquadrons() + squadCount;
        const int currentHull = CapShipCurrentHull(deathStar);
        const uint32_t completed = (deathStar->m_stateFlags >> 2) & 1;
        ObjectKey idKey;
        ObjectKey familyKey;
        deathStar->GetKey(&idKey);
        deathStar->GetKey(&familyKey);
        handle = TacticalRegisterDeathStar(KeyFamilyOf(&familyKey), KeyIdOf(&idKey), completed == 0, facilityCount != 0,
                                           static_cast<float>(deathStar->GetHull()), static_cast<float>(currentHull),
                                           hangarFree);
        ok = handle != 0;
    }
    if (ok) deathStar->SetDataField04(handle);
    return ok;
}

int BattleSetup::AddFightSquad(FightSquad* squad) {
    int handle = 0;
    int carrierHandle = 0;
    int attachedToShip = 0;
    GameObjNotifier* carrier = nullptr;
    bool ok = m_driver != nullptr && m_system != nullptr;
    if (ok) ok = squad->GetParent(&carrier);
    if (ok) {
        attachedToShip = inFamilyRange(carrier->GetFamily(), kFirstShipFamily, kEndShipFamily);
        if (attachedToShip) ok = static_cast<GameObj*>(carrier)->GetDataField04(&carrierHandle);
    }
    if (ok) {
        const int squadSize = squad->GetSquadSize();
        ObjectKey idKey;
        ObjectKey familyKey;
        squad->GetKey(&idKey);
        squad->GetKey(&familyKey);
        handle = TacticalRegisterFightSquad(
            KeyFamilyOf(&familyKey), KeyIdOf(&idKey), NotAlliance(ObjectControlSide(squad)),
            squad->GetRecordTypeBits() & 0xffff, *squad->GetName(), carrierHandle, attachedToShip,
            static_cast<float>(squad->GetField64()), static_cast<float>(squad->GetField68()),
            static_cast<float>(squad->GetField60()), squadSize, FightSquadCurrentSize(squad), 0,
            static_cast<float>((squad->GetField70() != 0) + (squad->GetField6c() != 0)), squad->GetGridValue(0, 2),
            squad->GetGridValue(1, 2), squad->GetGridValue(2, 2), squad->GetGridValue(3, 2), squad->GetGridValue(0, 1),
            squad->GetGridValue(1, 1), squad->GetGridValue(2, 1), squad->GetGridValue(3, 1), squad->GetGridValue(0, 0),
            squad->GetGridValue(1, 0), squad->GetGridValue(2, 0), squad->GetGridValue(3, 0),
            static_cast<float>(squad->GetGridFieldA4(2)), static_cast<float>(squad->GetGridFieldA4(1)),
            static_cast<float>(squad->GetGridFieldA4(0)), squad->GetTotalTorpedoes(),
            static_cast<float>(squad->GetTorpedoRange()));
        ok = handle != 0;
    }
    if (ok) squad->SetDataField04(handle);
    return ok;
}

int BattleSetup::AddCharacter(Character* character) {
    int shipHandle = 0;
    int isFirstCommander = 0;
    int isSecondCommander = 0;
    int isMajor = 0;
    int injured = 0;
    GameObjNotifier* ship = nullptr;
    bool ok = m_driver != nullptr && m_system != nullptr;
    if (ok) ok = character->GetParent(&ship);
    if (ok) ok = static_cast<GameObj*>(ship)->GetDataField04(&shipHandle);
    if (ok) {
        ObjectKey key;
        const uint32_t sideControl = character->m_flags & kSideControlMask;
        if (sideControl == kSide1Control) {
            isFirstCommander = character->GetKey(&key)->m_value == m_keyKind2Side1.m_value;
            isSecondCommander = character->GetKey(&key)->m_value == m_keyKind1Side1.m_value;
        } else if (sideControl == kSide2Control) {
            isFirstCommander = character->GetKey(&key)->m_value == m_keyKind2Side2.m_value;
            isSecondCommander = character->GetKey(&key)->m_value == m_keyKind1Side2.m_value;
        }
        isMajor = inFamilyRange(character->GetFamily(), kFirstMajorCharacterFamily, kEndMajorCharacterFamily);
        injured = character->m_injury != 0;
    }
    int handle = 0;
    bool registered = false;
    if (ok) {
        const uint32_t typeBits = character->GetRecordTypeBits();
        const float scaleA = static_cast<float>(ScaleCharacterValueA(character->GetLeadership()));
        const float scaleB = static_cast<float>(ScaleCharacterValueB(character->GetCombat()));
        ObjectKey idKey;
        ObjectKey familyKey;
        character->GetKey(&idKey);
        character->GetKey(&familyKey);
        handle = TacticalRegisterCharacter(KeyFamilyOf(&familyKey), KeyIdOf(&idKey), *character->GetName(),
                                           NotAlliance(ObjectControlSide(character)), isMajor, isFirstCommander,
                                           isSecondCommander, scaleA, scaleB, shipHandle, injured, typeBits);
        registered = handle != 0;
        if (registered) character->SetDataField04(handle);
    }
    return registered;
}

int BattleSetup::AddSpecialForce(GameObj* force) {
    int shipHandle = 0;
    GameObjNotifier* ship = nullptr;
    bool ok = m_driver != nullptr && m_system != nullptr;
    if (ok) ok = force->GetParent(&ship);
    if (ok) ok = static_cast<GameObj*>(ship)->GetDataField04(&shipHandle);
    bool registered = false;
    if (ok) {
        ObjectKey idKey;
        ObjectKey familyKey;
        force->GetKey(&idKey);
        force->GetKey(&familyKey);
        const int handle = TacticalRegisterSpecialForce(KeyFamilyOf(&familyKey), KeyIdOf(&idKey),
                                                          force->GetRecordTypeBits(), *force->GetName(),
                                                          NotAlliance(ObjectControlSide(force)), shipHandle);
        registered = handle != 0;
        if (registered) force->SetDataField04(handle);
    }
    return registered;
}

int BattleSetup::AddTroopRegiment(GameObj* troops) {
    int shipHandle = 0;
    GameObjNotifier* ship = nullptr;
    bool ok = m_driver != nullptr && m_system != nullptr;
    if (ok) ok = troops->GetParent(&ship);
    if (ok) ok = static_cast<GameObj*>(ship)->GetDataField04(&shipHandle);
    bool registered = false;
    if (ok) {
        ObjectKey idKey;
        ObjectKey familyKey;
        troops->GetKey(&idKey);
        troops->GetKey(&familyKey);
        const int handle = TacticalRegisterSpecialForce(KeyFamilyOf(&familyKey), KeyIdOf(&idKey),
                                                          troops->GetRecordTypeBits(), *troops->GetName(),
                                                          NotAlliance(ObjectControlSide(troops)), shipHandle);
        registered = handle != 0;
        if (registered) troops->SetDataField04(handle);
    }
    return registered;
}

int g_characterScaleDivisorA = 10;
int g_characterScaleDivisorB = 20;
