#include "data/game_math.h"
#include "data/sd_record_bases.h"
#include "economy/manu_mgr.h"
#include "economy/manu_mgr_decl.h"
#include "economy/procfacil.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_database.h"
#include "gameobj/object_queries.h"
#include "gameobj/object_type_db.h"

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kManuStatusCategory = 0xa0;
constexpr int kManuNoFacility = 3;
constexpr int kManuFacilitiesOutOfService = 4;
constexpr int kDestroyedByReplacement = 0x16;
constexpr uint32_t kOutOfService = 0x1;
constexpr uint32_t kControlSideShift = 6;
constexpr uint32_t kControlSideMask = 3;
constexpr uint32_t kSideMaskBits = 0xf;
constexpr uint32_t kAllianceOnly = 1;
constexpr int kModeIdle = 4;
constexpr int kFirstBusyMode = 1;
constexpr int kBusyModeLimit = 4;
constexpr uint32_t kFirstUnitFamily = 0x10;
constexpr uint32_t kUnitFamilyLimit = 0x40;

bool isOk(const StatusCode* status) { return status->m_second == static_cast<uint32_t>(kStatusOk); }

void setStatus(StatusCode* out, int category, int code) { *out = StatusCode(category, code); }

// Seconds-per-point style number in the facility's data record (word at +0x5c), 0 without a record.
int facilityRecordWord(GameObjNotifier* facility) {
    if (!facility->m_record) return 0;
    return static_cast<int>(static_cast<SdRecordExt2*>(facility->m_record)->m_field5c);
}

bool isInService(GameObjNotifier* facility) { return (AsFacilityFields(facility)->m_flags & kOutOfService) == 0; }

uint32_t recordKeyOf(DatRecord* record) { return (record->m_third << 24) | (record->m_id & 0xffffff); }

ObjectKey objectRecordKey(GameObjNotifier* object) {
    ObjectKey key;
    key.m_value = object->m_record ? recordKeyOf(object->m_record) : 0;
    return key;
}

ObjectKey nodeKey(AvlNode* node) {
    ObjectKey key;
    static_cast<ObjectKeyValueNode*>(node)->GetKey(&key);
    return key;
}

ObjectKey* nodeValueAsKey(AvlNode* node) {
    return reinterpret_cast<ObjectKey*>(&static_cast<ObjectKeyValueNode*>(node)->m_value);
}

AvlNode* nextNode(AvlNode* node) { return node->m_successor; }

}  // namespace

// ---- rates and estimates -----------------------------------------------------------------------------------

int ManuMgr::GetProductionRate(StatusCode* status, int* rate) {
    int ok = 1;
    *status = StatusCode(kStatusCategory, kStatusOk);
    *rate = 0;
    if (!HasHostFacility(0)) {
        if (!HasHostFacility(1)) {
            setStatus(status, kManuStatusCategory, kManuNoFacility);
        } else {
            setStatus(status, kManuStatusCategory, kManuFacilitiesOutOfService);
        }
    } else {
        GameObjNotifier* host = GetHostSystem();
        ok = host != nullptr;
        if (host) {
            ManufacturingFacilityIterator facilities(host);
            facilities.Begin();
            while (facilities.Current()) {
                GameObjNotifier* facility = facilities.Current();
                if (IsCompatibleFacility(static_cast<GameObj*>(facility)) && isInService(facilities.Current())) {
                    *rate += RatioToPercent(1, facilityRecordWord(facilities.Current()));
                }
                facilities.Advance();
            }
        }
    }
    return ok;
}

int ManuMgr::GetPointsForTime(int time, int includeProgress, StatusCode* status, int* points) {
    int ok = 1;
    *status = StatusCode(kStatusCategory, kStatusOk);
    *points = 0;
    if (!HasHostFacility(0)) {
        if (!HasHostFacility(1)) {
            setStatus(status, kManuStatusCategory, kManuNoFacility);
        } else {
            setStatus(status, kManuStatusCategory, kManuFacilitiesOutOfService);
        }
    } else {
        GameObjNotifier* host = GetHostSystem();
        ok = host != nullptr;
        if (host) {
            ManufacturingFacilityIterator facilities(host);
            facilities.Begin();
            while (facilities.Current()) {
                GameObjNotifier* facility = facilities.Current();
                if (IsCompatibleFacility(static_cast<GameObj*>(facility)) && isInService(facilities.Current())) {
                    int turns = time;
                    if (includeProgress != 0 && AsFacilityFields(facilities.Current())->m_progress != 0) {
                        const int progress = AsFacilityFields(facilities.Current())->m_progress;
                        const int word = facilityRecordWord(facilities.Current());
                        const int clock = CallGameHook2B08();
                        turns = word + clock + (time - progress);
                    }
                    *points += turns / facilityRecordWord(facilities.Current());
                }
                facilities.Advance();
            }
        }
    }
    return ok;
}

int ManuMgr::EstimateBuildTime(const NullableKey* seed, int count, int subtractProgress, StatusCode* status,
                               int* turns) {
    bool isUnitRecord = true;
    *status = StatusCode(kStatusCategory, kStatusOk);
    *turns = 0;
    ObjectKey seedKey;
    seedKey.m_value = seed->m_value;
    DatRecord* record = FindDataRecord(AsLookupKey(&seedKey));
    int cost = 0;
    if (record) {
        const uint32_t family = recordKeyOf(record) >> 24;
        if (family < kFirstUnitFamily || family >= kUnitFamilyLimit) isUnitRecord = false;
        if (isUnitRecord) {
            cost = static_cast<int>(static_cast<SdRecord*>(record)->GetField48());
            if (count >= 0) {
                cost = count * cost;
                if (subtractProgress != 0) cost = cost - (m_overflowPointCount + m_completedPointCount);
                if (cost < 0) cost = 0;
            }
        }
    }

    int rate = 0;
    int result = GetProductionRate(status, &rate) && record != nullptr;
    if (isOk(status) && rate != 0) {
        int estimate = DivideRoundUp(cost * 100, rate);
        int longest = 0;
        int points = 0;
        const int enough = GetPointsForTime(estimate, subtractProgress, status, &points);
        result = enough && result;
        if (isOk(status)) {
            if (points < cost) {
                GameObjNotifier* host = GetHostSystem();
                if (host) {
                    ManufacturingFacilityIterator facilities(host);
                    facilities.Begin();
                    while (facilities.Current()) {
                        if (IsCompatibleFacility(static_cast<GameObj*>(facilities.Current()))) {
                            if (isInService(facilities.Current())) {
                                if (longest == 0 || longest < facilityRecordWord(facilities.Current())) {
                                    longest = facilityRecordWord(facilities.Current());
                                }
                            }
                        }
                        facilities.Advance();
                    }
                }
                const int upper = longest + estimate;
                while (estimate <= upper && *turns == 0) {
                    if (!isOk(status)) return result;
                    const int again = GetPointsForTime(estimate, subtractProgress, status, &points);
                    result = again && result;
                    if (isOk(status) && cost <= points) *turns = estimate;
                    ++estimate;
                }
                if (isOk(status) && *turns == 0) *turns = upper;
            } else {
                while (estimate >= 0 && *turns == 0 && isOk(status)) {
                    const int again = GetPointsForTime(estimate, subtractProgress, status, &points);
                    result = again && result;
                    if (isOk(status) && points < cost) *turns = estimate + 1;
                    --estimate;
                }
            }
        }
    }
    return result;
}

int ManuMgr::EstimateBuildTimeFresh(const NullableKey* seed, int count, StatusCode* status, int* turns) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    return EstimateBuildTime(seed, count, 0, status, turns) != 0;
}

int ManuMgr::EstimateQueuedBuildTime(const ObjectKey* queued, StatusCode* status, int* turns) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    *turns = 0;
    if (m_remainingGameObjCount == 0) setStatus(status, kManuStatusCategory, 1);
    if ((m_flags & kSideMaskBits) != kAllianceOnly) return 1;
    int position = 1;
    if (!isOk(status)) return 1;
    AvlNode* node = static_cast<ManuMgr*>(ViewAs(0))->GetQueue()->First();
    while (node) {
        if (queued->m_value == nodeKey(node).m_value) break;
        node = nextNode(node);
        ++position;
    }
    if (!node) setStatus(status, kManuStatusCategory, 5);
    if (!isOk(status)) return 1;
    int result = 0;
    if (node) {
        ObjectKey seed;
        GetSeedKey(&seed);
        if (reinterpret_cast<NullableKey*>(&seed)->IsSet()) result = 1;
    }
    if (node) {
        ObjectKey seed;
        GetSeedKey(&seed);
        if (reinterpret_cast<NullableKey*>(&seed)->IsSet()) {
            const int estimated =
                EstimateBuildTime(reinterpret_cast<NullableKey*>(&seed), position, 1, status, turns);
            result = estimated && result;
        }
    }
    return result;
}

int ManuMgr::EstimateLastQueuedBuildTime(StatusCode* status, int* turns) {
    int ok = 1;
    *status = StatusCode(kStatusCategory, kStatusOk);
    *turns = 0;
    if (m_remainingGameObjCount == 0) setStatus(status, kManuStatusCategory, 1);
    if ((m_flags & kSideMaskBits) == kAllianceOnly && isOk(status)) {
        AvlNode* last = static_cast<ManuMgr*>(ViewAs(0))->GetQueue()->Last();
        ok = last != nullptr;
        if (last) {
            ObjectKey key = nodeKey(last);
            const int estimated = EstimateQueuedBuildTime(&key, status, turns);
            ok = estimated && ok;
        }
    }
    return ok;
}

// ---- queue upkeep --------------------------------------------------------------------------------------------

int ManuMgr::OnProductChanged(const ObjectKey* key, int a, int b, NotifyContext* ctx) {
    int result = 1;
    ObjectKey product(m_productKey);
    if (key->m_value == product.m_value) result = UpdateDeploymentKey(ctx);
    return result;
}

int ManuMgr::MoveOverflowToCompleted(const ObjectKey* key, int unused, NotifyContext* ctx) {
    int result = UpdateQueueMembership(key, ctx);
    if (result) {
        const int moved = MinInt(m_requiredPointCount, m_overflowPointCount);
        result = SetCompletedPointCount(moved, ctx);
        if (result) {
            result = SetOverflowPointCount(m_overflowPointCount - moved, ctx);
            if (result) result = RebalanceFacilities(ctx);
        }
    }
    return result;
}

int ManuMgr::HandleProductEvent(const ObjectKey* key, int a, int b, int c, NotifyContext* ctx) {
    int result = UpdateQueueMembership(key, ctx);
    if (result) result = RebalanceFacilities(ctx);
    return result;
}

int ManuMgr::AdvanceProgress(NotifyContext* ctx) {
    int result;
    if (m_completedPointCount < m_requiredPointCount) {
        result = SetCompletedPointCount(m_completedPointCount + 1, ctx);
    } else {
        result = SetOverflowPointCount(m_overflowPointCount + 1, ctx);
    }
    if (result) result = RebalanceFacilities(ctx);
    return result;
}

int ManuMgr::UpdateQueueMembership(const ObjectKey* key, NotifyContext* ctx) {
    int result = 1;
    int added = 0;
    int removed = 0;
    GameObjNotifier* product = GetMasterInFamilies_01_ff(key);
    if (product) {
        ObjectKeyValueSet& queue = static_cast<ManuMgrData*>(m_data)->m_queue;
        if (IsAcceptableProduct(static_cast<GameObj*>(product))) {
            added = queue.Find(key) == nullptr;
            if (added) queue.AddKeyValue(key, 0);
        } else {
            removed = queue.Find(key) != nullptr;
            if (removed) queue.RemoveKey(key);
        }
    }
    if (added || removed) {
        result = UpdateTotalRequired(ctx);
        if (result) {
            result = UpdateRemainingCount(ctx);
            if (result) result = ChooseProduct(ctx);
        }
    }
    return result;
}

int ManuMgr::UpdateRemainingCount(NotifyContext* ctx) {
    const int length = static_cast<ManuMgrData*>(m_data)->m_queue.Count() & 0xffff;
    return SetRemainingGameObjCount(length, ctx);
}

int ManuMgr::UpdateSeedKey(NotifyContext* ctx) {
    int result = 1;
    NullableKey seed;
    seed.m_value = 0;
    ObjectKey productCopy(m_productKey);
    if (productCopy.OrNull() == nullptr) {
        seed.m_value = 0;
    } else {
        GameObjNotifier* product = GetProduct();
        result = product != nullptr;
        if (!result) return 0;
        seed.m_value = objectRecordKey(product).m_value;
    }
    if (result) result = SetSeedKey(&seed, ctx);
    return result;
}

int ManuMgr::UpdateRequiredFromSeed(NotifyContext* ctx) {
    int ok = 1;
    int cost = 0;
    ObjectKey seed;
    GetSeedKey(&seed);
    if (reinterpret_cast<NullableKey*>(&seed)->OrNull() != nullptr) {
        ObjectKey lookup;
        GetSeedKey(&lookup);
        DatRecord* record = FindDataRecord(AsLookupKey(&lookup));
        ok = record != nullptr;
        if (!ok) return 0;
        cost = static_cast<int>(static_cast<SdRecord*>(record)->GetField48());
    }
    int result = 0;
    if (ok) result = SetRequiredPointCount(cost, ctx);
    return result;
}

int ManuMgr::UpdateTotalRequired(NotifyContext* ctx) {
    int ok = 1;
    int total = 0;
    AvlNode* node = static_cast<ManuMgrData*>(m_data)->m_queue.First();
    do {
        if (!node) break;
        ObjectKey key = nodeKey(node);
        GameObjNotifier* product = GetMasterInFamilies_01_ff(&key);
        ok = 0;
        if (product) {
            ObjectKey recordKey = objectRecordKey(product);
            DatRecord* record = FindDataRecord(AsLookupKey(&recordKey));
            ok = record != nullptr;
            if (ok) total += static_cast<int>(static_cast<SdRecord*>(record)->GetField48());
        }
        node = nextNode(node);
    } while (ok);
    int result = 0;
    if (ok) result = SetTotalRequiredPointCount(total, ctx);
    return result;
}

int ManuMgr::ChooseProduct(NotifyContext* ctx) {
    int result = 1;
    ManuMgrData* data = static_cast<ManuMgrData*>(m_data);
    if (m_remainingGameObjCount == 0) {
        result = data->m_queue.First() == nullptr;
        if (!result) return result;
        ObjectKey none;
        return SetProductKey(&none, ctx);
    }
    GameObjNotifier* current = GetProduct();
    if (current && IsAcceptableProduct(static_cast<GameObj*>(current))) return result;
    ObjectKey chosen;
    AvlNode* node = data->m_queue.First();
    while (node && chosen.OrNull() == nullptr) {
        ObjectKey key = nodeKey(node);
        GameObjNotifier* candidate = GetMasterInFamilies_01_ff(&key);
        result = candidate != nullptr;
        if (result && IsAcceptableProduct(static_cast<GameObj*>(candidate))) {
            candidate->GetKey(&chosen);
        }
        node = nextNode(node);
        if (!result) break;
    }
    if (result) result = SetProductKey(&chosen, ctx);
    return result;
}

int ManuMgr::UpdateDeploymentKey(NotifyContext* ctx) {
    int result = 1;
    ObjectKey key;
    ObjectKey productCopy(m_productKey);
    if (productCopy.OrNull() == nullptr) {
        key = m_targetKey;
    } else {
        GameObjNotifier* product = GetProduct();
        result = product != nullptr;
        if (!result) return 0;
        result = static_cast<GameObj*>(product)->GetValidParentKey(&key);
    }
    if (result) result = SetDeploymentKey(&key, ctx);
    return result;
}

int ManuMgr::UpdateProductName(NotifyContext* ctx) {
    int result = 1;
    String name;
    ObjectKey productCopy(m_productKey);
    if (productCopy.OrNull() == nullptr) {
        String empty("");
        name.Assign(empty);
    } else {
        GameObjNotifier* product = GetProduct();
        result = product != nullptr;
        if (!result) return 0;
        name.Assign(*static_cast<GameObj*>(product)->GetName());
    }
    if (result) result = SetProductName(&name, ctx);
    return result;
}

int ManuMgr::CompleteProduct(GameObj* product, NotifyContext* ctx) {
    ObjectKey managerOwner;
    ObjectKey productOwner;
    const int managerHasOwner = GetOwnerKey(&managerOwner);
    const int productHasOwner = product->GetOwnerKey(&productOwner);
    int ok = productHasOwner && managerHasOwner;
    ok = product->SetConstructed(1, ctx) && ok;
    if (managerOwner.m_value != productOwner.m_value) {
        if (managerOwner.OrNull() && productOwner.OrNull()) ok = product->SetEnrouteActive(1, ctx) && ok;
    }
    ok = product->SetCompleted(1, ctx) && ok;
    return ok;
}

// ---- creating and moving products ----------------------------------------------------------------------------

int ManuMgr::DestroyProducts(NotifyContext* ctx) {
    int result = 1;
    AvlNode* node = static_cast<ManuMgrData*>(m_data)->m_queue.First();
    while (node) {
        ObjectKey key = nodeKey(node);
        GameObj* product = static_cast<GameObj*>(GetMasterInFamilies_01_ff(&key));
        result = 0;
        if (product) result = product->ChangeDestroyedReason(kDestroyedByReplacement, ctx);
        node = nextNode(node);
        if (!result) return 0;
    }
    return result;
}

int ManuMgr::CreateProducts(ObjectKeyValueSet* products, int unused, NotifyContext* ctx) {
    int result = 1;
    AvlNode* node = products->First();
    while (node) {
        ObjectKey key = nodeKey(node);
        GameObj* location = static_cast<GameObj*>(GetMasterInFamilies_01_ff(&key));
        result = 0;
        if (location) {
            GameObj* child = location->CreateChild(nodeValueAsKey(node),
                                                   (m_flags >> kControlSideShift) & kControlSideMask, ctx, 1, 0);
            result = 0;
            if (child) {
                ObjectKey self;
                GetKey(&self);
                result = child->SetBuilderKey(&self, ctx);
            }
        }
        node = nextNode(node);
        if (!result) return 0;
    }
    return result;
}

int ManuMgr::RenameProductIfAny(String* name, NotifyContext* ctx) {
    int result = 1;
    ObjectKey productCopy(m_productKey);
    if (productCopy.IsSet()) result = SetProductName(name, ctx);
    return result;
}

int ManuMgr::ClearReserved(int value, NotifyContext* ctx) { return SetReserved(value, ctx); }

int ManuMgr::ReparentQueue(ObjectKeyValueSet* products, const ObjectKey* target, NotifyContext* ctx) {
    ObjectKey targetCopy(*target);
    int result = SetTargetKey(&targetCopy, ctx);
    if (result) {
        AvlNode* node = products->First();
        while (node) {
            GameObj* object = static_cast<GameObj*>(GetMasterInFamilies_01_ff(nodeValueAsKey(node)));
            result = 0;
            if (object) {
                ObjectKey newParent = nodeKey(node);
                result = object->Reparent(&newParent, ctx);
            }
            node = nextNode(node);
            if (!result) break;
        }
    }
    return result;
}

// ---- facility rebalancing --------------------------------------------------------------------------------------

namespace {

struct FacilityCounts {
    int total = 0;
    int mode1 = 0;
    int mode2 = 0;
    int mode3 = 0;
    int idle = 0;
};

}  // namespace

int ManuMgr::RebalanceFacilities(NotifyContext* ctx) {
    FacilityCounts counts;
    GameObjNotifier* host = GetHostSystem();
    int result = host != nullptr;
    if (result) {
        ManufacturingFacilityIterator facilities(host);
        facilities.Begin();
        while (facilities.Current()) {
            GameObjNotifier* facility = facilities.Current();
            if (IsCompatibleFacility(static_cast<GameObj*>(facility))) {
                ++counts.total;
                if (isInService(facility)) {
                    switch (AsFacilityFields(facility)->m_mode) {
                        case 1: ++counts.mode1; break;
                        case 2: ++counts.mode2; break;
                        case 3: ++counts.mode3; break;
                        case 4: ++counts.idle; break;
                    }
                }
            }
            facilities.Advance();
        }
    }
    if (!result) return result;

    int working = counts.mode1 + counts.mode3 + counts.mode2;
    if (counts.total == 0) return DestroyProducts(ctx);

    const int wanted = (m_totalRequiredPointCount - m_overflowPointCount) - m_completedPointCount;
    if (working < wanted) {
        int toStart = wanted - working;
        if (counts.idle < toStart) toStart = counts.idle;
        GameObjNotifier* best;
        do {
            const int previous = result;
            best = nullptr;
            ManufacturingFacilityIterator facilities(host);
            facilities.Begin();
            while (facilities.Current()) {
                GameObjNotifier* facility = facilities.Current();
                if (IsCompatibleFacility(static_cast<GameObj*>(facility)) && isInService(facility) &&
                    AsFacilityFields(facility)->m_mode == kModeIdle) {
                    if (!best || facilityRecordWord(best) > facilityRecordWord(facilities.Current())) {
                        best = facilities.Current();
                    }
                }
                facilities.Advance();
            }
            if (best) {
                const int started = static_cast<ProcFacil*>(best)->Restart(ctx);
                result = started && previous;
                --toStart;
            }
        } while (best && toStart != 0);
    } else if (wanted < working) {
        int excess = working - wanted;
        int mode = kFirstBusyMode;
        do {
            if (excess == 0) return result;
            GameObjNotifier* best;
            do {
                const int previous = result;
                best = nullptr;
                ManufacturingFacilityIterator facilities(host);
                facilities.Begin();
                while (facilities.Current()) {
                    GameObjNotifier* facility = facilities.Current();
                    if (IsCompatibleFacility(static_cast<GameObj*>(facility)) && isInService(facility) &&
                        AsFacilityFields(facility)->m_mode == mode) {
                        if (!best || facilityRecordWord(best) < facilityRecordWord(facilities.Current())) {
                            best = facilities.Current();
                        }
                    }
                    facilities.Advance();
                }
                if (best) {
                    const int released = static_cast<ProcFacil*>(best)->ResetToIdle(ctx);
                    result = released && previous;
                    --excess;
                }
            } while (best && excess != 0);
            ++mode;
        } while (mode < kBusyModeLimit);
    }
    return result;
}
