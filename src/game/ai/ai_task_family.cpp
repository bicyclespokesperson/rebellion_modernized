#include "ai/ai_task_family.h"

#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_task_util.h"
#include "core/checked_new.h"
#include "economy/defense_facility.h"
#include "economy/manufacturing_facility.h"
#include "economy/production_facility_types.h"
#include "galaxy/system.h"
#include "gameobj/object_lookup.h"
#include "orders/orders_210_216.h"

namespace aitask {

namespace {

constexpr int kScrapOrder = 0x200;
constexpr int kDestinationOrder = 0x214;
constexpr uint32_t kSystemDropped = 0x10000000;

AiSystemEval* EvalOf(const FamilyFields& f, const ObjectKey& key) { return f.owner->m_systems.FindEval(key); }

ObjectKeyValueNode* AddAllOfFamily(FacilityKind kind, ObjectKeyValueSet* team, GameObjNotifier* system, ObjectKeyValueNode* last) {
    switch (kind) {
    case FacilityKind::kManufacturing28: return AddAllOfKind<ManufacturingFacility28Iterator>(team, system, last);
    case FacilityKind::kManufacturing29: return AddAllOfKind<ManufacturingFacility29Iterator>(team, system, last);
    case FacilityKind::kManufacturing2A: return AddAllOfKind<ManufacturingFacility2AIterator>(team, system, last);
    }
    return last;
}

ObjectKeyValueNode* FirstNodeOf(ChildIterator* it, ObjectKeyValueSet* team) {
    it->Begin();
    return AddCurrentToTeam(team, it);
}

}  // namespace

int Need(const FamilyFields& f, const FamilyParams& p) {
    AiSideController* owner = f.owner;
    int need = (((f.ratio * owner->Stat(0x184)) / 100) * p.needPercent) / 100 - owner->Stat(p.usedStat);
    if (owner->Stat(0x184) - owner->Stat(0x188) < need) need = 0;
    return need;
}

int NeedForTargets(const FamilyFields& f, const FamilyParams& p) {
    int used = 0;
    for (ObjectKeyNode* node = f.targets->First(); node; node = NextNode(node)) {
        AiSystemEval* eval = EvalOf(f, KeyOfNode(node));
        if (eval) used += eval->Stat(p.supplyStat);
    }
    const int total = f.owner->Stat(0x184);
    int need = (((f.ratio * total) / 100) * p.targetPercent) / 100 - used;
    if (total - f.owner->Stat(0x188) < need) need = 0;
    return need;
}

int ComputeCount(const FamilyFields& f, const FamilyParams& p) {
    int result = 0;
    *f.count = 0;
    const int need = NeedForTargets(f, p);
    if (need < 1) {
        if (need < 0) result = p.lackStepCode;
        return result;
    }
    const int classes = f.owner->ChooseClass(f.classKey, 0x2a, p.classFlags, 0x4000, 2);
    if ((f.classKey->m_value & 0xff000000) != 0 && classes != 0) *f.count = need / classes;
    if (0 < *f.count) {
        if (*f.cap < *f.count) *f.count = *f.cap;
        AiSystemEval* eval = EvalOf(f, *f.system);
        if (eval) {
            int limit = eval->Stat(0x84);
            if (*f.limit < eval->Stat(0x84)) limit = *f.limit;
            if (limit < *f.count) *f.count = limit;
            return p.countStepCode;
        }
    }
    return result;
}

OrderBase* BuildScrapOrder(const FamilyFields& f, const FamilyParams& p) {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    ObjectKeyValueNode* last = nullptr;
    if (!IsSystemKey(*f.system)) return order;
    AiSystemEval* eval = EvalOf(f, *f.system);
    if (!eval) return order;
    GameObjNotifier* system = GetObjectInFamilies_90_98(f.side, f.system);
    if (system) {
        if ((eval->m_flags28.m_value & p.scrapFirstMask) == 0) last = AddAllOfFamily(p.scrapFirst, &team, system, last);
        if ((eval->m_flags28.m_value & p.scrapSecondMask) == 0) last = AddAllOfFamily(p.scrapSecond, &team, system, last);
        last = AddAllOfKind<DefenseFacility25Iterator>(&team, system, last);
        if (eval->Stat(0x84) == 0 && last == nullptr) {
            AiSideController* owner = f.owner;
            if (owner->Stat(0x1d0) < owner->Stat(0x1d4) || eval->Stat(0x50) == 0) {
                ProductionFacilityType2DIterator it(system, 1);
                last = FirstNodeOf(&it, &team);
            }
            if (last == nullptr && (owner->Stat(0x1d4) <= owner->Stat(0x1d0) || eval->Stat(0x54) == 0)) {
                ProductionFacilityType2CIterator it(system, 1);
                last = FirstNodeOf(&it, &team);
            }
        }
    }
    if (team.First()) order = NewTeamOrder(kScrapOrder, f.side, &team);
    return order;
}

OrderBase* BuildCapitalOrder(const FamilyFields& f, const FamilyParams& p, FacilityKind kind) {
    OrderBase* order = nullptr;
    bool found = false;
    ObjectKeyNode* node = f.targets->First();
    while (node) {
        if (found) break;
        AiSystemEval* eval = EvalOf(f, KeyOfNode(node));
        found = false;
        if (eval) {
            const bool several = 1 < f.targets->Count();
            if (eval->Stat(p.shortStat) < 1) {
                ObjectKeyNode* next = NextNode(node);
                ObjectKey key = KeyOfNode(node);
                f.targets->RemoveKey(&key);
                eval->m_flags28.m_value &= ~p.claimBit;
                node = next;
            } else if ((eval->m_flags28.m_value & 0x1000) == 0) {
                node = NextNode(node);
            } else if (eval->Stat(p.shortStat) < 2) {
                found = several;
                if (found) {
                    *f.system = KeyOfNode(node);
                    ObjectKey key = KeyOfNode(node);
                    f.targets->RemoveKey(&key);
                    eval->m_flags28.m_value &= ~p.claimBit;
                }
                node = nullptr;
            } else {
                *f.system = KeyOfNode(node);
                found = true;
            }
        }
    }
    if (!found) return order;
    ObjectKeyValueSet team;
    GameObjNotifier* system = GetObjectInFamilies_90_98(f.side, f.system);
    if (system) {
        ObjectKeyValueNode* made = nullptr;
        switch (kind) {
        case FacilityKind::kManufacturing28: made = AddFirstNode<ManufacturingFacility28Iterator>(&team, system); break;
        case FacilityKind::kManufacturing29: made = AddFirstNode<ManufacturingFacility29Iterator>(&team, system); break;
        case FacilityKind::kManufacturing2A: made = AddFirstNode<ManufacturingFacility2AIterator>(&team, system); break;
        }
        if (made) order = NewTeamOrder(kScrapOrder, f.side, &team);
    }
    return order;
}

int SelectTarget(const FamilyFields& f, const FamilyParams& p) {
    int result = 0;
    int best = 1000;
    ObjectKey scratch;
    AiSideController* owner = f.owner;
    f.system->Reset();
    *f.limit = 0;
    for (ObjectKeyNode* node = f.targets->First(); node; node = NextNode(node)) {
        AiSystemEval* eval = EvalOf(f, KeyOfNode(node));
        if (eval && (eval->m_flags30.m_value & 1) != 0 && (eval->m_flags28.m_value & 3) == 0 && 0 < eval->Stat(0x114) &&
            eval->Stat(p.shortStat) < best) {
            *f.limit = eval->Stat(0x114);
            best = eval->Stat(p.shortStat);
            *f.system = KeyOfNode(eval);
            result = 1;
        }
    }
    if (result == 0) {
        f.selector->Add(owner->RankSectors(p.sectorRequire, 0, 0, 0, 0, 0, p.sectorStat, 1));
        if (f.targets->Count() != 0) f.selector->Add(owner->RankSectors(p.sectorRequire, 0, 0, 0, 0, 0, 4, 2));
        *f.system = *f.selector->PickBest(&scratch);
        f.selector->Reset();
        f.selector->Add(owner->RankSystemsOfSector(f.system, 0, 0, 1, p.systemForbid, 0, 0x40000000, p.systemStat, 1));
        if (f.targets->Count() != 0)
            f.selector->Add(owner->RankSystemsOfSector(f.system, 0, 0, 1, p.systemForbid, 0, 0x40000000, 0x33, 1));
        *f.system = *f.selector->PickBest(&scratch);
        f.selector->Reset();
        if (IsSystemKey(*f.system)) {
            result = 1;
            AiSystemEval* eval = EvalOf(f, *f.system);
            if (eval) {
                ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(*f.system);
                if (node) {
                    f.targets->Add(node);
                    eval->m_flags28.m_value |= p.claimBit;
                    eval->MarkChanged();
                }
            }
        }
    }
    return result;
}

void AddTargetSystem(const FamilyFields& f, const FamilyParams& p) {
    AiRankedSelector sectorSelector;
    ObjectKey sector;
    ObjectKey system;
    ObjectKey scratch;
    bool found = false;
    AiSideController* owner = f.owner;
    f.system->Reset();
    if (p.targetCountGuard && !(f.targets->Count() < 2)) return;
    const int need = NeedForTargets(f, p);
    if (0 < need) {
        sectorSelector.Add(owner->RankSectors(p.sectorRequire, 0, 0, 0, 0, 0, p.sectorStat, 1));
        sector = *sectorSelector.PickBest(&scratch);
        for (;;) {
            if (!IsSectorKey(sector) || found) {
                sectorSelector.Reset();
                break;
            }
            AiSectorEval* sectorEval = owner->m_sectors.FindEval(sector);
            if (!sectorEval || sectorEval->Stat(p.sectorShortStat) <= 0) {
                sector.Reset();
                continue;
            }
            f.selector->Add(owner->RankSystemsOfSector(&sector, 0, 0, 1, p.systemForbid, 0, 0x40000000, p.systemStat, 1));
            system = *f.selector->PickBest(&scratch);
            while (IsSystemKey(system) && !found) {
                AiSystemEval* eval = EvalOf(f, system);
                if (eval && 0 < eval->Stat(p.shortStat) && eval->Stat(p.fitStat) <= need) {
                    *f.system = system;
                    found = true;
                }
                system = *f.selector->PickBest(&scratch);
            }
            f.selector->Reset();
            sector = *sectorSelector.PickBest(&scratch);
        }
    }
    if (IsSystemKey(*f.system)) {
        AiSystemEval* eval = EvalOf(f, *f.system);
        if (eval) {
            ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(*f.system);
            if (node) {
                f.targets->Add(node);
                eval->m_flags28.m_value |= p.claimBit;
                eval->MarkChanged();
            }
        }
    } else {
        bool anyTargetNeedsFacilities = false;
        if (f.targets->Count() < 2) {
            ObjectKeyNode* first = f.targets->First();
            if (first) {
                AiSystemEval* eval = EvalOf(f, KeyOfNode(first));
                if (eval && 0 < eval->Stat(p.shortStat)) anyTargetNeedsFacilities = true;
            }
            if (!anyTargetNeedsFacilities) {
                const int classes = owner->ChooseClass(f.classKey, 0x2a, p.classFlags, 0x4000, 2);
                if (classes <= need) owner->m_bitsA |= p.ownerBit;
            }
        }
    }
}

void RefreshTargets(const FamilyFields& f, const FamilyParams& p) {
    ObjectKeySet pending;
    ObjectKey best;
    int bestStat = 0;
    int budget = 1;
    for (;;) {
        best.Reset();
        for (ObjectKeyNode* node = f.targets->First(); node; node = NextNode(node)) {
            AiSystemEval* eval = EvalOf(f, KeyOfNode(node));
            if (eval && bestStat < eval->Stat(p.shortStat)) {
                ObjectKey key = KeyOfNode(node);
                if ((eval->m_flags30.m_value & kSystemDropped) != 0 && f.dropped->Find(&key) == nullptr) break;
                key = KeyOfNode(node);
                if (pending.Find(&key) == nullptr && 0 < eval->Stat(0x90)) {
                    bestStat = eval->Stat(p.shortStat);
                    best = KeyOfNode(eval);
                }
            }
        }
        if (IsSystemKey(best)) {
            ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(best);
            if (node) {
                pending.Add(node);
                --budget;
            }
        } else {
            budget = 0;
        }
        if (budget < 1) {
            for (ObjectKeyNode* node = f.dropped->First(); node; node = NextNode(node)) {
                AiSystemEval* eval = EvalOf(f, KeyOfNode(node));
                if (eval) eval->m_flags30.m_value &= ~kSystemDropped;
            }
            f.dropped->Clear();
            ObjectKeyNode* node = pending.First();
            while (node) {
                AiSystemEval* eval = EvalOf(f, KeyOfNode(node));
                if (eval) eval->m_flags30.m_value |= kSystemDropped;
                ObjectKeyNode* next = NextNode(node);
                ObjectKey key = KeyOfNode(node);
                pending.Detach(&key);
                f.dropped->Add(node);
                node = next;
            }
            return;
        }
    }
}

OrderBase* BuildManufactureOrder(const FamilyFields& f, int kind, uint32_t familyLow) {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    if (ai::KeyInFamilies(*f.manager, familyLow, familyLow + 2)) {
        order = static_cast<OrderBase*>(CreateMessageByKind(kind));
        if (order) {
            ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(*f.manager, 0);
            if (node) {
                team.Add(node);
                order->m_side = f.side;
                order->SetTeam(&team);
                ManufactureOrderBase* manufacture = static_cast<ManufactureOrderBase*>(order);
                manufacture->m_item.m_value = f.classKey->m_value;
                manufacture->m_count = *f.count;
            }
        }
    }
    return order;
}

OrderBase* BuildDestinationOrder(const FamilyFields& f, ObjectKey* target, uint32_t familyLow, uint32_t targetLow,
                                 uint32_t targetHigh) {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    if (ai::KeyInFamilies(*target, targetLow, targetHigh) && ai::KeyInFamilies(*f.manager, familyLow, familyLow + 2)) {
        order = static_cast<OrderBase*>(CreateMessageByKind(kDestinationOrder));
        if (order) {
            ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(*f.manager, 0);
            if (node) {
                team.Add(node);
                order->m_side = f.side;
                order->SetTeam(&team);
                order->SetTarget(target);
            }
        }
    }
    return order;
}

int PickSupplySystemNear(const FamilyFields& f, const FamilyParams& p) {
    int result = 0;
    ObjectKey scratch;
    AiSideController* owner = f.owner;
    if (p.supplyResetsCap) *f.cap = 0;
    f.selector->Add(owner->RankSectorsByDistance(f.system, 0x1000, 0, 0, 0, 0, 0, 2));
    *f.manager = *f.selector->PickBest(&scratch);
    f.selector->Reset();
    if (p.supplyByDistance)
        f.selector->Add(owner->RankSystemsOfSectorByDistance(f.manager, f.system, 0x800800, 0, 1, 3, 0, 0, 2));
    else
        f.selector->Add(owner->RankSystemsOfSector(f.manager, 0x800800, 0, 1, 3, 0, 0, 6, 1));
    *f.manager = *f.selector->PickBest(&scratch);
    f.selector->Reset();
    if (IsSystemKey(*f.manager)) {
        AiSystemEval* eval = EvalOf(f, *f.manager);
        System* system = static_cast<System*>(GetObjectInFamilies_90_98(f.side, f.manager));
        if (system && eval) {
            *f.cap = eval->Stat(0x60);
            if (1 < *f.cap) --*f.cap;
            GameObjNotifier* manager = system->FindManufacturingManagerBySlot123(1);
            if (manager) {
                ObjectKey key;
                GetObjectKey(manager, &key);
                *f.manager = key;
                result = 1;
            }
        }
    }
    if (p.supplyClaims && result != 0 && f.targets->Find(f.system) == nullptr) {
        AiSystemEval* eval = EvalOf(f, *f.system);
        if (eval) {
            ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(*f.system);
            if (node) {
                f.targets->Add(node);
                eval->m_flags28.m_value |= p.claimBit;
                eval->MarkChanged();
            }
        }
    }
    return result;
}

}  // namespace aitask
