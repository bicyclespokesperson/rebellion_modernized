#include <new>

#include "tactical/tactical_externals_2b.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_side.h"
#include "tactical/tactical_unit_access.h"

namespace {

constexpr int kCapitalGroupKind = 4;
constexpr int kCapitalShipClass = 0;
constexpr int kStateActive = 2;
constexpr int kStateWithdrawing = 5;
constexpr int kStateDocked = 1;
constexpr int kStateLanding = 3;
constexpr float kDeathStarReady = 100.0f;
constexpr float kDeathStarBonus = 20000000.0f;
constexpr float kRankStep = 1.0f;
constexpr int kDeathStarKnockedOutFlag = 0x78;
constexpr int kDeathStarCoreFlag = 0x74;
constexpr int kUnitHealthOffset = 0x3cc;

SimUnitNode* firstUnitNode(UnitRefTree& tree) { return reinterpret_cast<SimUnitList*>(&tree)->First(); }

SimUnitNode* firstUnitNode(SimUnitList* list) { return list->First(); }

GroupRefNode* firstGroupNode(GroupRefTree& tree) { return static_cast<GroupRefNode*>(tree.FirstNode()); }

GroupRefNode* nextGroupNode(GroupRefNode* node) { return static_cast<GroupRefNode*>(node->Next()); }

TargetPriorityNode* newPriorityNode(float priority, SimUnit* unit) {
    void* memory = ::operator new(sizeof(TargetPriorityNode));
    return memory ? new (memory) TargetPriorityNode(priority, unit) : nullptr;
}

TacticalSideBase* opposingSide(HeadlessSimulator* simulator, int sideIndex) {
    return reinterpret_cast<TacticalSideBase*>(simulator->m_sides[sideIndex == 0 ? 1 : 0]);
}

int kindOfGroup(ShipGroup* group) { return TacticalUnitKind(group); }

}  // namespace

TargetPriorityTree* TacticalAiSide::BuildTargetList() {
    TargetPriorityTree deferred;
    TargetPriorityTree* list = &m_targets[0];
    list->Clear();
    if (m_attackEvaluatorKind == 0) ResetEvaluators();

    const int index = SideIndex();
    TacticalSideBase* enemy = opposingSide(GetHeadlessSimulator(), index);
    for (SimUnitNode* node = firstUnitNode(enemy->m_fleet); node; node = node->Next()) {
        SimUnit* unit = node->Value();
        if (!unit) continue;
        const float score = EvaluateAttack(0, unit);
        TargetPriorityNode* entry = newPriorityNode(-score, unit);
        if (EvaluateUnit4(unit) == 1) {
            list->Insert(entry);
        } else if (unitState(unit) == kStateActive) {
            deferred.Insert(entry);
        } else if (entry) {
            delete entry;
        }
    }
    for (SimUnitNode* node = firstUnitNode(enemy->m_hangars); node; node = node->Next()) {
        SimUnit* unit = node->Value();
        if (!unit || unitState(unit) != kStateActive) continue;
        const float score = EvaluateAttack(0, unit);
        TargetPriorityNode* entry = newPriorityNode(-score, unit);
        if (EvaluateUnit4(unit) == 1) {
            list->Insert(entry);
        } else if (unitState(unit) == kStateActive) {
            deferred.Insert(entry);
        } else if (entry) {
            delete entry;
        }
    }
    for (SimUnitNode* node = firstUnitNode(&GetHeadlessSimulator()->m_launched); node; node = node->Next()) {
        SimUnit* unit = node->Value();
        if (!unit || unitSide(unit) == index) continue;
        const int state = unitState(unit);
        if (state != kStateDocked && state != kStateLanding) continue;
        const float score = EvaluateAttack(0, unit);
        list->Insert(newPriorityNode(-score, unit));
    }
    for (AvlNode* node = deferred.First(); node;) {
        AvlNode* next = node->m_successor;
        deferred.Remove(node);
        TargetPriorityNode* last = static_cast<TargetPriorityNode*>(list->Last());
        if (last) static_cast<TargetPriorityNode*>(node)->m_priority = last->m_priority - -kRankStep;
        list->Insert(node);
        node = next;
    }
    return list;
}

int TacticalAiSide::HasNoShipsAtAll() {
    if (m_unk80 == 0 || m_state != 0) return 0;
    m_strength = 0;
    const int index = SideIndex();
    TacticalSideBase* enemy = opposingSide(GetHeadlessSimulator(), index);
    if (enemy->HasDisabledUnit()) return 0;
    for (SimUnitNode* node = firstUnitNode(m_hangars); node; node = node->Next()) {
        if (FUN_005b1b70(node->Value()) == 1) return 1;
    }
    for (SimUnitNode* node = firstUnitNode(m_fleet); node; node = node->Next()) {
        if (FUN_005b1b70(node->Value()) == 1) return 1;
    }
    if (SideIndex() != 1) return 0;
    TacticalUnit* deathStar = GetTacticalRegistry()->GetDeathStar();
    if (!deathStar) return 0;
    if (FUN_005ba420(deathStar) != 0) return 1;
    if (deathStarField(deathStar, kDeathStarCoreFlag) != 0) return 1;
    return 0;
}

int TacticalAiSide::ResetEvaluators() {
    m_unk1c = 0;
    if (m_attackEvaluator) {
        delete m_attackEvaluator;
        m_attackEvaluator = nullptr;
        m_attackEvaluatorKind = 0;
    }
    if (m_formationEvaluator) {
        delete m_formationEvaluator;
        m_formationEvaluator = nullptr;
        m_formationEvaluatorKind = 0;
    }
    m_formationEvaluatorKind = 3;
    void* memory = ::operator new(sizeof(FormationEvaluator));
    m_formationEvaluator = memory ? new (memory) FormationEvaluator() : nullptr;
    m_attackEvaluatorKind = 4;
    memory = ::operator new(sizeof(AttackEvaluator));
    m_attackEvaluator = memory ? new (memory) AttackEvaluator() : nullptr;
    return m_attackEvaluatorKind;
}

int TacticalAiSide::Refresh(int a) {
    int count = 0;
    m_strength = 0;
    for (SimUnitNode* node = firstUnitNode(m_fleet); node; node = node->Next()) {
        bool engaged = false;
        const int index = SideIndex();
        GroupRefTree& enemyGroups =
            reinterpret_cast<TacticalSideBase*>(GetHeadlessSimulator()->m_sides[index == 1 ? 0 : 1])->m_groups;
        for (GroupRefNode* groupNode = firstGroupNode(enemyGroups); groupNode; groupNode = nextGroupNode(groupNode)) {
            SimUnit* leader = groupNode->Value()->m_formationA.FirstUnit();
            if (leader && unitHandle(leader) == reinterpret_cast<UnitRefNode*>(node)->Key()) {
                const float groupStrength = groupNode->Value()->MemberStrengthAgainst(node->Value());
                if (!(unitVirtuals(node->Value())->Slot25() > groupStrength)) {
                    engaged = true;
                    break;
                }
            }
        }
        if (!engaged) m_strength = FUN_005b7a80(node->Value(), 0, a) + m_strength;
        ++count;
    }
    if (SideIndex() == 1) {
        for (SimUnitNode* node = firstUnitNode(&GetHeadlessSimulator()->m_launched); node; node = node->Next()) {
            m_strength = FUN_005b7a80(node->Value(), 0, a) + m_strength;
            ++count;
        }
    }
    for (SimUnitNode* node = firstUnitNode(m_hangars); node; node = node->Next()) {
        m_strength = FUN_005b7a80(node->Value(), 0, a) + m_strength;
        ++count;
    }
    TacticalUnit* deathStar = GetTacticalRegistry()->GetDeathStar();
    if (deathStar && FUN_005ba420(deathStar)) {
        if (SideIndex() == 1) {
            m_strength = kDeathStarBonus + m_strength;
            return count + 1;
        }
        bool carried = FUN_005afcc0(GetHeadlessSimulator()) != 0;
        if (!carried) {
            for (GroupRefNode* groupNode = firstGroupNode(m_groups); groupNode; groupNode = nextGroupNode(groupNode)) {
                if (groupNode->Value()->m_unk68 == 6 || groupNode->Value()->m_mode == 6) {
                    carried = true;
                    break;
                }
            }
        }
        if (carried) m_strength = kDeathStarBonus + m_strength;
    }
    return count;
}

int TacticalAiSide::SplitIntoGroups(TargetPriorityTree* targets, int least, int most) {
    targets->First();
    int total = targets->Count() & 0xffff;
    int root = 1;
    if (most >= 1) {
        do {
            if (root * root > total) break;
            ++root;
        } while (root <= most);
    }
    int groups;
    if (total > 1) {
        groups = root - 1;
    } else {
        groups = 1;
    }
    const int available = most - least + 1;
    if (groups >= available) groups = available;

    const int perGroup = total / groups;
    int index = 0;
    int created = 0;
    FighterGroup* group = nullptr;
    for (TargetPriorityNode* node = static_cast<TargetPriorityNode*>(targets->First()); node;
         node = static_cast<TargetPriorityNode*>(node->m_successor)) {
        SimUnit* unit = node->m_unit.m_unit;
        if (index % perGroup == 0 && created < groups) {
            ++created;
            void* memory = ::operator new(sizeof(FighterGroup));
            group = memory ? new (memory) FighterGroup(unitHandle(unit)) : nullptr;
            GetHeadlessSimulator();
            FUN_005afde0(group);
            m_groups.AddGroup(group);
            group->SetSeedHandle(unitHandle(unit));
            unitGroupId(unit) = group->m_id;
        } else {
            group->AddMember(unitHandle(unit));
        }
        ++index;
    }
    return index > 0 ? groups : 0;
}

int TacticalAiSide::RecordFleet(SimUnitList*) {
    RecordFleetPriorities();
    return 1;
}

int TacticalAiSide::Virtual11() {
    if (HasNoShipsAtAll()) {
        if (m_unkb0 == 1) return 1;
        Refresh(0);
        if (!(m_strength < 0.0f) && !(m_strength > 0.0f)) return 1;
        HeadlessSimulator* simulator = GetHeadlessSimulator();
        const int index = SideIndex();
        TacticalSideBase* enemy = reinterpret_cast<TacticalSideBase*>(simulator->m_sides[index == 0 ? 1 : 0]);
        enemy->Refresh(0);
        const float enemyStrength = FUN_005afa30(GetHeadlessSimulator(), SideIndex());
        if (enemyStrength > 0.0f) {
            const float ratio = m_strength / enemyStrength;
            if (!(ratio >= m_unk24)) return 1;
            if (!(ratio < 0.0f) && !(ratio > 0.0f)) return 1;
        }
    }
    return 0;
}

int TacticalAiSide::StepSide(float step) {
    HeadlessSimulator* simulator = GetHeadlessSimulator();
    TacticalUnit* deathStar = GetTacticalRegistry()->GetDeathStar();
    if (SideIndex() == 0) {
        if (deathStar && FUN_005ba450(deathStar) == 1) {
            const int enemyHangars = reinterpret_cast<TacticalSideBase*>(simulator->m_sides[1])->m_hangars.CountNodes();
            if (enemyHangars < m_hangars.CountNodes()) {
                for (GroupRefNode* node = firstGroupNode(m_groups); node; node = nextGroupNode(node)) {
                    if (kindOfGroup(node->Value()) == kCapitalGroupKind) {
                        FUN_005aefc0(simulator, node->Value());
                        break;
                    }
                }
            }
        }
    } else {
        if (deathStar && FUN_005ba420(deathStar)) {
            if (deathStarFloat(deathStar, 0x6c) < kDeathStarReady || deathStarFloat(deathStar, 0x6c) > kDeathStarReady) {
                FUN_005ba7f0(deathStar, step);
            }
            if (!(deathStarFloat(deathStar, 0x6c) < kDeathStarReady) &&
                !(deathStarFloat(deathStar, 0x6c) > kDeathStarReady)) {
                BuildTargetList();
                for (AvlNode* node = m_targets[0].First(); node; node = node->m_successor) {
                    SimUnit* unit = static_cast<TargetPriorityNode*>(node)->m_unit.m_unit;
                    if (unitVirtuals(unit)->GetUnitClass() == kCapitalShipClass) {
                        FUN_005ba710(deathStar, unitHandle(unit));
                        FUN_005af7a0(GetHeadlessSimulator(), unit);
                        break;
                    }
                }
            }
        }
        for (SimUnitNode* node = firstUnitNode(&GetHeadlessSimulator()->m_launched); node; node = node->Next()) {
            TacticalUnit* launchInterface =
                GetTacticalRegistry()->FindLaunchInterface(unitCarrierHandle(node->Value()));
            if (launchInterface) FUN_005b8450(launchInterface, node->Value());
            unitState(node->Value()) = kStateActive;
            m_hangars.AddUnit(node->Value());
        }
        reinterpret_cast<SimUnitList*>(&GetHeadlessSimulator()->m_launched)->Clear();
    }

    BuildTargetList();
    ReprioritiseTargets(step);
    for (GroupRefNode* node = firstGroupNode(m_groups); node; node = nextGroupNode(node)) {
        SimUnit* unit = node->Value()->m_formationA.FirstUnit();
        if (unit) {
            const float damage = node->Value()->MemberStrengthAgainst(unit) * step;
            FUN_005b54d0(unit, damage);
            const float health = unitField<float>(unit, kUnitHealthOffset);
            if (!(health < 0.0f) && !(health > 0.0f)) FUN_005af7a0(GetHeadlessSimulator(), unit);
        }
    }

    if (Virtual11() == 1) {
        for (SimUnitNode* node = firstUnitNode(m_hangars); node;) {
            SimUnitNode* next = node->Next();
            if (FUN_005b1b70(node->Value()) == 0) {
                if (FUN_005afe40(GetHeadlessSimulator(), node->Value()) == 0) {
                    FUN_005af7a0(GetHeadlessSimulator(), node->Value());
                }
            } else {
                FUN_005af910(GetHeadlessSimulator(), node->Value());
            }
            node = next;
        }
        if (SideIndex() == 1 && deathStar && FUN_005ba420(deathStar)) {
            deathStarField(deathStar, kDeathStarKnockedOutFlag) = 1;
        }
        for (SimUnitNode* node = firstUnitNode(m_fleet); node;) {
            SimUnitNode* next = node->Next();
            if (FUN_005b1b70(node->Value()) == 1) {
                FUN_005af910(GetHeadlessSimulator(), node->Value());
            } else {
                FUN_005af7a0(GetHeadlessSimulator(), node->Value());
            }
            node = next;
        }
        m_state = kStateWithdrawing;
    }

    int active = 0;
    for (SimUnitNode* node = firstUnitNode(m_fleet); node; node = node->Next()) {
        if (unitState(node->Value()) == kStateActive) ++active;
        FUN_005b6320(node->Value(), step);
    }
    for (SimUnitNode* node = firstUnitNode(m_hangars); node; node = node->Next()) {
        if (unitState(node->Value()) == kStateActive) ++active;
        FUN_005b6320(node->Value(), step);
    }
    if (active != 0) return 0;
    if (SideIndex() != 1 || !deathStar) return 1;
    if (FUN_005ba420(deathStar) != 0) return 0;
    return 1;
}
