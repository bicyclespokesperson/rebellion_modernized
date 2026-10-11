#include "rules/planet_bombard.h"

#include "ai/ai_common.h"
#include "character/character.h"
#include "data/random.h"
#include "economy/defense_facility.h"
#include "economy/facility.h"
#include "economy/procfacil.h"
#include "fleet/capship.h"
#include "fleet/fleet.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "galaxy/system_rules_1e.h"
#include "gameobj/game_hooks.h"
#include "mission/mission.h"
#include "mission/mission_rules.h"
#include "rules/event_rules.h"
#include "rules/loyalty_params.h"
#include "story/story_characters.h"

namespace {

constexpr FamilyRange kProcFacilityFamilies(0x28, 0x30);
constexpr FamilyRange kDeathStarFamilies(0x18, 0x1c);
constexpr uint32_t kBombardFleetFlag = 0x40;
constexpr uint32_t kUsableFilter = 1;
constexpr uint32_t kExistingFilter = 3;
constexpr uint32_t kAlwaysFilter = 4;
constexpr int kFriendlyFireReason = 0x10;
constexpr int kBombardedReason = 0xf;
constexpr int kBombardmentShipReason = 0xf;
constexpr int kPlanetDeathReason = 10;
constexpr int kLieutenantNumber = 2;
constexpr int kCommanderNumber = 3;

int both(int result, int ok) { return result != 0 && ok != 0 ? 1 : 0; }

bool inFamilies(GameObjNotifier* object, const FamilyRange& range) {
    const uint32_t family = object->GetFamily();
    return family >= range.m_first && family < range.m_second;
}

int commanderLeadership(GameObjNotifier* commander) {
    return commander != nullptr ? static_cast<Character*>(commander)->GetLeadership() : 0;
}

}  // namespace


// ---- visitors ----------------------------------------------------------------------------------------------------

ShipVisitor::ShipVisitor(BombardWalker* walker) : m_walker(walker) {}
ShipVisitor::~ShipVisitor() = default;

DefenderVisitor::DefenderVisitor(BombardWalker* walker) : m_walker(walker) {}
DefenderVisitor::~DefenderVisitor() = default;

ShipCounter::ShipCounter(BombardWalker* walker) : ShipVisitor(walker), m_count(0) {}
ShipCounter::~ShipCounter() = default;

int ShipCounter::Visit(GameObj*, GameObj*, uint32_t* stop, NotifyContext*) {
    ++m_count;
    *stop = 0;
    return 1;
}

ShipPicker::ShipPicker(BombardWalker* walker)
    : ShipVisitor(walker), m_index(0), m_target(0), m_ship(nullptr), m_fleet(nullptr) {}
ShipPicker::~ShipPicker() = default;

int ShipPicker::Visit(GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext*) {
    *stop = 0;
    const int present = ship != nullptr;
    if (present) {
        *stop = m_index == m_target;
        if (*stop != 0) {
            m_ship = ship;
            m_fleet = fleet;
            return present;
        }
        ++m_index;
    }
    return present;
}

DeathStarFinder::DeathStarFinder(BombardWalker* walker) : ShipVisitor(walker), m_found(0) {}
DeathStarFinder::~DeathStarFinder() = default;

int DeathStarFinder::Visit(GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext*) {
    *stop = 0;
    m_found = 0;
    const int present = ship != nullptr && fleet != nullptr;
    if (present != 0) {
        m_found = inFamilies(ship, kDeathStarFamilies);
        *stop = m_found;
    }
    return present;
}

ShipDamager::ShipDamager(BombardWalker* walker) : ShipVisitor(walker) {}
ShipDamager::~ShipDamager() = default;

int ShipDamager::Visit(GameObj* shipObject, GameObj*, uint32_t* stop, NotifyContext* ctx) {
    *stop = 0;
    CapShip* ship = static_cast<CapShip*>(shipObject);
    int engineDamage = ship->GetDataWord20();
    int weaponDamage = reinterpret_cast<ProcFacil*>(ship)->ProcFacil::GetCycleCount();
    const int shield = ship->GetField60() - ship->GetDamageField60();
    int shieldLeft = shield;
    const int hull = ship->GetHull() - ship->GetHullValueDamage();
    int hullLeft = hull;

    int absorbedByShield = 0;
    int shieldLost = 0;
    SplitDifference(engineDamage, shieldLeft, &absorbedByShield, &shieldLost);
    int absorbedByHull = 0;
    int weaponOverflow = 0;
    SplitDifference(weaponDamage, shieldLost, &absorbedByHull, &weaponOverflow);
    int hullOverflow = 0;
    int hullDamage = 0;
    SplitDifference(absorbedByHull, hullLeft, &hullDamage, &hullOverflow);

    ship->SetDataWord20(absorbedByShield);
    ship->SetDataWord24(hullDamage);
    ship->SetDataWord28((ship->GetField60() - ship->GetDamageField60()) - weaponOverflow);
    const int hullToLose = (ship->GetHull() - ship->GetHullValueDamage()) - hullOverflow;
    if (ship->DamageHull(hullToLose, kBombardmentShipReason, ctx) == 0) return 0;
    return ship->InflictCriticalHit(hullToLose, kBombardmentShipReason, ctx);
}

ShipCleaner::ShipCleaner(BombardWalker* walker) : ShipVisitor(walker) {}
ShipCleaner::~ShipCleaner() = default;

int ShipCleaner::Visit(GameObj* shipObject, GameObj* fleet, uint32_t* stop, NotifyContext*) {
    *stop = 0;
    if (shipObject != nullptr && fleet != nullptr) {
        CapShip* ship = static_cast<CapShip*>(shipObject);
        ship->SetDataWord20(0);
        ship->SetDataWord24(0);
        ship->SetDataWord28(0);
    }
    return 1;
}

DefenderCounter::DefenderCounter(BombardWalker* walker) : DefenderVisitor(walker), m_count(0) {}
DefenderCounter::~DefenderCounter() = default;

int DefenderCounter::Visit(GameObj*, GameObj*, int, int, uint32_t* stop, NotifyContext*) {
    ++m_count;
    *stop = 0;
    return 1;
}

DefenderPicker::DefenderPicker(BombardWalker* walker)
    : DefenderVisitor(walker), m_index(0), m_target(0), m_picked{nullptr, nullptr, 0, 0} {}
DefenderPicker::~DefenderPicker() = default;

int DefenderPicker::Visit(GameObj* facility, GameObj* troop, int energy, int rawMaterial, uint32_t* stop,
                          NotifyContext*) {
    *stop = 0;
    const int exactlyOne = (facility != nullptr) + rawMaterial + (troop != nullptr) + energy == 1;
    if (exactlyOne != 0) {
        *stop = m_index == m_target;
        if (*stop != 0) {
            m_picked.m_facility = facility;
            m_picked.m_troop = troop;
            m_picked.m_energy = energy;
            m_picked.m_rawMaterial = rawMaterial;
            return exactlyOne;
        }
        ++m_index;
    }
    return exactlyOne;
}

void DefenderPicker::GetPicked(int* found, DefenderRef* ref) {
    *found = 1;
    ref->m_facility = nullptr;
    ref->m_troop = nullptr;
    ref->m_energy = 0;
    ref->m_rawMaterial = 0;
    if (m_picked.m_facility != nullptr) {
        ref->m_facility = m_picked.m_facility;
    } else if (m_picked.m_troop != nullptr) {
        ref->m_troop = m_picked.m_troop;
    } else if (m_picked.m_energy != 0) {
        ref->m_energy = 1;
    } else if (m_picked.m_rawMaterial != 0) {
        ref->m_rawMaterial = 1;
    } else {
        *found = 0;
    }
}

// ---- walker ------------------------------------------------------------------------------------------------------

BombardWalker::BombardWalker()
    : m_unused4(1), m_system(nullptr), m_attackerSide(0), m_shipCount(0), m_defenderCount(0), m_collateral(0),
      m_damaged(0), m_planetDestroyed(0), m_shipsGone(0), m_hitDefenses(0), m_hitProduction(0), m_hitResources(0),
      m_collateralAllowed(0), m_deathStar(0) {}
BombardWalker::~BombardWalker() = default;

int BombardWalker::ForEachBombardingShip(ShipVisitor* visitor, NotifyContext* ctx) {
    int ok = 1;
    uint32_t stop = 0;
    if (m_system == nullptr) return ok;
    FleetIterator fleets(m_system, kUsableFilter, m_attackerSide);
    fleets.Begin();
    while (fleets.Current() != nullptr && stop == 0) {
        Fleet* fleet = static_cast<Fleet*>(fleets.Current());
        fleets.Advance();
        if ((fleet->m_fleetFlags & kBombardFleetFlag) != 0) {
            CapShipIterator ships(fleet, kUsableFilter, m_attackerSide);
            ships.Begin();
            while (ok != 0 && ships.Current() != nullptr && stop == 0) {
                GameObjNotifier* ship = ships.Current();
                ships.Advance();
                ok = visitor->Visit(static_cast<GameObj*>(ship), fleet, &stop, ctx);
            }
        }
        if (ok == 0) break;
    }
    return ok;
}

int BombardWalker::ForEachDefender(DefenderVisitor* visitor, NotifyContext* ctx) {
    int ok = 1;
    uint32_t stop = 0;
    if (m_system == nullptr) return ok;
    const uint32_t defenderSide = m_system->ControlSide();
    if (m_hitDefenses != 0) {
        TroopRegIterator troops(m_system, kUsableFilter, defenderSide);
        troops.Begin();
        while (troops.Current() != nullptr && stop == 0) {
            GameObjNotifier* troop = troops.Current();
            troops.Advance();
            ok = visitor->Visit(nullptr, static_cast<GameObj*>(troop), 0, 0, &stop, ctx);
            if (ok == 0) break;
        }
        DefenseFacilityIterator defenses(m_system, kUsableFilter, m_system->ControlSide());
        defenses.Begin();
        while (ok != 0 && defenses.Current() != nullptr && stop == 0) {
            GameObjNotifier* facility = defenses.Current();
            defenses.Advance();
            ok = visitor->Visit(static_cast<GameObj*>(facility), nullptr, 0, 0, &stop, ctx);
        }
        AllFacilityIterator facilities(m_system, kUsableFilter, m_system->ControlSide());
        facilities.Begin();
        while (ok != 0 && facilities.Current() != nullptr && stop == 0) {
            GameObjNotifier* facility = facilities.Current();
            facilities.Advance();
            ok = visitor->Visit(static_cast<GameObj*>(facility), nullptr, 0, 0, &stop, ctx);
        }
    }
    if (m_hitProduction != 0) {
        ProcFacilIterator production(m_system, kUsableFilter, m_system->ControlSide());
        production.Begin();
        while (ok != 0 && production.Current() != nullptr && stop == 0) {
            GameObjNotifier* facility = production.Current();
            production.Advance();
            ok = visitor->Visit(static_cast<GameObj*>(facility), nullptr, 0, 0, &stop, ctx);
        }
    }
    if (m_hitResources != 0 && ok != 0) {
        if (m_system->m_energy != 0 && stop == 0) ok = visitor->Visit(nullptr, nullptr, 1, 0, &stop, ctx);
        if (ok != 0 && m_system->m_rawMaterial != 0 && stop == 0)
            ok = visitor->Visit(nullptr, nullptr, 0, 1, &stop, ctx);
    }
    return ok;
}

int BombardWalker::DestroyDefender(GameObj* object, NotifyContext* ctx) {
    if (inFamilies(object, kProcFacilityFamilies)) m_damaged = 1;
    const int reason = object->ControlSide() == m_attackerSide ? kFriendlyFireReason : kBombardedReason;
    return object->ChangeDestroyedReason(reason, ctx);
}

int BombardWalker::DestroyEnergy(NotifyContext* ctx) {
    const int energy = m_system->m_energy;
    if (energy == 0) return 0;
    m_damaged = 1;
    return m_system->SetEnergy(energy - 1, ctx);
}

int BombardWalker::DestroyRawMaterial(NotifyContext* ctx) {
    const int rawMaterial = m_system->m_rawMaterial;
    if (rawMaterial == 0) return 0;
    m_damaged = 1;
    return m_system->SetRawMaterial(rawMaterial - 1, ctx);
}

int BombardWalker::GetDefenderStrength(const DefenderRef* ref, int* out) {
    *out = 0;
    if (ref->m_facility != nullptr) {
        Facility* facility = static_cast<Facility*>(ref->m_facility);
        const int loss = facility->Virtual113();
        *out = static_cast<int>(facility->GetRecordField58()) - loss;
        return 1;
    }
    if (ref->m_troop != nullptr) {
        TroopReg* troop = static_cast<TroopReg*>(ref->m_troop);
        const int loss = troop->Virtual121();
        *out = static_cast<int>(troop->GetBombardmentDefense()) - loss;
        return 1;
    }
    if (ref->m_energy != 0) {
        *out = g_gnprtb0604;
        return 1;
    }
    if (ref->m_rawMaterial != 0) *out = g_gnprtb0605;
    return 1;
}

int BombardWalker::DestroyDefenderRef(const DefenderRef* ref, NotifyContext* ctx) {
    int result = 1;
    if (ref->m_facility != nullptr) return DestroyDefender(static_cast<GameObj*>(ref->m_facility), ctx);
    if (ref->m_troop != nullptr) return DestroyDefender(static_cast<GameObj*>(ref->m_troop), ctx);
    if (ref->m_energy != 0) return DestroyEnergy(ctx);
    if (ref->m_rawMaterial != 0) result = DestroyRawMaterial(ctx);
    return result;
}

int BombardWalker::PickRandomProcFacility(int* found, DefenderRef* ref) {
    *found = 0;
    ref->m_facility = nullptr;
    ref->m_troop = nullptr;
    ref->m_energy = 0;
    ref->m_rawMaterial = 0;
    ProcFacilIterator production(m_system, kUsableFilter, m_system->ControlSide());
    const int count = production.Count();
    if (count != 0) {
        const int pick = GameRandom(count - 1);
        int index = 0;
        GameObjNotifier* chosen = nullptr;
        production.Begin();
        // BUG(original): the iterator is never advanced, so the first facility is the only candidate.
        while (production.Current() != nullptr && chosen == nullptr) {
            if (index < pick)
                ++index;
            else
                chosen = production.Current();
        }
        ref->m_facility = chosen;
        *found = 1;
    }
    return 1;
}

// ---- resolver ----------------------------------------------------------------------------------------------------

BombardResolver::BombardResolver() : m_unused4(1), m_walker() {}
BombardResolver::~BombardResolver() = default;

int BombardResolver::Run(GameObjNotifier* system, ActivityRequest* request, NotifyContext* ctx) {
    int consistent = 1;
    int hitDefenses = 0;
    int hitProduction = 0;
    int hitResources = 0;
    int collateral = 0;
    int deathStar = 0;
    switch (request->m_kind) {
        case 0x220:
            hitDefenses = 1;
            collateral = 1;
            break;
        case 0x221:
            hitProduction = 1;
            break;
        case 0x222:
            hitDefenses = 1;
            hitProduction = 1;
            hitResources = 1;
            break;
        case 0x223:
            hitDefenses = 1;
            hitProduction = 1;
            hitResources = 1;
            deathStar = 1;
            break;
        default:
            consistent = 0;
    }
    uint32_t attackerSide = 0;
    int result = 0;
    if (consistent != 0) {
        FleetIterator fleets(system, kUsableFilter);
        fleets.Begin();
        do {
            if (fleets.Current() == nullptr) break;
            if ((static_cast<Fleet*>(fleets.Current())->m_fleetFlags >> 6 & 1) != 0) {
                if (attackerSide == 0)
                    attackerSide = fleets.Current()->ControlSide();
                else
                    consistent = attackerSide == fleets.Current()->ControlSide();
            }
            fleets.Advance();
        } while (consistent != 0);
        if (consistent != 0 && attackerSide != 0) {
            m_walker.m_system = static_cast<System*>(system);
            m_walker.m_attackerSide = attackerSide;
            m_walker.m_defenderSide = system->ControlSide();
            m_walker.m_hitDefenses = hitDefenses;
            m_walker.m_hitProduction = hitProduction;
            m_walker.m_hitResources = hitResources;
            m_walker.m_collateralAllowed = collateral;
            m_walker.m_deathStar = deathStar;
            result = RunPhases(ctx);
        }
    }
    return result;
}

int BombardResolver::RunPhases(NotifyContext* ctx) {
    if (m_walker.m_system != nullptr && m_walker.m_attackerSide != 0) {
        int ok = StartBombard(ctx) != 0;
        ok = both(DefensesFireAtShips(ctx), ok);
        ok = both(ShipsFireAtDefenders(ctx), ok);
        ok = both(FinishBombard(ctx), ok);
        if (ok != 0) return 1;
    }
    return 0;
}

int BombardResolver::StartBombard(NotifyContext* ctx) {
    int ok = m_walker.m_system->SetBombard(1, ctx) != 0;
    int deathStarPresent = 0;
    if (m_walker.m_deathStar != 0) {
        DeathStarFinder finder(&m_walker);
        ok = both(m_walker.ForEachBombardingShip(&finder, ctx), ok);
        deathStarPresent = finder.m_found;
    }
    if (deathStarPresent != 0) {
        ok = both(m_walker.DestroyDefender(m_walker.m_system, ctx), ok);
        m_walker.m_planetDestroyed = 1;
        m_walker.m_damaged = 1;
        MinorCharacterIterator characters(m_walker.m_system, kExistingFilter);
        for (characters.Begin(); characters.Current() != nullptr; characters.Advance()) {
            Character* character = static_cast<Character*>(characters.Current());
            if (((character->m_stateFlags >> 5) & 1) != 0) continue;
            int dies = 0;
            ok = both(RollChance0606(&dies), ok);
            if (dies == 0) continue;
            ok = both(character->Virtual188(1, kPlanetDeathReason, ctx), ok);
            if (((character->m_stateFlags >> 6) & 1) != 0)
                ok = both(character->ChangeDestroyedReason(kPlanetDeathReason, ctx), ok);
        }
        m_walker.m_shipsGone = 1;
    } else {
        ShipCounter ships(&m_walker);
        ok = both(m_walker.ForEachBombardingShip(&ships, ctx), ok);
        m_walker.m_shipCount = ships.m_count;
        DefenderCounter defenders(&m_walker);
        ok = both(m_walker.ForEachDefender(&defenders, ctx), ok);
        m_walker.m_defenderCount = defenders.m_count;
        m_walker.m_collateral = m_walker.m_collateralAllowed != 0;
    }
    return ok;
}

int BombardResolver::DefensesFireAtShips(NotifyContext* ctx) {
    int ok = 1;
    DefenseFacility22Iterator turrets(m_walker.m_system, kUsableFilter);
    turrets.Begin();
    while (turrets.Current() != nullptr) {
        ShipPicker picker(&m_walker);
        picker.m_target = GameRandom(m_walker.m_shipCount - 1);
        if (ok != 0) {
            ok = m_walker.ForEachBombardingShip(&picker, ctx);
            GameObj* ship = picker.m_ship;
            if (ok != 0 && ship != nullptr) {
                const int rating = static_cast<Mission*>(turrets.Current())->Mission::GetRecordHidden();
                GameObjNotifier* commander = nullptr;
                ok = m_walker.m_system->FindCharacterByCommandKind(kCommanderNumber, &commander);
                const int leadership = ok != 0 ? commanderLeadership(commander) : 0;
                if (ok != 0) {
                    int damage = 0;
                    ok = ComputeScaledByGnprtb0600(rating, leadership, &damage);
                    if (ok != 0) static_cast<CapShip*>(ship)->SetDataWord20(static_cast<CapShip*>(ship)->GetDataWord20() + damage);
                }
            }
        }
        turrets.Advance();
        if (ok == 0) break;
    }
    DefenseFacility23Iterator cannons(m_walker.m_system, kUsableFilter);
    cannons.Begin();
    while (ok != 0 && cannons.Current() != nullptr) {
        ShipPicker picker(&m_walker);
        picker.m_target = GameRandom(m_walker.m_shipCount - 1);
        if (ok != 0) {
            ok = m_walker.ForEachBombardingShip(&picker, ctx);
            GameObj* ship = picker.m_ship;
            if (ok != 0 && ship != nullptr) {
                const int rating = static_cast<Mission*>(cannons.Current())->Mission::GetRecordHidden();
                GameObjNotifier* commander = nullptr;
                ok = m_walker.m_system->FindCharacterByCommandKind(kCommanderNumber, &commander);
                const int leadership = ok != 0 ? commanderLeadership(commander) : 0;
                if (ok != 0) {
                    int damage = 0;
                    ok = ComputeScaledByGnprtb0600(rating, leadership, &damage);
                    if (ok != 0) static_cast<CapShip*>(ship)->SetDataWord24(reinterpret_cast<ProcFacil*>(ship)->ProcFacil::GetCycleCount() + damage);
                }
            }
        }
        cannons.Advance();
        if (ok == 0) break;
    }
    ShipDamager damager(&m_walker);
    if (ok != 0) ok = m_walker.ForEachBombardingShip(&damager, ctx);
    if (ok != 0) {
        ShipCounter survivors(&m_walker);
        ok = m_walker.ForEachBombardingShip(&survivors, ctx) != 0;
        if (survivors.m_count == 0) m_walker.m_shipsGone = 1;
    }
    return ok;
}

int BombardResolver::ShipsFireAtDefenders(NotifyContext* ctx) {
    int ok = 1;
    if (m_walker.m_shipsGone != 0) return ok;
    int total = 0;
    FleetIterator fleets(m_walker.m_system, kUsableFilter, m_walker.m_attackerSide);
    fleets.Begin();
    while (fleets.Current() != nullptr) {
        Fleet* fleet = static_cast<Fleet*>(fleets.Current());
        if (((fleet->m_fleetFlags >> 6) & 1) != 0) {
            const int strength = fleet->SumUnitStat119();
            int leadership = 0;
            int bonus = 0;
            if (ok != 0) {
                GameObjNotifier* commander = nullptr;
                ok = fleet->FindShipByNumber(kLieutenantNumber, &commander);
                if (ok != 0 && commander != nullptr) leadership = commanderLeadership(commander);
                if (ok != 0) {
                    ok = ComputeScaledByGnprtb0601(strength, leadership, &bonus);
                    if (ok != 0) total += bonus;
                }
            }
        }
        fleets.Advance();
        if (ok == 0) break;
    }

    int shielding = 0;
    DefenseFacility24Iterator shields(m_walker.m_system, kUsableFilter, m_walker.m_system->ControlSide());
    shields.Begin();
    while (ok != 0 && shields.Current() != nullptr) {
        shielding += static_cast<Mission*>(shields.Current())->Mission::GetRecordDetectionPhases();
        shields.Advance();
    }
    int remaining = 0;
    if (ok != 0 && total > shielding) remaining = total - shielding;

    if (remaining != 0 && m_walker.m_collateralAllowed != 0) {
        const int collateral = m_walker.m_collateral;
        if (GameRandom(m_walker.m_defenderCount - 1 + collateral) < collateral) {
            int found = 0;
            DefenderRef ref;
            m_walker.PickRandomProcFacility(&found, &ref);
            int strength = 0;
            unsigned fired = 0;
            ok = m_walker.GetDefenderStrength(&ref, &strength);
            if (ok != 0) {
                ok = RollBelowRange0602(strength, &fired);
                if (ok != 0 && fired != 0) ok = m_walker.DestroyDefenderRef(&ref, ctx);
            }
        }
    }

    for (int shot = 0; ok != 0 && m_walker.m_shipsGone == 0 && shot < remaining; ++shot) {
        DefenderPicker picker(&m_walker);
        picker.m_target = GameRandom(m_walker.m_defenderCount - 1);
        if (ok != 0) ok = m_walker.ForEachDefender(&picker, ctx);
        int found = 0;
        DefenderRef ref;
        picker.GetPicked(&found, &ref);
        if (ok != 0 && found != 0) {
            int strength = 0;
            unsigned fired = 0;
            ok = m_walker.GetDefenderStrength(&ref, &strength);
            if (ok != 0) {
                ok = RollBelowRange0602(strength, &fired);
                if (ok != 0 && fired != 0) {
                    ok = m_walker.DestroyDefenderRef(&ref, ctx);
                    DefenderCounter left(&m_walker);
                    ok = both(m_walker.ForEachDefender(&left, ctx), ok);
                    if (left.m_count == 0) m_walker.m_shipsGone = 1;
                }
            }
        }
    }
    return ok;
}

int BombardResolver::FinishBombard(NotifyContext* ctx) {
    System* system = m_walker.m_system;
    const uint32_t attackerSide = m_walker.m_attackerSide;
    ShipCleaner cleaner(&m_walker);
    int ok = m_walker.ForEachBombardingShip(&cleaner, ctx) != 0;
    if (m_walker.m_damaged != 0) {
        const int result = system->AdjustLoyalty(GetParameter1E19(), attackerSide, ctx);
        ok = both(result, ok);
    }
    if (m_walker.m_planetDestroyed != 0) {
        LoyaltyRule first;
        BuildCaptureRuleAlt(attackerSide, 1, &first);
        ApplyLoyaltyRuleToAllSectors(&first, ctx);
        LoyaltyRule second;
        ok = BuildCaptureRuleAlt(attackerSide, 0, &second);
        ok = both(ApplyLoyaltyRuleToAllSectors(&second, ctx), ok);
    } else if (m_walker.m_damaged != 0) {
        LoyaltyRule first;
        BuildCaptureRule(attackerSide, 1, &first);
        ApplyLoyaltyRuleToParentSector(system, &first, ctx);
        LoyaltyRule second;
        ok = BuildCaptureRule(attackerSide, 0, &second);
        ok = both(ApplyLoyaltyRuleToParentSector(system, &second, ctx), ok);
    }
    ok = both(system->SetBombard(0, ctx), ok);
    FleetIterator fleets(system, kAlwaysFilter, attackerSide);
    for (fleets.Begin(); fleets.Current() != nullptr; fleets.Advance())
        static_cast<Fleet*>(fleets.Current())->SetBombard(0, ctx);
    return ok;
}
