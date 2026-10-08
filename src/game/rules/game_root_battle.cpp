#include "fleet/capship.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet_iterator.h"
#include "galaxy/system.h"
#include "galaxy/system_neighbours.h"
#include "galaxy/system_rules.h"
#include "character/character_mgr.h"
#include "gameobj/game_root.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_queries.h"
#include "ai/ai_common.h"
#include "rules/event_rules.h"
#include "rules/loyalty_params.h"
#include "rules/rules_mgr.h"

// The game root's handling of the one system whose battle is pending: the key (+0x8c) and notify context (+0x90) it
// remembers, the questions the battle screen asks (can a side retreat, and how far), and what happens before and after
// the fight.

namespace {

constexpr FamilyRange kSystemFamilies(0x90, 0x98);
constexpr uint32_t kAllianceSide = 1;
constexpr uint32_t kEmpireSide = 2;
constexpr uint32_t kBothSides = 3;
constexpr uint32_t kBattlePendingFlag = 0x1000;
constexpr int kBattleContextTag = 0x226;

int both(int result, int ok) { return result != 0 && ok != 0 ? 1 : 0; }

bool isSystemKey(const ObjectKey& key) {
    return key.Family() >= kSystemFamilies.m_first && key.Family() < kSystemFamilies.m_second;
}

uint32_t opposingSide(uint32_t side) {
    if (side == kBothSides) return kBothSides;
    return side == kAllianceSide ? kEmpireSide : kAllianceSide;
}

RulesMgr* rulesMgr() { return static_cast<RulesMgr*>(GetGameRootMember_bc()); }

int hasFleets(GameObjNotifier* system, uint32_t side) {
    FleetIterator fleets(system, 1, side);
    return fleets.HasCurrent();
}

int hasSquads(GameObjNotifier* system, uint32_t side) {
    FightSquadIterator squads(system, 1, side);
    return squads.HasCurrent();
}

int projectorStillWorks(GameObjNotifier* capShip) {
    CapShip* ship = static_cast<CapShip*>(capShip);
    const int loss = ship->GetGravityWellProjectorLoss();
    return ship->GetGravityWellProjector() - loss != 0;
}

// Whether any capital ship of `side` in the system still has a working gravity well projector.
uint32_t findProjector(GameObjNotifier* system, uint32_t side) {
    uint32_t found = 0;
    FleetIterator fleets(system, 1, side);
    for (fleets.Begin(); fleets.Current() != nullptr && found == 0; fleets.Advance()) {
        CapShipIterator ships(fleets.Current(), 1, side);
        for (ships.Begin(); ships.Current() != nullptr && found == 0; ships.Advance())
            found = projectorStillWorks(ships.Current());
    }
    return found;
}

// A fleet of `side` with a ship that can move, else a squadron that can.
GameObjNotifier* findMobileUnit(GameObjNotifier* system, uint32_t side) {
    GameObjNotifier* found = nullptr;
    FleetIterator fleets(system, 1, side);
    for (fleets.Begin(); fleets.Current() != nullptr && found == nullptr; fleets.Advance()) {
        CapShipIterator ships(fleets.Current(), 1, side);
        for (ships.Begin(); ships.Current() != nullptr && found == nullptr; ships.Advance()) {
            if (static_cast<GameObj*>(ships.Current())->GetTransitSpeed(0) != 0) found = fleets.Current();
        }
    }
    if (found != nullptr) return found;
    FightSquadIterator squads(system, 1, side);
    for (squads.Begin(); squads.Current() != nullptr && found == nullptr; squads.Advance()) {
        if (static_cast<GameObj*>(squads.Current())->GetTransitSpeed(0) != 0) found = squads.Current();
    }
    return found;
}

int markAutorouting(GameObjNotifier* system, uint32_t side, NotifyContext* ctx, int ok) {
    FleetIterator fleets(system, 1, side);
    for (fleets.Begin(); fleets.Current() != nullptr; fleets.Advance())
        ok = both(static_cast<GameObj*>(fleets.Current())->SetAutorouting(1, ctx), ok);
    FightSquadIterator squads(system, 1, side);
    for (squads.Begin(); squads.Current() != nullptr; squads.Advance())
        ok = both(static_cast<GameObj*>(squads.Current())->SetAutorouting(1, ctx), ok);
    return ok;
}

}  // namespace

int GameRoot::GetRootKey(ObjectKey* out) {
    *out = m_key8c;
    return 1;
}

NotifyContext* GameRoot::GetBattleContext() { return &m_ctx90; }

int GameRoot::QueryRetreat(int side, uint32_t* gravityWell, uint32_t* hasMobileUnit, int* canRetreat,
                           uint32_t* retreatDays) {
    *gravityWell = 0;
    *hasMobileUnit = 0;
    *canRetreat = 0;
    *retreatDays = 0;
    GameObjNotifier* system = GetMasterInFamilies_90_98(&m_key8c);
    int valid = system != nullptr;
    if (valid != 0 && side != 2 && side != 1) valid = 0;
    if (valid != 0) *gravityWell = findProjector(system, opposingSide(side));
    GameObjNotifier* unit = nullptr;
    if (valid != 0) unit = findMobileUnit(system, side);
    if (unit == nullptr) return valid;
    *hasMobileUnit = 1;
    ObjectKey retreatSystem;
    if (valid != 0) {
        ObjectKey owner;
        GetOwnerKeyValue(unit, &owner);
        valid = isSystemKey(owner);
    }
    if (valid != 0) {
        RulesMgr* rules = rulesMgr();
        IntPair status;
        ObjectKey target;
        ObjectKey placed;
        ObjectKey owner;
        valid = rules->FindDestination(static_cast<GameObj*>(unit), GetOwnerKeyValue(unit, &owner), &target, 0, 0,
                                       &placed, &status);
        if (valid != 0 && status.m_second == -1) {
            GameObjNotifier* placedObject = GetMasterInFamilies_01_ff(&placed);
            if (placedObject != nullptr) {
                ObjectKey placedOwner;
                placedObject->GetOwnerKey(&placedOwner);
                if (isSystemKey(placedOwner)) {
                    *canRetreat = 1;
                    retreatSystem = placedOwner;
                }
            }
        }
    }
    if (valid != 0 && *canRetreat != 0) {
        System* here = static_cast<System*>(GetMasterInFamilies_90_98(&m_key8c));
        System* there = static_cast<System*>(GetMasterInFamilies_90_98(&retreatSystem));
        valid = here != nullptr && there != nullptr;
        if (valid != 0) {
            MapPosition thereAt;
            MapPosition hereAt;
            there->GetRecordField4c(reinterpret_cast<uint32_t*>(&thereAt));
            here->GetRecordField4c(reinterpret_cast<uint32_t*>(&hereAt));
            *retreatDays = TravelDays(&hereAt, &thereAt);
        }
    }
    return valid;
}

int GameRoot::ClearBattle() {
    const int hasRandom = HasRandomGenerator();
    if (hasRandom != 0) {
        m_key8c.Reset();
        NotifyContext cleared;
        m_ctx90 = cleared;
    }
    return hasRandom;
}

int GameRoot::StartPendingBattle() {
    NotifyContext* ctx = &m_ctx90;
    int ok = HasRandomGenerator();
    if (ok == 0) return ok;
    ok = ClearBattle() != 0;
    for (System* system = static_cast<System*>(GetFirstMasterInFamilies_90_98()); system != nullptr;) {
        if (m_key8c.OrNull() != nullptr) break;
        if (system->GetDataFlagBit3() != 0) {
            ok = both(system->SetDataFlag8(0), ok);
            if ((system->m_flags & kBattlePendingFlag) != 0) {
                ObjectKey systemKey;
                GetObjectKey(system, &systemKey);
                m_key8c = systemKey;
            }
        }
        const FamilyRange systems(0x90, 0x98);
        system = static_cast<System*>(system->FindNextSibling(&systems, 1));
    }
    if (m_key8c.OrNull() == nullptr) {
        m_unk70 = 0;
        return ok;
    }
    m_ctx90.m_reserved14 = CallGameHook2AFC();
    m_ctx90.m_reserved18 = kBattleContextTag;
    System* battleSystem = static_cast<System*>(GetMasterInFamilies_90_98(&m_key8c));
    ok = both(battleSystem != nullptr, ok);
    if (battleSystem != nullptr) ok = both(battleSystem->SetBattle(1, ctx), ok);
    return ok;
}

int GameRoot::PrepareBattle() {
    NotifyContext* ctx = &m_ctx90;
    int ok = HasRandomGenerator();
    if (ok == 0) return ok;
    ok = m_key8c.IsSet() != 0;
    System* system = static_cast<System*>(GetMasterInFamilies_90_98(&m_key8c));
    ok = both(system != nullptr, ok);
    if (system == nullptr) return ok;
    ok = both(system->SetBattle(0, ctx), ok);
    uint32_t attacker = system->GetDataWord64();
    const int allianceHere = hasFleets(system, kAllianceSide) || hasSquads(system, kAllianceSide);
    const int empireHere = hasFleets(system, kEmpireSide) || hasSquads(system, kEmpireSide);
    uint32_t defender = 0;
    if (attacker == 0 && allianceHere != 0 && empireHere != 0) {
        const uint32_t controller = (system->m_controlBits >> 6) & 3;
        attacker = controller == kBothSides ? kBothSides : opposingSide(controller);
    }
    if (attacker != 0) defender = opposingSide(attacker);
    if (attacker != 0) {
        RulesMgr* rules = rulesMgr();
        ok = both(rules != nullptr, ok);
        if (rules != nullptr) {
            FightSquadIterator squads(system, 1, attacker);
            for (squads.Begin(); squads.Current() != nullptr; squads.Advance())
                ok = both(rules->EnforceLocation(static_cast<GameObj*>(squads.Current()), ctx), ok);
        }
    }
    if (defender != 0) ok = markAutorouting(system, defender, ctx, ok);
    return ok;
}

int GameRoot::ResolveBattle() {
    NotifyContext* ctx = &m_ctx90;
    uint32_t winner = 0;
    uint32_t loser = 0;
    if (HasRandomGenerator() == 0) return 0;
    int keySet = m_key8c.IsSet() != 0;
    System* system = static_cast<System*>(GetMasterInFamilies_90_98(&m_key8c));
    int ok = system != nullptr && keySet != 0;
    if (system != nullptr) {
        const int allianceHere = hasFleets(system, kAllianceSide) || hasSquads(system, kAllianceSide);
        const int empireHere = hasFleets(system, kEmpireSide) || hasSquads(system, kEmpireSide);
        ok = !(allianceHere != 0 && empireHere != 0) && ok != 0;
        if (allianceHere != 0 && empireHere == 0) {
            winner = kAllianceSide;
            loser = kEmpireSide;
        } else if (allianceHere == 0 && empireHere != 0) {
            winner = kEmpireSide;
            loser = kAllianceSide;
        }
        if (allianceHere == 0 || empireHere == 0) ok = both(system->SetControlKindBattleWon(winner, ctx), ok);
        if (loser != 0) {
            const int valueSide2 = system->GetDataWord68(2);
            const int valueSide1 = system->GetDataWord68(1);
            ok = both(ApplyStrengthImbalance(system, winner, valueSide1, valueSide2, ctx), ok);
        }
        CharacterMgr* characters = static_cast<CharacterMgr*>(GetGameRootMember_c0());
        ok = both(characters != nullptr, ok);
        if (characters != nullptr) ok = both(characters->ShiftLoyaltyForSystem(system, winner, ctx), ok);
    }
    return both(ClearBattle(), ok);
}
