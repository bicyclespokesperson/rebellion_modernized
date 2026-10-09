#include <new>

#include "tactical/tactical_externals_2b.h"
#include "tactical/tactical_groups.h"
#include "tactical/tactical_side.h"
#include "tactical/tactical_simulator.h"
#include "tactical/tactical_ship.h"

namespace {

constexpr int kFighterClass = 1;
constexpr int kTypeKinds = 4;
constexpr unsigned kMinimumToSplit = 8;

TacticalAiSide* sideOf(HeadlessSimulator* simulator, int index) {
    return reinterpret_cast<TacticalAiSide*>(simulator->m_sides[index]);
}

// The lists have their own vtables; the call must stay virtual even for the simulator's own member.
__attribute__((noinline)) UnitRefNode* firstNode(UnitRefTree* list) { return list->FirstNode(); }

UnitRefNode* firstMemberNode(CapitalShipGroup* group) { return static_cast<UnitRefNode*>(group->FirstNode()); }

// Counts the side's capital ships by type, forms one group per type present and then splits the biggest groups
// (eight ships or more) to give a group to each type that has none.
void formCapitalGroups(HeadlessSimulator* simulator, int side, int firstType) {
    unsigned counts[kTypeKinds] = {0, 0, 0, 0};
    CapitalShipGroup* groups[kTypeKinds];
    for (UnitRefNode* node = firstNode(&simulator->m_units); node; node = node->Next()) {
        if (node->Value()->m_side == side) {
            const unsigned offset = node->Value()->GetTypeIndex() - firstType;
            if (offset < kTypeKinds) ++counts[node->Value()->GetTypeIndex() - firstType];
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
                type = node->Value()->GetTypeIndex();
                next = node->Next();
                movedUnits.AddUnit(node->Value());
                donor->Virtual21(node->Key());
                donor->RemoveMember(node->Key());
            }
            node = next;
        }
        groups[target] = simulator->FormCapitalGroup(&movedUnits, type, target);
    }
}

void splitFighters(TacticalAiSide* side) {
    side->RecordFleetPriorities();
    int first = side->SplitIntoGroups(&side->m_targets[1], 0, 3);
    int second = side->SplitIntoGroups(&side->m_targets[2], first, first + 3);
    side->SplitIntoGroups(&side->m_targets[3], first + second, 7);
}

}  // namespace

CapitalShipGroup* HeadlessSimulator::FormCapitalGroup(UnitRefTree* units, int unitType, int role) {
    bool first = true;
    CapitalShipGroup* group = nullptr;
    for (UnitRefNode* node = firstNode(units); node; node = node->Next()) {
        if (node->Value()->GetTypeIndex() != unitType) continue;
        TacticalShipBase* unit = node->Value();
        if (first) {
            first = false;
            void* memory = ::operator new(sizeof(CapitalShipGroup));
            group = memory ? new (memory) CapitalShipGroup(static_cast<int>(unit->m_id)) : nullptr;
            RegisterCapitalShipGroup(group);
            unit->m_groupHandle = group->m_id;
            group->SetRole(role);
            group->SetSeedHandle(static_cast<int>(unit->m_id));
            sideOf(this, unit->m_side)->m_groups.AddGroup(group);
        } else {
            group->AddMember(static_cast<int>(unit->m_id));
        }
    }
    return group;
}

void HeadlessSimulator::FormBattleGroups(int) {
    for (UnitRefNode* node = firstNode(&m_units); node; node = node->Next()) {
        TacticalShipBase* unit = node->Value();
        if (unit->GetUnitClass() == kFighterClass) {
            if (unit->IsState1() == 0) sideOf(this, unit->m_side)->m_hangars.AddUnit(unit);
        } else {
            sideOf(this, unit->m_side)->m_fleet.AddUnit(unit);
        }
        if (unit->IsState1() == 0) m_active.AddUnit(unit);
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
