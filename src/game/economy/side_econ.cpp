#include "core/checked_new.h"
#include "data/dat_record.h"
#include "economy/manu_mgr.h"
#include "economy/side.h"
#include "economy/side_decl.h"
#include "events/plain_events.h"
#include "galaxy/game_setup_seed.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"
#include "gameobj/object_type_db.h"

extern int g_setupComplete;  // 0x6b90e0

namespace {

constexpr uint32_t kControlSideMask = 0xc0;
constexpr uint32_t kUsableFlag = 0x1;
constexpr uint32_t kCompletedFlag = 0x4;
constexpr uint32_t kExistingFlag = 0x40;
constexpr uint32_t kProcFacilExcludedFlag = 0x1;
constexpr uint32_t kProcFacilWaitingFlag = 0x2;
constexpr uint32_t kProcFacilReleasableBit = 2;
constexpr int kFacilityManuSlot = 129;
constexpr int kStatusCategorySide = 0xf3;
constexpr int kStatusNotEnoughMaintenance = 1;

bool sameControlSide(const GameObjRoot* a, const GameObjRoot* b) {
    return ((a->m_flags ^ b->m_flags) & kControlSideMask) == 0;
}

ProductionFacility* asFacility(GameObjNotifier* object) { return static_cast<ProductionFacility*>(static_cast<GameObj*>(object)); }

GameObjNotifier* nextInRange(GameObjNotifier* object, const FamilyRange& range) {
    return static_cast<GameObj*>(object)->FindNextSibling(&range, 1);
}

bool isReleasable(const ProductionFacility* facility) { return ((facility->m_procFlags >> kProcFacilReleasableBit) & 1) != 0; }

void addPair(IntPair* sum, const IntPair& other) {
    sum->m_first += other.m_first;
    sum->m_second += other.m_second;
}

void subtractPair(IntPair* from, const IntPair& other) {
    from->m_first -= other.m_first;
    from->m_second -= other.m_second;
}

}  // namespace

// ---- facility helpers ---------------------------------------------------------------------------------------

int __stdcall MarkFacilityWaiting(ProductionFacility* facility, NotifyContext* ctx) {
    int result = 1;
    if ((facility->m_procFlags & kProcFacilWaitingFlag) == 0) result = facility->SetPointPresent(1, ctx);
    return result;
}

int Side::FindFacilityManuMgr(GameObj* facility, ManuMgr** out) {
    *out = nullptr;
    GameObjNotifier* root = GetGameRootMember_bc();
    bool valid = root != nullptr;
    if (valid) {
        const uint32_t facilityFamily = facility->GetFamily();
        valid = facilityFamily >= 0x28 && facilityFamily < 0x2c;
    }
    int result = valid ? 1 : 0;
    if (valid) {
        GameObjNotifier* system = nullptr;
        result = FUN_00555540(root, facility, &system);
        if (result != 0 && system != nullptr) {
            const uint32_t systemFamily = system->GetFamily();
            if (systemFamily >= 0x90 && systemFamily < 0x98) {
                using SlotFn = int(__thiscall*)(GameObj*);
                const int slot = (*reinterpret_cast<SlotFn**>(facility))[kFacilityManuSlot](facility);
                ManuMgr* manager = static_cast<ManuMgr*>(static_cast<System*>(system)->System::FindManufacturingManagerBySlot123(slot));
                *out = manager;
                result = manager != nullptr;
            }
        }
    }
    return result;
}

void Side::ReleaseFacilityToManager(ProductionFacility* facility, NotifyContext* ctx) {
    int result = 0;
    if (isReleasable(facility)) result = facility->SetPointPresent(0, ctx);
    if (result) {
        ManuMgr* manager = nullptr;
        if (FindFacilityManuMgr(facility, &manager) && manager != nullptr) reinterpret_cast<ManuMgr*>(manager)->ManuMgr::AdvanceProgress(ctx);
    }
}

// ---- queries ------------------------------------------------------------------------------------------------

int Side::IsScrapCandidate(GameObj* unit, int* out) {
    if ((unit->m_stateFlags & kExistingFlag) && sameControlSide(unit, this) && (unit->m_stateFlags & kCompletedFlag) &&
        static_cast<UnitObjBase*>(unit)->GetRecordField4c() != 0) {
        *out = 1;
        return 1;
    }
    *out = 0;
    return 1;
}

int Side::CollectRdRecords(int kind, IdAvlTree* out) {
    int level = 0;
    bool valid = true;
    if (kind == 0x28) {
        level = m_shipyardRdOrder;
    } else if (kind == 0x29) {
        level = m_trainingFacilRdOrder;
    } else if (kind == 0x2a) {
        level = m_constructionYardRdOrder;
    } else {
        valid = false;
    }
    int result = valid ? 1 : 0;
    if (valid) {
        int maxLevel = 0;
        ObjectKey key;
        GetKey(&key);
        result = CollectDataRecords(SideNumberFromKey(&key), kind, level, 1, 1, 0, out, &maxLevel);
    }
    return result;
}

int Side::CheckMaintCapacity(const ObjectKey* key, int count, StatusCode* status) {
    *status = StatusCode(1, 0xffffffff);
    bool result = true;
    const uint32_t keyFamily = key->m_value >> 24;
    if (keyFamily >= 0x10 && keyFamily < 0x40) {
        SdRecord* record = static_cast<SdRecord*>(FindDataRecord(AsLookupKey(key)));
        result = record != nullptr;
        if (result) {
            const int needed = static_cast<int>(record->GetField4c()) * count;
            const int available = static_cast<int>(m_maintState.m_first) - static_cast<int>(m_maintState.m_second);
            if (needed != 0 && needed - available != 0 && available <= needed) {
                *status = StatusCode(kStatusCategorySide, kStatusNotEnoughMaintenance);
            }
        }
    }
    return result;
}

// ---- maintenance --------------------------------------------------------------------------------------------

int Side::UpdateMaintDeficit(NotifyContext* ctx) {
    const int capacity = static_cast<int>(m_maintState.m_first);
    const bool fits = capacity >= m_maintRequired && capacity >= m_maintRequiredB;
    return SetMaintDeficit(fits ? 0 : 1, ctx);
}

namespace {

using FirstFacility = GameObjNotifier* (*)();

// Moves a mine/refinery group's allocation towards `required` by letting the facilities of `range` adjust
// themselves, pass after pass, until the amount is spent or no facility can take more; `working` follows along and
// is stored at the end.
int allocateFacilities(Side* side, const IntPair* state, int required, FirstFacility firstFacility,
                       const FamilyRange& range, int (Side::*setState)(const IntPair*, NotifyContext*),
                       NotifyContext* ctx) {
    int delta = required - static_cast<int>(state->m_second);
    bool again = true;
    int result = 1;
    const int room = static_cast<int>(state->m_first) - static_cast<int>(state->m_second);
    if (room < delta) delta = room;
    if (delta == 0) return result;

    const bool reducing = delta < 1;
    if (reducing) delta = -delta;
    IntPair working = *state;
    while (again) {
        again = false;
        GameObjNotifier* object = firstFacility();
        bool spent = false;
        while (result != 0 && object != nullptr) {
            if (delta == 0) {
                spent = true;
                break;
            }
            ProductionFacility* facility = asFacility(object);
            if ((facility->m_stateFlags & kUsableFlag) && !(facility->m_procFlags & kProcFacilExcludedFlag) &&
                sameControlSide(facility, side)) {
                int used = 0;
                IntPair pair;
                if (reducing) {
                    result = facility->RewindWork(delta, reinterpret_cast<const int*>(&working), &used, ctx);
                    if (result) {
                        delta -= used;
                        subtractPair(&working, IntPair(0, used));
                        facility->GetWork(&pair);
                        if (pair.m_second != 0) again = true;
                    }
                } else {
                    result = facility->AdvanceWork(delta, reinterpret_cast<const int*>(&working), &used, ctx);
                    if (result) {
                        delta -= used;
                        addPair(&working, IntPair(0, used));
                        facility->GetWork(&pair);
                        if (pair.m_first != pair.m_second) again = true;
                    }
                }
            }
            object = nextInRange(object, range);
        }
        if (spent || delta == 0) break;
    }
    (side->*setState)(&working, ctx);
    return result;
}

}  // namespace

int Side::AllocateMines(NotifyContext* ctx) {
    return allocateFacilities(this, &m_mineState, m_maintRequired, GetFirstMasterInFamilies_2c_2d,
                              FamilyRange(0x2c, 0x2d), &Side::SetMineState, ctx);
}

int Side::AllocateRefineries(NotifyContext* ctx) {
    return allocateFacilities(this, &m_refineryState, m_maintRequiredB, GetFirstMasterInFamilies_2d_2e,
                              FamilyRange(0x2d, 0x2e), &Side::SetRefineryState, ctx);
}

int Side::RecomputeMaintenance(NotifyContext* ctx) {
    int total = 0;
    IntPair mines;
    IntPair refineries;
    const FamilyRange units(0x10, 0x40);
    for (GameObjNotifier* object = GetFirstMasterInFamilies_10_40(); object; object = nextInRange(object, units)) {
        GameObj* unit = static_cast<GameObj*>(object);
        if (!(unit->m_stateFlags & kExistingFlag) || !sameControlSide(unit, this)) continue;
        total += static_cast<UnitObjBase*>(unit)->GetRecordField4c();
        const uint32_t unitFamily = unit->GetFamily();
        if (unitFamily < 0x2c || unitFamily > 0x2f) continue;
        ProductionFacility* facility = static_cast<ProductionFacility*>(unit);
        if (!(facility->m_stateFlags & kUsableFlag) || (facility->m_procFlags & kProcFacilExcludedFlag)) continue;
        IntPair pair;
        if (unitFamily == 0x2c) {
            facility->GetWork(&pair);
            addPair(&mines, pair);
        } else if (unitFamily == 0x2d) {
            facility->GetWork(&pair);
            addPair(&refineries, pair);
        }
    }
    int result = SetMineState(&mines, ctx);
    if (result) result = SetRefineryState(&refineries, ctx);
    if (result) result = SetMaintRequired(total, ctx);
    if (result) result = SetMaintRequiredB(total, ctx);
    if (result) result = AllocateMines(ctx);
    if (result) result = AllocateRefineries(ctx);
    return result;
}

int Side::RefundScrappedUnit(GameObj* unit, NotifyContext* ctx) {
    int result = 1;
    const uint32_t reason = unit->m_packed & 0xff;
    if (reason >= 0x14 && reason <= 0x16 && (unit->m_stateFlags & kCompletedFlag)) {
        const int refund = static_cast<int>(static_cast<UnitObjBase*>(unit)->GetRecordField48());
        result = SetRefinedMaterials(m_refinedMaterials + refund / 2, ctx);
    }
    return result;
}

int Side::ScrapRandomUnit(NotifyContext* ctx) {
    GameObj* chosen = nullptr;
    int candidates = 0;
    int ok = (static_cast<int>(m_maintState.m_first) < m_maintRequired ||
              static_cast<int>(m_maintState.m_first) < m_maintRequiredB)
                 ? 1
                 : 0;
    int isCandidate = 0;
    for (GameObjNotifier* object = GetFirstMasterInFamilies_10_40(); object; object = FindNextUnit(object)) {
        if (ok == 0) return 0;
        ok = IsScrapCandidate(static_cast<GameObj*>(object), &isCandidate);
        if (ok != 0 && isCandidate != 0) ++candidates;
    }
    if (ok != 0 && candidates != 0) {
        int index = 0;
        const int target = GameRandom(candidates - 1);
        for (GameObjNotifier* object = GetFirstMasterInFamilies_10_40(); object; object = FindNextUnit(object)) {
            if (ok == 0) return 0;
            if (chosen != nullptr) break;
            ok = IsScrapCandidate(static_cast<GameObj*>(object), &isCandidate);
            if (ok != 0 && isCandidate != 0) {
                if (index == target) chosen = static_cast<GameObj*>(object);
                ++index;
            }
        }
        const bool candidatesOk = ok != 0;
        ok = 0;
        if (candidatesOk && chosen != nullptr) ok = chosen->SetAutoscrapRequest(1, ctx);
    }
    return ok;
}

int Side::UpdateRdProgress(NotifyContext* ctx) {
    int result = 1;
    int shipyardPoints = 0;
    int trainingPoints = 0;
    int constructionPoints = 0;
    for (GameObjNotifier* object = GetFirstMasterInFamilies_90_98(); object; object = NextSystemMaster(object)) {
        if (result == 0) break;
        if (!sameControlSide(this, object)) continue;
        GameObjNotifier* system = object;
        ManuMgr* shipyards = static_cast<ManuMgr*>(static_cast<System*>(system)->System::FindManufacturingManagerBySlot123(rd_kind::kShipyard));
        ManuMgr* trainers = static_cast<ManuMgr*>(static_cast<System*>(system)->System::FindManufacturingManagerBySlot123(rd_kind::kTrainingFacil));
        ManuMgr* builders = static_cast<ManuMgr*>(static_cast<System*>(system)->System::FindManufacturingManagerBySlot123(rd_kind::kConstructionYard));
        result = (shipyards != nullptr && trainers != nullptr && builders != nullptr) ? 1 : 0;
        if (result != 0) {
            shipyardPoints += reinterpret_cast<ManuMgr*>(shipyards)->ManuMgr::CountHostFacilities(0);
            trainingPoints += reinterpret_cast<ManuMgr*>(trainers)->ManuMgr::CountHostFacilities(0);
            constructionPoints += reinterpret_cast<ManuMgr*>(builders)->ManuMgr::CountHostFacilities(0);
        }
    }
    if (result != 0) result = SetShipyardRdProgress(m_shipyardRdProgress + shipyardPoints, ctx);
    if (result != 0) {
        result = SetTrainingFacilRdProgress(m_trainingFacilRdProgress + trainingPoints, ctx);
        if (result != 0) result = SetConstructionYardRdProgress(m_constructionYardRdProgress + constructionPoints, ctx);
    }
    return result;
}

int Side::SetRdScalePercent(int percent) {
    const int accepted = percent > 0;
    if (accepted) m_rdScalePercent = percent;
    return accepted;
}

int Side::SetUnknownC4(int value) {
    const int accepted = value != 0;
    if (accepted) m_unkC4 = value;
    return accepted;
}

int Side::AddObjectRef(const ObjectKey* key, int* count, NotifyContext* unused) {
    *count = 0;
    int result = HasContainers();
    ObjectRefNode* found = nullptr;
    if (result) {
        for (AvlNode* node = m_objectRefs->First(); node && !found; node = node->m_successor) {
            ObjectRefNode* entry = static_cast<ObjectRefNode*>(node);
            if (entry->m_key.m_value == key->m_value) found = entry;
        }
        if (!found) {
            found = m_objectRefs->AddKey(*key);
            result = found != nullptr;
        }
        if (result) *count = ++found->m_count;
    }
    return result;
}

// ---- raw and refined materials ------------------------------------------------------------------------------

int Side::TakeRawMaterial(int* took, NotifyContext* ctx) {
    if (g_setupComplete == 0) {
        *took = 1;
        return 1;
    }
    *took = m_rawMaterials > 0 ? 1 : 0;
    if (*took != 0) return SetRawMaterials(m_rawMaterials - 1, ctx);
    return 1;
}

int Side::TakeRefinedMaterial(int* took, NotifyContext* ctx) {
    *took = m_refinedMaterials > 0 ? 1 : 0;
    if (*took != 0) return SetRefinedMaterials(m_refinedMaterials - 1, ctx);
    return 1;
}

int Side::PushRefineryQueue(KeyQueueNode* node, NotifyContext* ctx) {
    int result = 0;
    if (node && m_refineryQueue && m_refineryQueue->Add(node)) {
        result = SetWaitingRefineries(m_waitingRefineries + 1, ctx);
    }
    return result;
}

int Side::PushManufacturerQueue(KeyQueueNode* node, NotifyContext* ctx) {
    int result = 0;
    if (node && m_manufacturerQueue && m_manufacturerQueue->Add(node)) {
        result = SetWaitingManufacturers(m_waitingManufacturers + 1, ctx);
    }
    return result;
}

KeyQueueNode* Side::PopRefineryQueue(NotifyContext* ctx) {
    KeyQueueNode* node = nullptr;
    if (m_refineryQueue) {
        node = static_cast<KeyQueueNode*>(m_refineryQueue->Dequeue());
        if (node) SetWaitingRefineries(m_waitingRefineries - 1, ctx);
    }
    return node;
}

KeyQueueNode* Side::PopManufacturerQueue(NotifyContext* ctx) {
    KeyQueueNode* node = nullptr;
    if (m_manufacturerQueue) {
        node = static_cast<KeyQueueNode*>(m_manufacturerQueue->Dequeue());
        if (node) SetWaitingManufacturers(m_waitingManufacturers - 1, ctx);
    }
    return node;
}

int Side::EnqueueRefinery(KeyQueueNode* node, NotifyContext* ctx) { return node ? PushRefineryQueue(node, ctx) : 0; }

int Side::EnqueueManufacturer(KeyQueueNode* node, NotifyContext* ctx) {
    return node ? PushManufacturerQueue(node, ctx) : 0;
}

namespace {

// Queue entry for a waiting facility: its key and its current request number.
KeyQueueNode* newWaitingEntry(ProductionFacility* facility) {
    KeyQueueNode* node = new KeyQueueNode();
    if (!node) return nullptr;
    if (node->m_flag == 0) {
        delete node;
        return nullptr;
    }
    ObjectKey key;
    facility->GetKey(&key);
    node->m_key = key;
    node->m_value = static_cast<int>(facility->GetCycleCount());
    return node;
}

}  // namespace

int Side::AcquireRawMaterial(ProductionFacility* facility, NotifyContext* ctx) {
    int result = 1;
    if ((facility->m_procFlags & kProcFacilWaitingFlag) == 0 && facility->m_state == 1) {
        int took = 0;
        result = TakeRawMaterial(&took, ctx);
        if (result != 0) {
            if (took == 0) {
                result = 0;
                if (KeyQueueNode* node = newWaitingEntry(facility)) result = EnqueueRefinery(node, ctx);
            } else {
                result = facility->SetPointPresent(1, ctx);
            }
        }
    }
    return result;
}

int Side::AcquireRefinedMaterial(ProductionFacility* facility, NotifyContext* ctx) {
    int result = 1;
    if ((facility->m_procFlags & kProcFacilWaitingFlag) == 0 && facility->m_state == 1) {
        int took = 0;
        result = TakeRefinedMaterial(&took, ctx);
        if (result != 0) {
            if (took == 0) {
                result = 0;
                if (KeyQueueNode* node = newWaitingEntry(facility)) result = EnqueueManufacturer(node, ctx);
            } else {
                result = facility->SetPointPresent(1, ctx);
            }
        }
    }
    return result;
}

int Side::ReleaseRawMaterial(ProductionFacility* facility, NotifyContext* ctx) {
    int result = isReleasable(facility) ? 1 : 0;
    if (result) result = facility->SetPointPresent(0, ctx);
    if (result) result = SetRawMaterials(m_rawMaterials + 1, ctx);
    return result;
}

int Side::ReleaseRefinedMaterial(ProductionFacility* facility, NotifyContext* ctx) {
    int result = isReleasable(facility) ? 1 : 0;
    if (result) result = facility->SetPointPresent(0, ctx);
    if (result) result = SetRefinedMaterials(m_refinedMaterials + 1, ctx);
    return result;
}

int Side::ServeRefineryQueue(int* more, NotifyContext* ctx) {
    *more = 0;
    int result = 1;
    if (m_waitingRefineries > 0 && m_rawMaterials > 0) {
        KeyQueueNode* node = PopRefineryQueue(ctx);
        ObjectKey key;
        node->GetKey(&key);
        GameObjNotifier* object = GetMasterInFamilies_28_30(&key);
        if (object != nullptr && asFacility(object)->m_state == 1) {
            ProductionFacility* facility = asFacility(object);
            if (static_cast<uint32_t>(node->m_value) == facility->GetCycleCount()) {
                int took = 0;
                result = TakeRawMaterial(&took, ctx);
                if (result != 0 && took != 0) result = facility->SetPointPresent(1, ctx);
            }
        }
        delete node;
    }
    if (result != 0) *more = (m_waitingRefineries < 1 || m_rawMaterials < 1) ? 0 : 1;
    return result;
}

int Side::ServeManufacturerQueue(int* more, NotifyContext* ctx) {
    *more = 0;
    int result = 1;
    if (m_waitingManufacturers > 0 && m_refinedMaterials > 0) {
        KeyQueueNode* node = PopManufacturerQueue(ctx);
        ObjectKey key;
        node->GetKey(&key);
        GameObjNotifier* object = GetMasterInFamilies_28_30(&key);
        if (object != nullptr && asFacility(object)->m_state == 1) {
            ProductionFacility* facility = asFacility(object);
            if (static_cast<uint32_t>(node->m_value) == facility->GetCycleCount()) {
                int took = 0;
                result = TakeRefinedMaterial(&took, ctx);
                if (result != 0 && took != 0) result = facility->SetPointPresent(1, ctx);
            }
        }
        delete node;
    }
    if (result != 0) *more = (m_waitingManufacturers < 1 || m_refinedMaterials < 1) ? 0 : 1;
    return result;
}

// ---- timed events -------------------------------------------------------------------------------------------

namespace {

template <class Event>
int scheduleEvent(Side* side) {
    void* memory = ::operator new(sizeof(Event));
    Event* event = memory ? new (memory) Event() : nullptr;
    int result = 0;
    if (event) {
        result = event->m_flag;
        if (result == 0) {
            delete event;
        } else {
            ObjectKey key;
            side->GetKey(&key);
            event->m_key = key;
            CallGameHook2AF8(static_cast<int>(reinterpret_cast<intptr_t>(event)));
        }
    }
    return result;
}

}  // namespace

int Side::ScheduleRefineryEvent() { return scheduleEvent<RefineryQueueEvent>(this); }

int Side::ScheduleManufacturerEvent() { return scheduleEvent<ManufacturerQueueEvent>(this); }
