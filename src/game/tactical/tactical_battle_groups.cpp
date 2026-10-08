#include <new>

#include "tactical/tactical_externals_2b.h"
#include "tactical/tactical_groups.h"
#include "tactical/tactical_side.h"
#include "tactical/tactical_simulator.h"
#include "tactical/tactical_unit_access.h"

namespace {

constexpr int kFighterClass = 1;
constexpr int kTypeKinds = 4;
constexpr unsigned kMinimumToSplit = 8;
constexpr int kSimulatorUnlaunchedOffset = 0x64;

TacticalAiSide* sideOf(HeadlessSimulator* simulator, int index) {
    return reinterpret_cast<TacticalAiSide*>(simulator->m_sides[index]);
}

UnitRefTree* unlaunchedUnits(HeadlessSimulator* simulator) {
    return reinterpret_cast<UnitRefTree*>(reinterpret_cast<char*>(simulator) + kSimulatorUnlaunchedOffset);
}

SimUnitList* asList(UnitRefTree* tree) { return reinterpret_cast<SimUnitList*>(tree); }

// The lists have their own vtables; the call must stay virtual even for the simulator's own member.
__attribute__((noinline)) SimUnitNode* firstNode(SimUnitList* list) { return list->First(); }

UnitRefNode* firstMemberNode(CapitalShipGroup* group) { return static_cast<UnitRefNode*>(group->FirstNode()); }

// Counts the side's capital ships by type, forms one group per type present and then splits the biggest groups
// (eight ships or more) to give a group to each type that has none.
void formCapitalGroups(HeadlessSimulator* simulator, int side, int firstType) {
    unsigned counts[kTypeKinds] = {0, 0, 0, 0};
    CapitalShipGroup* groups[kTypeKinds];
    for (SimUnitNode* node = firstNode(&simulator->m_units); node; node = node->Next()) {
        if (unitSide(node->Value()) == side) {
            const unsigned offset = node->Value()->Slot09() - firstType;
            if (offset < kTypeKinds) ++counts[node->Value()->Slot09() - firstType];
        }
    }
    for (int i = 0; i < kTypeKinds; ++i) {
        const unsigned count = counts[i];
        groups[i] = nullptr;
        if (count != 0) groups[i] = simulator->FormCapitalGroup(&simulator->m_units, firstType + i, i);
    }
    for (int target = 0; target < kTypeKinds; ++target) {
        if (counts[target] != 0) continue;
        unsigned best = 0;
        int biggest = 0;
        for (int i = 0; i < kTypeKinds; ++i) {
            if (best < counts[i]) {
                best = counts[i];
                biggest = i;
            }
        }
        if (counts[biggest] < kMinimumToSplit) continue;
        const unsigned moved = counts[biggest] >> 1;
        counts[target] = moved;
        counts[biggest] = counts[biggest] - moved;
        UnitRefTree movedUnits;
        CapitalShipGroup* donor = groups[biggest];
        UnitRefNode* node = firstMemberNode(donor);
        int type = firstType;
        UnitRefNode* next = nullptr;
        for (unsigned remaining = counts[target]; remaining != 0; --remaining) {
            if (node) {
                type = node->Value()->Slot09();
                next = static_cast<UnitRefNode*>(node->Next());
                movedUnits.AddUnit(node->Value());
                donor->Virtual21(node->Key());
                donor->RemoveMember(node->Key());
            }
            node = next;
        }
        groups[target] = simulator->FormCapitalGroup(asList(&movedUnits), type, target);
    }
}

void splitFighters(TacticalAiSide* side) {
    side->RecordFleetPriorities();
    int first = side->SplitIntoGroups(&side->m_targets[1], 0, 3);
    int second = side->SplitIntoGroups(&side->m_targets[2], first, first + 3);
    side->SplitIntoGroups(&side->m_targets[3], first + second, 7);
}

}  // namespace

CapitalShipGroup* HeadlessSimulator::FormCapitalGroup(SimUnitList* units, int unitType, int role) {
    bool first = true;
    CapitalShipGroup* group = nullptr;
    for (SimUnitNode* node = firstNode(units); node; node = node->Next()) {
        if (node->Value()->Slot09() != unitType) continue;
        SimUnit* unit = node->Value();
        if (first) {
            first = false;
            void* memory = ::operator new(sizeof(CapitalShipGroup));
            group = memory ? new (memory) CapitalShipGroup(unitHandle(unit)) : nullptr;
            FUN_005afe10(group);
            unitGroupId(unit) = group->m_id;
            group->SetRole(role);
            group->SetSeedHandle(unitHandle(unit));
            sideOf(this, unitSide(unit))->m_groups.AddGroup(group);
        } else {
            group->AddMember(unitHandle(unit));
        }
    }
    return group;
}

void HeadlessSimulator::FormBattleGroups(int) {
    for (SimUnitNode* node = firstNode(&m_units); node; node = node->Next()) {
        SimUnit* unit = node->Value();
        if (unitVirtuals(unit)->GetUnitClass() == kFighterClass) {
            if (FUN_005b69f0(unit) == 0) sideOf(this, unitSide(unit))->m_hangars.AddUnit(unit);
        } else {
            sideOf(this, unitSide(unit))->m_fleet.AddUnit(unit);
        }
        if (FUN_005b69f0(unit) == 0) unlaunchedUnits(this)->AddUnit(unit);
    }
    formCapitalGroups(this, 0, 0x1d);
    splitFighters(sideOf(this, 0));
    formCapitalGroups(this, 1, 0x21);
    splitFighters(sideOf(this, 1));
    sideOf(this, 0)->RecordFleet(&m_units);
    sideOf(this, 1)->RecordFleet(&m_units);
    sideOf(this, 0)->Refresh(0);
    sideOf(this, 1)->Refresh(0);
    m_bestStrength[0] = sideOf(this, 0)->m_strength;
    m_bestStrength[1] = sideOf(this, 1)->m_strength;
}
