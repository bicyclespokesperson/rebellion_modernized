#include <new>
#include "galaxy/system_rules.h"

#include "data/game_math.h"
#include "galaxy/system_rules_1e.h"
#include "galaxy/system_rules_decl.h"
#include "gameobj/object_key.h"

using namespace system_rules;

// Rules about the units stationed in a system (blockades, battles, garrison, fleets' flags) and the helpers that push a
// System value onto one unit.

int System::RecomputeBlockadeAndBattlePending(NotifyContext* ctx) {
    uint32_t blockade = 0;
    uint32_t causedByBattle = 0;
    int battlePending = 0;
    const int alliancePresent = FUN_00509710(this, 1);
    const int empirePresent = FUN_00509710(this, 2);
    const uint32_t side = ControlSide();
    const bool known = side != 0 && side <= 3;
    if (known) {
        int bothPresent = 0;
        if (alliancePresent && empirePresent) {
            bothPresent = 1;
            battlePending = 1;
        }
        causedByBattle = bothPresent;
        if (bothPresent == 0 && ((m_flags & system_flag::kPopulated) || ControlSide() != 3)) {
            ChildIterator fleets(this, &family::kFleets, kStateUsable);
            fleets.Begin();
            while (fleets.Current() && blockade == 0) {
                const uint32_t fleetFlags = fleets.Current()->m_flags;
                const uint32_t systemFlags = GameObjRoot::m_flags;
                fleets.Advance();
                blockade = ((fleetFlags >> 6) & 3) != ((systemFlags >> 6) & 3);
            }
        }
        const uint32_t flags = m_flags;
        if (((flags >> 5) & 1) == blockade) {
            causedByBattle = (flags >> 14) & 1;
        } else {
            causedByBattle = ((flags >> 12) & 1) != causedByBattle;
        }
    }
    int ok = SetBlockade(blockade, ctx) && known;
    ok = SetBattlePending(battlePending, ctx) && ok;
    return SetBattlePendingCausedCurrentBlockade(causedByBattle, ctx) && ok;
}

int System::RecomputeGarrisoned(NotifyContext* ctx) {
    ChildIterator troops(this, &family::kTroopRegiments, kStateUsable);
    return SetGarrisoned(troops.HasCurrent(), ctx);
}

int System::RecomputeSuppressing(NotifyContext* ctx) {
    if ((m_flags & system_flag::kPopulated) && PlayerSide(ControlSide()) != 0 &&
        (m_flags & system_flag::kUprising) && m_troopRegSurplus >= 0)
        return SetSuppressing(1, ctx);
    return SetSuppressing(0, ctx);
}

int System::RecomputeCombatUnitFastRepair(NotifyContext* ctx) {
    GameObjNotifier* site = FUN_00509670(this, 0);
    if (!site) return 0;
    return SetCombatUnitFastRepair(FUN_00528040(site, 0) != 0, ctx);
}

int System::RecomputeFacilityBattleFlags(NotifyContext* ctx) {
    int result = 1;
    const int disrupted = (m_flags & (system_flag::kUprising | system_flag::kBlockade)) != 0;
    ChildIterator facilities(this, &kFacilities28To30, kStateExisting);
    for (facilities.Begin(); facilities.Current(); facilities.Advance())
        result = FUN_0053a7f0(facilities.Current(), disrupted, ctx);
    return result;
}

int System::RecomputeFleetExploredFlags(NotifyContext* ctx) {
    int ok = 1;
    int alliance = 0;
    int empire = 0;
    ChildIterator fleets(this, &family::kFleets, kStateAny);
    for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
        GameObjNotifier* fleet = fleets.Current();
        const uint32_t side = fleet->ControlSide();
        const uint32_t fleetFlags = FieldAt<uint32_t>(fleet, 0x58);
        if ((!((fleetFlags >> 1) & 1) || !((fleetFlags >> 2) & 1)) && ok) ok = 1;
        else ok = 0;
        if (ok && ((FieldAt<uint32_t>(fleet, 0x58) >> 1) & 1)) {
            if (((FieldAt<uint32_t>(fleet, 0x50) >> 3) & 1) == 0) {
                alliance += side == 1;
                empire += side == 2;
            } else {
                FUN_004fe2a0(fleet, 0, ctx);
            }
        }
    }
    int result = 0;
    if (ok) result = alliance < 2 && empire < 2;
    if (alliance == 0) result = FUN_00512d00(this, 1, 1, ctx) && result;
    if (empire == 0) result = FUN_00512d00(this, 2, 1, ctx) && result;
    return result;
}

int System::RecomputeFleetEmptyFlags(NotifyContext* ctx) {
    int ok = 1;
    int alliance = 0;
    int empire = 0;
    ChildIterator fleets(this, &family::kFleets, kStateAny);
    for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
        GameObjNotifier* fleet = fleets.Current();
        const uint32_t side = fleet->ControlSide();
        const uint32_t fleetFlags = FieldAt<uint32_t>(fleet, 0x58);
        if ((!((fleetFlags >> 1) & 1) || !((fleetFlags >> 2) & 1)) && ok) ok = 1;
        else ok = 0;
        if (ok && ((FieldAt<uint32_t>(fleet, 0x58) >> 2) & 1)) {
            ChildIterator contents(fleet, &kEveryFamily, kStateAny);
            if (!contents.HasCurrent() && ((FieldAt<uint32_t>(fleet, 0x50) >> 3) & 1) == 0) {
                alliance += side == 1;
                empire += side == 2;
            } else {
                FUN_004fe310(fleet, 0, ctx);
            }
        }
    }
    int result = 0;
    if (ok) result = alliance < 2 && empire < 2;
    if (alliance == 0) result = FUN_00512d00(this, 1, 0, ctx) && result;
    if (empire == 0) result = FUN_00512d00(this, 2, 0, ctx) && result;
    return result;
}

int System::RecomputeFleetBattleFlags(NotifyContext* ctx) {
    int result = 1;
    ChildIterator fleets(this, &family::kFleets, kStateAny);
    for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
        uint32_t inBattle = 0;
        if (FieldAt<uint32_t>(fleets.Current(), 0x50) & 1) inBattle = (m_flags >> 4) & 1;
        const int ok = FUN_004fe380(fleets.Current(), inBattle, ctx);
        result = ok && result;
    }
    return result;
}

int System::RecomputeFleetBlockadeFlags(NotifyContext* ctx) {
    int result = 1;
    ChildIterator fleets(this, &family::kFleets, kStateExisting);
    for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
        uint32_t blockaded = 0;
        if (FieldAt<uint32_t>(fleets.Current(), 0x50) & 1) blockaded = (m_flags >> 5) & 1;
        const int ok = FUN_004fe3f0(fleets.Current(), blockaded, ctx);
        result = ok && result;
    }
    return result;
}

int System::RefreshSiteKind0(NotifyContext* ctx) {
    GameObjNotifier* site = FUN_00509670(this, 0);
    return site ? FUN_00529dd0(site, ctx) : 0;
}

int System::RefreshSiteKind2(NotifyContext* ctx) {
    GameObjNotifier* site = FUN_00509670(this, 2);
    return site ? FUN_00529dd0(site, ctx) : 0;
}

int System::RefreshSiteKind1(NotifyContext* ctx) {
    GameObjNotifier* site = FUN_00509670(this, 1);
    return site ? FUN_00529dd0(site, ctx) : 0;
}

int System::RecomputeTroopWithdrawPercents(NotifyContext* ctx) {
    int result = 1;
    ChildIterator troops(this, &family::kTroopRegiments, kStateUsable);
    for (troops.Begin(); troops.Current() && result; troops.Advance()) result = PushTroopWithdrawPercent(troops.Current(), ctx);
    return result;
}

int System::RecomputeProductionModifiers(NotifyContext* ctx) {
    int result = 1;
    ChildIterator facilities(this, &kFacilities2cTo30, kStateUsable);
    for (facilities.Begin(); facilities.Current() && result; facilities.Advance())
        result = PushProductionModifier(facilities.Current(), ctx);
    return result;
}

int System::RecomputeFastRepair(NotifyContext* ctx) {
    int result = 1;
    ChildIterator fleets(this, &family::kFleets, kStateExisting);
    for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
        ChildIterator ships(fleets.Current(), &family::kCapitalShips, kStateExisting);
        for (ships.Begin(); ships.Current(); ships.Advance()) {
            const int shipOk = PushFastRepair(ships.Current(), ctx);
            result = shipOk && result;
            ChildIterator squadrons(ships.Current(), &family::kFighterSquadrons, kStateExisting);
            for (squadrons.Begin(); squadrons.Current(); squadrons.Advance()) {
                const int squadronOk = PushFastRepair(squadrons.Current(), ctx);
                result = squadronOk && result;
            }
        }
    }
    ChildIterator ownSquadrons(this, &family::kFighterSquadrons, kStateExisting);
    for (ownSquadrons.Begin(); ownSquadrons.Current(); ownSquadrons.Advance()) {
        const int ok = PushFastRepair(ownSquadrons.Current(), ctx);
        result = ok && result;
    }
    return result;
}

int System::PushTroopWithdrawPercent(GameObjNotifier* troops, NotifyContext* ctx) {
    return FUN_00504470(troops, m_troopRegWithdrawPercent, ctx);
}

int System::PushProductionModifier(GameObjNotifier* facility, NotifyContext* ctx) {
    return FUN_0055a7a0(facility, m_productionModifier, ctx);
}

int System::PushFastRepair(GameObjNotifier* unit, NotifyContext* ctx) {
    ObjectKey ownerKey;
    GetOwnerKeyValue(unit, &ownerKey);
    ObjectKey ownKey;
    GetKey(&ownKey);
    int result = 1;
    if (ownerKey.m_value == ownKey.m_value) {
        const int repair = (m_flags & system_flag::kCombatUnitFastRepair) && ((GameObjRoot::m_flags ^ unit->m_flags) & 0xc0) == 0;
        result = FUN_00557860(unit, repair, ctx);
    }
    return result;
}

ObjectKey* GetOwnerKeyValue(GameObjNotifier* object, ObjectKey* out) {
    ObjectKey key;
    object->GetOwnerKey(&key);
    new (out) ObjectKey(key);
    return out;
}

int System::NoteWithdrawingUnit(GameObjNotifier* unit, int mustBeIdle, NotifyContext* ctx) {
    int result = 1;
    bool candidate = false;
    const uint32_t family = unit->GetFamily();
    if (family < 8 || family > 0xf) candidate = unit->GetFamily() > 0x1b && unit->GetFamily() < 0x20;
    else candidate = true;
    if (candidate) {
        const uint32_t unitState = FieldAt<uint32_t>(unit, 0x50);
        candidate = mustBeIdle == 0 && (m_flags & system_flag::kBattlePending) && (unitState & 0x10) && (unitState & 0x40) &&
                    unit->ControlSide() != static_cast<uint32_t>(FUN_00509ad0(this));
    }
    if (candidate) {
        const uint32_t unitSide = unit->ControlSide();
        int known = unitSide >= 1 && unitSide <= 3 && unitSide != 3;
        if ((m_controlBits & 0xc) == 0) known = SetControlKindBattleWithdrew(unitSide, ctx) && known;
        result = 0;
        if (known) result = unitSide == ((m_controlBits >> 2) & 3);
    }
    return result;
}

int System::CheckHyperspaceBlocked(int sideA, int sideB, NotifyContext* ctx) {
    SystemData* data = static_cast<SystemData*>(m_data);
    const int a = FUN_0053fa60(0x38e, sideA != 3, this, &data->m_stats[5], ctx);
    const int b = FUN_0053fa60(0x38e, sideB != 3, this, &data->m_stats[5], ctx);
    return b != 0 && a != 0;
}

int UpdateSystemFamilyChildren(GameObj* parent, NotifyContext* ctx) {
    const FamilyRange systems(0x90, 0x98);
    return parent->UpdateChildren(&systems, ctx);
}
