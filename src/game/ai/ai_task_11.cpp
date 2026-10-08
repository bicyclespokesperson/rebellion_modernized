#include "ai/ai_class_records.h"
#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"
#include "ai/ai_task_family.h"
#include "ai/ai_task_types.h"
#include "ai/ai_task_util.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "gameobj/object_lookup.h"

using namespace aitask;

namespace {

constexpr FamilyParams kParams = {
    0x5a, 0x24c, 10, 0xd8, 0xd0, 0x58, 0x20, 0x11, 0x200003, 4, 0x200000, 0xb4, 0x4000, 2, 0xe, 0xc,
    FacilityKind::kManufacturing29, 0x400000, FacilityKind::kManufacturing2A, 0x3800000, false, false};

constexpr int kScrapOrder = 0x200;
constexpr int kMoveOrder = 0x201;
constexpr int kAssignOrder = 0x270;
constexpr int kStageTargets = 0x3ec;
constexpr int kStageFleets = 0x3f8;
constexpr int kStageClasses = 0x3f9;
constexpr uint32_t kSystemClaimed = 0x200000;
constexpr uint32_t kDefaultSystemKey = 0x90000109;

bool IsFleetKey(const ObjectKey& key) { return ai::KeyInFamilies(key, 8, 0x10) != 0; }
bool IsCapitalShipKey(const ObjectKey& key) { return ai::KeyInFamilies(key, 0x14, 0x1c) != 0; }

}  // namespace

AiTask11::AiTask11(int view)
    : AiStagedTask(11, view), m_selector(), m_manager(), m_unit(), m_system(), m_classKey(), m_targets(), m_dropped() {
    m_classKey.m_value = 0;
    m_stage = 0;
    m_step = 0;
    m_count = 0;
    m_cap = 0;
    m_limit = 0;
    m_classFlags = 0;
    m_classMask = 0;
    m_claimBit = 0;
    m_targets.Clear();
    m_dropped.Clear();
    m_x24 = 0x1e;
}

AiTask11::~AiTask11() { REB_KEEP_VPTR_STORE(); }

FamilyFields AiTask11::Fields() {
    return {m_owner, m_view, m_x24, &m_selector, &m_targets, &m_dropped, &m_system, &m_manager, &m_classKey, &m_count, &m_cap, &m_limit};
}

void AiTask11::Read(istream* in) {
    AiTaskBase::Read(in);
    ReadValue32(in, &m_stage);
    ReadValue32(in, &m_step);
    ReadValue32(in, &m_classFlags);
    ReadValue32(in, &m_classMask);
    ReadValue32(in, &m_claimBit);
    ReadValue32(in, &m_count);
    ReadValue32(in, &m_cap);
    ReadValue32(in, &m_limit);
    m_selector.Read(in);
    m_manager.Read(in);
    m_unit.Read(in);
    m_system.Read(in);
    m_targets.Load(in);
    m_dropped.Load(in);
}

void AiTask11::Write(ostream* out) {
    AiTaskBase::Write(out);
    WriteValue32(out, &m_stage);
    WriteValue32(out, &m_step);
    WriteValue32(out, &m_classFlags);
    WriteValue32(out, &m_classMask);
    WriteValue32(out, &m_claimBit);
    WriteValue32(out, &m_count);
    WriteValue32(out, &m_cap);
    WriteValue32(out, &m_limit);
    m_selector.Write(out);
    m_manager.Write(out);
    m_unit.Write(out);
    m_system.Write(out);
    m_targets.Save(out);
    m_dropped.Save(out);
}

OrderBase* AiTask11::Step() {
    if (m_flags.Get() != 1) {
        m_stage = 0;
        m_step = 0;
        m_finished = 1;
        return nullptr;
    }
    if (m_stage == kStageTargets) return StepTargets();
    if (m_stage == kStageFleets) return StepFleets();
    if (m_stage != kStageClasses) {
        m_stage = kStageFleets;
        m_step = 0;
        return nullptr;
    }
    return StepClasses();
}

OrderBase* AiTask11::StepFleets() {
    switch (m_step) {
    case 2: {
        OrderBase* order = BuildFleetMoveOrder();
        m_step = 3;
        if (order) {
            m_finished = 1;
            ++m_priority;
        }
        return order;
    }
    case 3: {
        OrderBase* order = BuildFleetOrder();
        m_step = 4;
        if (order) {
            m_finished = 1;
            ++m_priority;
            return order;
        }
        return nullptr;
    }
    case 4: {
        OrderBase* order = BuildTroopOrder();
        m_step = 0;
        m_stage = kStageClasses;
        m_finished = 1;
        return order;
    }
    default:
        m_step = 2;
        return nullptr;
    }
}

OrderBase* AiTask11::StepClasses() {
    OrderBase* order = nullptr;
    switch (m_step) {
    case 5:
        ChooseClassMask();
        m_step = 9;
        return nullptr;
    default:
        m_step = 8;
        return nullptr;
    case 8:
        m_step = ChooseManager() != 0 ? 5 : 0xd;
        return nullptr;
    case 9:
        m_step = ChooseClass();
        if (m_step == 0) {
            m_stage = kStageTargets;
            m_finished = 1;
        }
        return nullptr;
    case 10:
        order = BuildShipScrapOrder();
        break;
    case 0xd:
        if (IsOverQuota() != 0) {
            m_step = 10;
            return nullptr;
        }
        m_step = 0;
        m_stage = kStageTargets;
        return nullptr;
    case 0xf:
        order = BuildManufactureOrder211();
        break;
    }
    m_step = 0;
    m_stage = kStageTargets;
    m_finished = 1;
    return order;
}

OrderBase* AiTask11::StepTargets() {
    OrderBase* order = nullptr;
    switch (m_step) {
    case 7:
        if (SelectTarget() != 0) {
            m_step = 8;
            return nullptr;
        }
        break;
    case 8:
        m_owner->m_bitsA &= ~2u;
        if (m_owner->m_bitB == 2 && PickSupplySystem() != 0) {
            m_owner->AdvanceBitCursor();
            m_step = 0xb;
            return nullptr;
        }
        m_step = 0x10;
        return nullptr;
    case 9:
        m_step = ComputeFacilityCount();
        if (m_step != 0) return nullptr;
        m_stage = kStageFleets;
        m_finished = 1;
        return nullptr;
    default:
        m_step = 7;
        return nullptr;
    case 0xb:
        order = BuildScrapOrder();
        if (order) {
            m_finished = 1;
            ++m_priority;
        }
        m_step = 9;
        return order;
    case 0xc:
        order = BuildCapitalOrder();
        break;
    case 0xe: {
        OrderBase* destination = BuildDestinationOrder(&m_system);
        if (destination) {
            m_step = 0xf;
            m_finished = 1;
            ++m_priority;
            return destination;
        }
        break;
    }
    case 0xf:
        order = BuildManufactureOrder210();
        break;
    case 0x10:
        AddTargetSystem();
        RefreshTargets();
        break;
    }
    m_step = 0;
    m_stage = kStageFleets;
    m_finished = 1;
    return order;
}

OrderBase* AiTask11::BuildFleetMoveOrder() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    ObjectKey scratch;
    ObjectKey target;
    bool found = false;
    AiSideController* owner = m_owner;
    m_selector.Add(owner->RankFleets(0x1005, 0x802, 0, 1));
    m_unit = *m_selector.PickBest(&scratch);
    while (IsFleetKey(m_unit) && !found) {
        AiFleetEval* fleet = owner->m_fleets.FindEval(m_unit);
        if (fleet) {
            AiSystemEval* system = owner->m_systems.FindEval(fleet->m_systemKey);
            if (system && (system->m_flags30.m_value & 4) != 0) found = true;
        }
        if (found) break;
        m_unit = *m_selector.PickBest(&scratch);
    }
    m_selector.Reset();
    if (!IsFleetKey(m_unit)) {
        m_selector.Add(owner->RankFleets(0x1005, 0x802, 0, 1));
        m_unit = *m_selector.PickBest(&scratch);
        found = false;
        while (IsFleetKey(m_unit) && !found) {
            AiFleetEval* fleet = owner->m_fleets.FindEval(m_unit);
            if (fleet) {
                AiSystemEval* system = owner->m_systems.FindEval(fleet->m_systemKey);
                if (system && (system->m_flags28.m_value & kSystemClaimed) == 0) found = true;
            }
            if (found) break;
            m_unit = *m_selector.PickBest(&scratch);
        }
        m_selector.Reset();
    }
    if (!IsFleetKey(m_unit)) return order;

    AiFleetEval* fleet = owner->m_fleets.FindEval(m_unit);
    if (fleet) {
        for (ObjectKeyNode* ship = fleet->m_shipKeys.First(); ship; ship = NextNode(ship)) {
            ObjectKey key = KeyOfNode(ship);
            AiShipRecord* record = static_cast<AiShipRecord*>(owner->m_ships.Find(&key));
            if (record && (record->m_flags.m_value & 0x1000) != 0 && (record->m_flags.m_value & 0x800800) == 0) {
                ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(KeyOfNode(ship), 0);
                if (node) team.Add(node);
            }
        }
    }
    if (team.First()) {
        m_selector.Add(owner->RankSectorsByDistance(&m_unit, 8, 0, 0x20000, 0, 0, 0, 2));
        target = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        m_selector.Add(owner->RankSystemsOfSectorByDistance(&target, &m_unit, 0x200000, 0, 0, 2, 0, 0, 2));
        target = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        if (!IsSystemKey(target)) {
            m_selector.Add(owner->RankSectorsByDistance(&m_unit, 8, 0, 4, 0, 0, 0, 2));
            target = *m_selector.PickBest(&scratch);
            m_selector.Reset();
            m_selector.Add(owner->RankSystemsOfSectorByDistance(&target, &m_unit, 0, 0, 1, 2, 0, 0, 2));
            target = *m_selector.PickBest(&scratch);
            m_selector.Reset();
        }
        if (IsSystemKey(target)) {
            order = static_cast<OrderBase*>(CreateMessageByKind(kMoveOrder));
            if (order) {
                order->m_side = m_view;
                order->SetTeam(&team);
                order->SetTarget(&target);
            }
        }
    }
    return order;
}

OrderBase* AiTask11::BuildFleetOrder() {
    AiRankedSelector shipSelector;
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    ObjectKey scratch;
    ObjectKey ship;
    ObjectKey target;
    bool found = false;
    AiSideController* owner = m_owner;
    m_selector.Add(owner->RankFleets(5, 0x3802, 0, 2));
    m_unit = *m_selector.PickBest(&scratch);
    while (IsFleetKey(m_unit) && !found) {
        AiFleetEval* fleet = owner->m_fleets.FindEval(m_unit);
        if (fleet) {
            if ((fleet->m_flags.m_value & 0x400) == 0) {
                AiSystemEval* system = owner->m_systems.FindEval(fleet->m_systemKey);
                if (system) {
                    if ((fleet->m_flags.m_value & 0x10) != 0) {
                        target = KeyOfValue(kDefaultSystemKey);
                        AiSystemEval* home = owner->m_systems.FindEval(target);
                        if (home && (home->m_flags30.m_value & 1) != 0) found = KeyOfNode(system).m_value != target.m_value;
                    }
                    if (!found && ((system->m_flags30.m_value & 1) == 0 || (system->m_flags30.m_value & 0x40000) == 0)) {
                        ObjectKey sectorKey = KeyOfNode(system->m_sector);
                        AiSectorEval* sector = static_cast<AiSectorEval*>(owner->m_sectors.Find(&sectorKey));
                        if (sector) {
                            ObjectKey sectorOf = KeyOfNode(sector);
                            m_selector.Add(owner->RankSystemsOfSectorByDistance(&sectorOf, &m_unit, 0, 0, 0x40001, 2, 0, 0, 2));
                            target = *m_selector.PickBest(&scratch);
                            m_selector.Reset();
                            if (IsSystemKey(target)) found = true;
                            if (!found) {
                                if ((sector->m_flags24.m_value & 0x40000000) != 0) {
                                    sectorOf = KeyOfNode(sector);
                                    m_selector.Add(owner->RankSystemsOfSectorByDistance(&sectorOf, &m_unit, 0x80000000, 0, 1, 2, 0, 0, 2));
                                    target = *m_selector.PickBest(&scratch);
                                    m_selector.Reset();
                                    if (IsSystemKey(target)) found = true;
                                }
                                if (!found) {
                                    sectorOf = KeyOfNode(sector);
                                    target = *owner->FindNearestSystemWithFleets(&scratch, &sectorOf, 0, 0, 0x40001, 2, 0, 0);
                                    if (IsSystemKey(target)) found = true;
                                    if (!found) {
                                        sectorOf = KeyOfNode(sector);
                                        target = *owner->FindNearestSystemWithFleets(&scratch, &sectorOf, 0, 0, 1, 2, 0, 0);
                                        if (IsSystemKey(target)) found = true;
                                    }
                                }
                            }
                        }
                    }
                    if (found && IsSystemKey(target)) {
                        order = static_cast<OrderBase*>(CreateMessageByKind(kMoveOrder));
                        if (order) {
                            ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(m_unit, 0);
                            if (node) {
                                team.Add(node);
                                order->m_side = m_view;
                                order->SetTeam(&team);
                                order->SetTarget(&target);
                            }
                        }
                    }
                }
            } else if ((fleet->m_flags.m_value & 0x4000) != 0) {
                shipSelector.Add(owner->RankShipsOfFleet(&m_unit, 0x4000, 0x803800, 0, 1));
                ship = *shipSelector.PickBest(&scratch);
                while (IsCapitalShipKey(ship)) {
                    ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(ship, 0);
                    if (node) team.Add(node);
                    ship = *shipSelector.PickBest(&scratch);
                }
                shipSelector.Reset();
                order = static_cast<OrderBase*>(CreateMessageByKind(kAssignOrder));
                if (order) {
                    order->m_side = m_view;
                    order->SetTeam(&team);
                    order->SetTarget(&fleet->m_systemKey);
                }
                found = true;
            }
        }
        m_unit = *m_selector.PickBest(&scratch);
    }
    m_selector.Reset();
    return order;
}

OrderBase* AiTask11::BuildTroopOrder() {
    AiRankedSelector shipSelector;
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    ObjectKey scratch;
    ObjectKey sector;
    ObjectKey system;
    ObjectKey ship;
    AiSideController* owner = m_owner;
    if ((owner->m_flags.Get() & 0x30) == 0) return order;

    bool found = false;
    m_selector.Add(owner->RankSectorsByX3c(0, 0, 0x8000000, 0, 0, 0, 2));
    sector = *m_selector.PickBest(&scratch);
    const ObjectKey firstSector = sector;
    while (IsSectorKey(sector) && !found) {
        AiSectorEval* eval = static_cast<AiSectorEval*>(owner->m_sectors.Find(&sector));
        if (eval) {
            AiSystemRecord* record = static_cast<AiSystemRecord*>(owner->m_systemRecords.FindById(eval->m_x38));
            if (record && 0 < record->ForwardToPosture()) {
                system = record->m_systemKey;
                if (IsSystemKey(system)) {
                    AiSystemEval* systemEval = owner->m_systems.FindEval(system);
                    if (systemEval && (systemEval->m_flags28.m_value & 2) == 0) found = true;
                }
            }
        }
        sector = *m_selector.PickBest(&scratch);
    }
    m_selector.Reset();
    if (!found) {
        system.Reset();
        AiSectorEval* eval = static_cast<AiSectorEval*>(owner->m_sectors.Find(&firstSector));
        if (eval) {
            AiSystemRecord* record = static_cast<AiSystemRecord*>(owner->m_systemRecords.FindById(eval->m_x38));
            if (record && 0 < record->ForwardToPosture()) system = record->m_systemKey;
        }
    }
    if (IsSystemKey(system)) {
        m_selector.Add(owner->RankFleets(5, 2, 0xc, 1));
        m_unit = *m_selector.PickBest(&scratch);
        bool troopsFound = false;
        while (IsFleetKey(m_unit) && !troopsFound) {
            AiFleetEval* fleet = owner->m_fleets.FindEval(m_unit);
            if (fleet) {
                shipSelector.Add(owner->RankShipsOfFleet(&m_unit, 0x24000, 0x800, 9, 1));
                ship = *shipSelector.PickBest(&scratch);
                while (IsCapitalShipKey(ship)) {
                    GameObjNotifier* object = GetObjectInFamilies_14_1c(m_view, &ship);
                    if (object) {
                        TroopRegIterator troops(object, 1);
                        troops.Begin();
                        if (troops.Current()) {
                            troopsFound = true;
                            do {
                                ObjectKey key;
                                GetObjectKey(troops.Current(), &key);
                                ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(key, 0);
                                if (node) team.Add(node);
                                troops.Advance();
                            } while (troops.Current());
                        }
                    }
                    ship = *shipSelector.PickBest(&scratch);
                }
                shipSelector.Reset();
            }
            m_unit = *m_selector.PickBest(&scratch);
        }
        m_selector.Reset();
        if (team.First()) {
            order = static_cast<OrderBase*>(CreateMessageByKind(kMoveOrder));
            if (order) {
                order->m_side = m_view;
                order->SetTeam(&team);
                order->SetTarget(&system);
            }
        }
    }
    return order;
}

int AiTask11::ChooseManager() {
    int result = 0;
    bool found = false;
    m_owner->m_key24.Reset();
    ObjectKeyNode* node = m_targets.First();
    while (node && !found) {
        m_manager = KeyOfNode(node);
        if (!IsSystemKey(m_manager)) {
            ObjectKeyNode* next = NextNode(node);
            ObjectKey key = KeyOfNode(node);
            m_targets.RemoveKey(&key);
            node = next;
            continue;
        }
        AiSystemEval* eval = m_owner->m_systems.FindEval(m_manager);
        const bool stale = !eval || (eval->m_flags30.m_value & 1) == 0 || (eval->m_flags28.m_value & kSystemClaimed) == 0;
        if (stale) {
            if (eval) {
                eval->m_flags28.m_value &= ~kSystemClaimed;
                eval->MarkChanged();
            }
            ObjectKeyNode* next = NextNode(node);
            ObjectKey key = KeyOfNode(node);
            m_targets.RemoveKey(&key);
            node = next;
            continue;
        }
        if (0 < eval->Stat(0x58) && (eval->m_flags28.m_value & 0x200) != 0 && (eval->m_flags28.m_value & 3) == 0) {
            m_owner->m_key24 = m_manager;
            found = true;
        }
        node = NextNode(node);
    }
    if (!found) {
        m_manager = m_owner->m_key1c;
        if (IsSystemKey(m_manager)) {
            AiSystemEval* eval = m_owner->m_systems.FindEval(m_manager);
            if (eval && (eval->m_flags30.m_value & 1) != 0 && (eval->m_flags28.m_value & kSystemClaimed) != 0 &&
                0 < eval->Stat(0x58) && (eval->m_flags28.m_value & 0x200) != 0 && (eval->m_flags28.m_value & 3) == 0)
                found = true;
        }
        if (!found) {
            m_manager.Reset();
            return 0;
        }
    }
    System* system = static_cast<System*>(GetObjectInFamilies_90_98(m_view, &m_manager));
    if (system) {
        GameObjNotifier* manager = system->FindManufacturingManagerBySlot123(0);
        if (manager) {
            ObjectKey key;
            GetObjectKey(manager, &key);
            m_manager = key;
            result = 1;
        }
    }
    return result;
}

void AiTask11::ChooseClassMask() {
    int counts[6] = {};
    enum { kFlag1, kFlag80, kFlag4, kFlag8, kFlag10, kFlag800 };
    m_classFlags = 0;
    m_classMask = 0;
    for (AvlNode* node = m_owner->m_systemRecords.First(); node; node = node->m_successor) {
        const uint32_t flags = static_cast<AiSystemRecord*>(node)->m_flagsB.m_value;
        const uint32_t taken = m_flags28.m_value;
        if ((flags & 1) != 0 && (taken & 0x1000000) == 0) ++counts[kFlag1];
        if ((flags & 0x80) != 0 && (taken & 0x2000000) == 0) ++counts[kFlag80];
        if ((flags & 4) != 0 && (taken & 0x4000000) == 0) ++counts[kFlag4];
        if ((flags & 8) != 0 && (taken & 0x8000000) == 0) ++counts[kFlag8];
        if ((flags & 0x10) != 0 && (taken & 0x10000000) == 0) ++counts[kFlag10];
        if ((flags & 0x800) != 0 && (taken & 0x40000000) == 0 && (m_owner->m_flags.Get() & 0x1000000) != 0) ++counts[kFlag800];
    }
    const int a = counts[kFlag1], b = counts[kFlag80], c = counts[kFlag4], d = counts[kFlag8], e = counts[kFlag10], g = counts[kFlag800];
    auto choose = [this](int flags, int mask, int bit) {
        m_classFlags = flags;
        m_classMask = mask;
        m_claimBit = bit;
    };
    if (c < 1) {
        if (a < 1 || a < b || a < c || a < e || a < d || a < g) {
            if (b < 1 || b < c || b < e || b < d || b < g) {
                if (e < 1 || e < d || e < g) {
                    if (d < 1 || d < g) {
                        if (0 < g) choose(0x100400, 0x10000, 0x40000000);
                    } else {
                        choose(0x100102, 0x80, 0x8000000);
                    }
                } else {
                    choose(0x100800, 0x800, 0x10000000);
                }
            } else {
                choose(0x100002, 0x400, 0x2000000);
            }
        } else {
            choose(0x100001, 0x80000, 0x1000000);
        }
    } else {
        choose(0x100004, 0x100, 0x4000000);
    }
    if (m_classFlags != 0) return;
    if ((m_flags28.m_value & 0x4000000) == 0 && m_owner->Stat(0x260) < m_owner->Stat(0x20c)) choose(0x100004, 0x100, 0x4000000);
    if (m_classFlags != 0) return;
    if (m_view == 2 && 1 < m_targets.Count() && (m_owner->m_flags.Get() & 4) == 0 && (m_flags28.m_value & 0x20000000) == 0)
        choose(0x80, 0x200, 0x20000000);
    if (m_classFlags != 0) return;
    if ((m_flags28.m_value & 0x1000000) == 0 && m_owner->Stat(0x268) < 1) choose(0x100001, 0x80000, 0x1000000);
    if (m_classFlags != 0) return;
    if ((m_flags28.m_value & 0x2000000) == 0 && m_owner->Stat(0x26c) < 1) choose(0x100002, 0x400, 0x2000000);
    if (m_classFlags == 0 && (m_flags28.m_value & 0x8000000) == 0 && m_owner->Stat(0x27c) <= m_owner->m_classes.m_x34)
        choose(0x100102, 0x80, 0x8000000);
}

int AiTask11::ChooseClass() {
    int result = 0;
    ObjectKey picked;
    ObjectKey scratch;
    m_flags28.m_value &= 0x80ffffff;
    bool done = false;
    m_count = 0;
    const int need = Need();
    if (need < 1) {
        if (need < 0) result = 10;
        return result;
    }
    m_selector.Add(m_owner->RankClasses(0x28, m_classFlags, m_classMask, 1));
    picked = *m_selector.PickBest(&scratch);
    m_classKey = picked;
    while ((m_classKey.m_value & 0xff000000) != 0 && !done) {
        AiClassRecordBase* record = m_owner->m_classes.Find(&m_classKey);
        if (record && record->m_valueB <= need) {
            m_count = 1;
            done = true;
        }
        if (!done) {
            picked = *m_selector.PickBest(&scratch);
            m_classKey = picked;
        }
    }
    m_selector.Reset();
    if (m_count < 1) {
        m_flags28.m_value |= m_claimBit;
    } else {
        m_owner->m_key24.Reset();
        result = 0xf;
    }
    return result;
}

int AiTask11::IsOverQuota() { return (((m_x24 * m_owner->Stat(0x184)) / 100) * 0x5a) / 100 - m_owner->Stat(0x24c) < 0; }

OrderBase* AiTask11::BuildShipScrapOrder() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    ObjectKey scratch;
    m_selector.Add(m_owner->RankFleets(5, 0x402, 0xe, 2));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankShipsOfFleet(&m_unit, 0x24000, 0x2800, 0xb, 2));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (IsCapitalShipKey(m_unit)) {
        ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(m_unit, 0);
        if (node) team.Add(node);
    }
    if (team.First()) order = NewTeamOrder(kScrapOrder, m_view, &team);
    return order;
}

int AiTask11::SelectTarget() { return aitask::SelectTarget(Fields(), kParams); }

int AiTask11::PickSupplySystem() { return aitask::PickSupplySystemNear(Fields(), kParams); }

OrderBase* AiTask11::BuildScrapOrder() { return aitask::BuildScrapOrder(Fields(), kParams); }

int AiTask11::ComputeFacilityCount() { return aitask::ComputeCount(Fields(), kParams); }

OrderBase* AiTask11::BuildCapitalOrder() {
    return aitask::BuildCapitalOrder(Fields(), kParams, FacilityKind::kManufacturing28);
}

OrderBase* AiTask11::BuildManufactureOrder211() { return aitask::BuildManufactureOrder(Fields(), 0x211, 0xa2); }

OrderBase* AiTask11::BuildDestinationOrder(ObjectKey* target) { return aitask::BuildDestinationOrder(Fields(), target, 0xa0); }

OrderBase* AiTask11::BuildManufactureOrder210() { return aitask::BuildManufactureOrder(Fields(), 0x210, 0xa0); }

void AiTask11::AddTargetSystem() { aitask::AddTargetSystem(Fields(), kParams); }

int AiTask11::Need() { return aitask::Need(Fields(), kParams); }

int AiTask11::NeedForTargets() { return aitask::NeedForTargets(Fields(), kParams); }

void AiTask11::RefreshTargets() { aitask::RefreshTargets(Fields(), kParams); }
