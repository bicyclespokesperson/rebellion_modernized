#include "galaxy/system_rules.h"
#include "core/icf_stubs.h"

#include "data/game_math.h"
#include "data/random.h"
#include "economy/facility.h"
#include "galaxy/system_rules_1e.h"
#include "galaxy/system_rules_decl.h"
#include "gameobj/object_queries.h"

using namespace system_rules;

// The System "Recompute" rules: each derives one System property (or the state of the units in the system) from the
// current situation and runs the validated setter / the unit's own setter. They return the setters' result (1 = ok).

int System::RecomputeObservation(NotifyContext* ctx) {
    ChildIterator allianceFleets(this, 1, &family::kFleets, kStateUsable);
    ChildIterator empireFleets(this, 2, &family::kFleets, kStateUsable);
    int observedByAlliance = 1;
    if (!allianceFleets.HasCurrent() && ControlSide() != 1) observedByAlliance = 0;
    int observedByEmpire = 0;
    if (empireFleets.HasCurrent() || ControlSide() == 2) observedByEmpire = 1;
    int result = SetObservedByAlliance(observedByAlliance, ctx);
    if (result) result = SetObservedByEmpire(observedByEmpire, ctx);
    return result;
}

int System::RecomputeRawMaterial(NotifyContext* ctx) {
    int result = 1;
    if (m_energy < m_rawMaterial) result = SetRawMaterial(m_energy, ctx);
    return result;
}

int System::RecomputeEnergyAllocated(NotifyContext* ctx) {
    int allocated = 0;
    ChildIterator allocators(this, ControlSide(), &family::kFacilities, kStateExisting);
    for (allocators.Begin(); allocators.Current(); allocators.Advance()) allocated += ReturnOne();

    if (m_energy < allocated) {
        ChildIterator facilities(this, &family::kFacilities, kStateExisting);
        const int facilityCount = facilities.Count();
        ChildIterator mines(this, &kFacilities20To22, kStateExisting);
        const int mineCount = mines.Count();
        const int excess = MaxInt(facilityCount - m_energy, 0);
        if (excess != 0) {
            ObjectKeyValueSet candidates;
            ChildIterator all(this, &family::kFacilities, kStateExisting);
            for (all.Begin(); all.Current(); all.Advance()) {
                ObjectKey key;
                candidates.AddKeyValue(all.Current()->GetKey(&key), 0);
            }
            if (mineCount != 0 && facilityCount > excess) {
                ObjectKey protectedKey;
                protectedKey.m_value = 0x20000005;
                candidates.RemoveKey(&protectedKey);
            }
            ObjectKeyValueSet doomed;
            int remaining = static_cast<uint16_t>(candidates.Count());
            for (int i = 0; i < excess; ++i) {
                const int last = remaining - 1;
                int skip = GameRandom(last);
                ObjectKeyNode* node = candidates.First();
                if (node) {
                    // BUG(original): the random skip walks no list, so the first node is always taken.
                    while (skip != 0) --skip;
                    ObjectKey key;
                    node->GetKey(&key);
                    candidates.RemoveKey(&key);
                    doomed.AddKeyValue(&key, 0);
                    remaining = last;
                }
            }
            for (ObjectKeyNode* node = doomed.First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
                ObjectKey key;
                GameObj* facility = static_cast<GameObj*>(GetMasterInFamilies_20_30(node->GetKey(&key)));
                if (facility) facility->Virtual46(ctx);
            }
        }
    }
    return SetEnergyAllocated(MinInt(allocated, m_energy), ctx);
}

int System::RecomputeRawMaterialAllocated(NotifyContext* ctx) {
    int allocated = 0;
    int result = 1;
    ChildIterator allocators(this, ControlSide(), &kFacilities2cTo2d, kStateExisting);
    for (allocators.Begin(); allocators.Current(); allocators.Advance())
        allocated += static_cast<Facility*>(allocators.Current())->Virtual114();

    if (m_rawMaterial < allocated) {
        ChildIterator sites(this, &kFacilities2cTo2d, kStateExisting);
        const int siteCount = sites.Count();
        int excess = MaxInt(siteCount - m_rawMaterial, 0);
        ChildIterator victims(this, &kFacilities2cTo2d, kStateExisting);
        for (victims.Begin(); victims.Current() && result && excess != 0; --excess) {
            result = static_cast<GameObj*>(victims.Current())->Virtual46(ctx);
            victims.Advance();
        }
    }
    int outcome = 0;
    if (result) outcome = SetRawMaterialAllocated(MinInt(allocated, m_rawMaterial), ctx);
    return outcome;
}

int System::RecomputeSmugglingPercent(NotifyContext* ctx) {
    int percent = 0;
    const uint32_t side = ControlSide();
    const int player = PlayerSide(side);
    if (player != 0) {
        const int deathStar = (m_flags >> 11) & 1;
        const int a = this->System::CountDeathStars(side, 1);
        const int b = this->System::CountTroopRegiments(side, 1);
        const int c = this->System::CountFighterSquadrons(side, 1);
        const int d = this->System::CountCapitalShips(side, 1);
        const int share = this->System::GetLoyaltyFor(player);
        percent = ComputeUprisingPercent(side, share, deathStar, d, c, b, a);
    }
    return SetSmugglingPercent(percent, ctx);
}

int System::RecomputeProductionModifier(NotifyContext* ctx) {
    int modifier = 100;
    const int player = PlayerSide(ControlSide());
    if (player != 0) modifier = ScaleByRatio1E53(this->System::GetLoyaltyFor(player));
    return SetProductionModifier(modifier, ctx);
}

int System::RecomputeTroopRegWithdrawPercent(NotifyContext* ctx) {
    int percent = 100;
    ChildIterator marines(this, &kFacilities22To23, kStateUsable);
    const int marineCount = marines.Count();
    if ((m_flags & system_flag::kBlockade) && marineCount == 0) {
        int shipCount = 0;
        int units = 0;
        ChildIterator fleetList(this, &family::kFleets, kStateUsable);
        for (fleetList.Begin(); fleetList.Current(); fleetList.Advance()) {
            ChildIterator ships(fleetList.Current(), &family::kCapitalShips, kStateUsable);
            for (ships.Begin(); ships.Current(); ships.Advance()) {
                ++shipCount;
                ChildIterator squadrons(ships.Current(), &family::kFighterSquadrons, kStateUsable);
                units += squadrons.Count();
            }
        }
        ChildIterator ownSquadrons(this, &family::kFighterSquadrons, kStateUsable);
        percent = ComputeResidualPercent(shipCount, units + ownSquadrons.Count());
    }
    return SetTroopRegWithdrawPercent(percent, ctx);
}

int System::RecomputeControlKindUprising(NotifyContext* ctx) {
    if (m_flags & system_flag::kUprising) return SetControlKindUprising(ControlSide(), ctx);
    return SetControlKindUprising(0, ctx);
}

int System::RecomputeTroopRegSurplus(NotifyContext* ctx) {
    int surplus = 0;
    if (PlayerSide(ControlSide()) != 0) {
        ChildIterator troops(this, &family::kTroopRegiments, kStateUsable);
        surplus = troops.Count() - m_troopRegRequired;
    }
    return SetTroopRegSurplus(surplus, ctx);
}

int System::RecomputeTroopRegRequired(NotifyContext* ctx) {
    int required = 0;
    const uint32_t side = ControlSide();
    const int player = PlayerSide(side);
    if (player != 0 && (m_flags & system_flag::kPopulated)) {
        const int uprising = (m_flags >> 2) & 1;
        const int deathStar = (m_flags >> 11) & 1;
        required = ComputeSupportCost(side, this->System::GetLoyaltyFor(player), deathStar, uprising, 1);
    }
    return SetTroopRegRequired(required, ctx);
}

int System::RecomputeControlData(NotifyContext* ctx) {
    SideBits bits(m_controlData);
    bits.SetLoyalty((m_flags & system_flag::kPopulated) ? ClassifyPercent(m_loyalty) : 3);
    ChildIterator allianceTroops(this, 1, &family::kTroopRegiments, kStateUsable);
    const int alliancePresent = allianceTroops.HasCurrent();
    ChildIterator empireTroops(this, 2, &family::kTroopRegiments, kStateUsable);
    const int empirePresent = empireTroops.HasCurrent();
    uint32_t garrison = bits.Garrison();
    if (!alliancePresent) {
        if (!empirePresent) garrison = 0;
        else if (!alliancePresent) garrison = 2;
    } else if (!empirePresent) {
        garrison = 1;
    }
    bits.SetGarrison(garrison);
    bits.SetController((bits.m_value & 0x30) == 0 ? bits.Loyalty() : bits.Garrison());
    return SetControlData(&bits, ctx) != 0;
}

int System::RecomputePopulated(NotifyContext* ctx) {
    int result = 1;
    if (!(m_flags & system_flag::kPopulated) && !(m_stateFlags & 8)) {
        ChildIterator facilities(this, &family::kFacilities, kStateUsable);
        result = SetPopulated(facilities.HasCurrent(), ctx);
    }
    if (result && (m_stateFlags & 8)) result = SetPopulated(0, ctx);
    return result;
}

int System::RecomputeUprising(NotifyContext* ctx) {
    int uprising = 0;
    const uint32_t side = ControlSide();
    if ((m_flags & system_flag::kPopulated) && (side == 1 || side == 2)) {
        if ((m_flags & system_flag::kUprising) || m_troopRegSurplus < 0) uprising = 1;
    }
    const int ok = SetUprising(uprising, ctx) != 0;
    int result = ok;
    if (((m_flags >> 2) & 1) == static_cast<uint32_t>(uprising)) {
        uint32_t kindSide = side;
        if (uprising == 0) kindSide = 0;
        const int kindResult = SetControlKindUprising(kindSide, ctx);
        result = kindResult && ok;
    }
    return result;
}

int System::RecomputeNeverBeenControlled(NotifyContext* ctx) {
    int result = 1;
    if (m_flags & system_flag::kNeverBeenControlled) {
        const int keep = ControlSide() != 3 && (m_flags & system_flag::kPopulated);
        result = SetNeverBeenControlled(!keep, ctx) != 0;
    }
    return result;
}
