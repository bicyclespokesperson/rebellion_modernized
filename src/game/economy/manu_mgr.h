#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "core/layout.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_family.h"

// ManuMgr (families 0xa0-0xaf, ctor 0x5279d0, VT 0x65fbf8, secondary ObjectBase VT 0x65fbf0, 0x8c bytes) is the abstract
// base of the manufacturing managers: one per (system, kind of product). It owns the build queue of its system: the
// product objects (ships, troops, ...) that are being built are real game objects, created as children of the target
// system with this manager as their builder (GameObj::m_builderKey). The queue is the ObjectKeyValueSet in its data
// block (ManuMgrData). Its parent is the system; the facilities that do the work are the manufacturing facilities
// (families 0x28-0x2b) below that same system. Vtable: 151 slots, 0-110 are GameObj's, 111-150 new. Leaf classes
// (economy/manu_mgr_leaves.h: ShipyardManuMgr, ConstructionYardManuMgr, TrainingFacilManuMgr) implement slots 1, 5-9, 15, 22, 37, 57, 122 and 123.
//
// Eleven properties follow GameObj's SetX -> ApplyX -> OnXChanged / OnXNotified scheme (see game_obj.h):
//
//   field                      Set / Apply            Changed / Notified slot
//   RemainingGameObjCount +58  0x529480 / 0x52aed0    129 / 140  queue length
//   CompletedPointCount   +5c  0x5294e0 / 0x52af50    130 / 141  build points done on the current product
//   OverflowPointCount    +60  0x529540 / 0x52afd0    131 / 142  points the facilities delivered beyond the need
//   SeedKey               +64  0x5295a0 / 0x52b050    132 / 143  record key of the current product's type
//   RequiredPointCount    +68  0x529650 / 0x52b170    133 / 144  cost of the current product
//   TotalRequiredPointCount +6c 0x5296b0 / 0x52b1f0   134 / 145  cost of the whole queue
//   Reserved              +70  0x529710 / 0x52b270    135 / 146  (event 0x163, a flag)
//   ProductKey            +74  0x529770 / 0x52b2f0    136 / 147  the product being built right now (event 0x320)
//   DeploymentKey         +78  0x529820 / 0x52b410    137 / 148  where the finished product goes (event 0x321)
//   TargetKey             +7c  0x5298d0 / 0x52b530    138 / 149  the system the manager works for
//   ProductName           +80  0x529980 / 0x52b650    139 / 150  name of the current product (event 0x520)

class Crc32;
class EventKindSet;
class ManuMgr;

// The data block (VT 0x66a0c8, 0x20 bytes, ctor 0x583800): GameObjData plus the queue (keys of the product objects).
class ManuMgrData : public GameObjData {
public:
    ManuMgrData();                                // 0x583800
    virtual ~ManuMgrData();                       // 0x583870 (scalar deleting destructor 0x583850)
    virtual int Read(istream* in) override;       // 0x5838c0
    virtual int Write(ostream* out) override;     // 0x5838f0
    virtual int Checksum(Crc32* crc) override;    // 0x583920

    ObjectKeyValueSet m_queue;  // +0x18
};

class ManuMgr : public GameObj {
public:
    ManuMgr();                // 0x5279d0
    virtual ~ManuMgr();       // 0x527a90 (deleting), 0x527ab0

    virtual int GetEventKinds(int unused, EventKindSet* out) override;  // 15, 0x527c20
    virtual int Virtual16() override { return 1; }                      // 16, 0x40f340
    virtual int IsObservable() override { return 0; }                   // 19, 0x6158b0
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;  // 22, 0x527c40
    virtual GameObjData* CreateData() override;                          // 58, 0x52c010
    virtual int OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) override;  // 60, 0x52b790
    virtual int OnCreatedChanged(int newValue, NotifyContext* ctx) override;                               // 69, 0x52b820
    virtual int OnExistingChanged(int newValue, NotifyContext* ctx) override;                              // 74, 0x52b7d0

    // Persistence entry points: pure here (the bodies are called qualified by the leaf classes).
    virtual int Read(istream* in) override = 0;             // 5, body 0x52a470
    virtual int Write(ostream* out) override = 0;           // 6, body 0x52a510
    virtual int Checksum(Crc32* crc) override = 0;          // 7, body 0x52a660
    virtual int Validate() override = 0;                    // 8, body 0x52a6f0: base check and queue length
    virtual int ApplyAll(NotifyContext* ctx) override = 0;  // 9, body 0x52a5b0: side views only

    // ---- slots 111-128: request checks ("Impl" fills a status, the wrapper starts from {1,-1} and maps a refusal
    // to {1,1}); names are provisional ----
    // Ok only if the system has a usable manufacturing facility for this kind of product; else {0xa0,3}.
    virtual int CheckFacilityAvailableImpl(const ActivityRequest* request, StatusCode* out);              // 111, 0x527d00
    virtual int CheckFacilityAvailable(const ActivityRequest* request, int unused, StatusCode* out);      // 112, 0x527e90
    // Master only: forwards the request, with kind 0x203, to the current product (its slot 25); {0xa0,1} without one.
    virtual int CheckProductActionImpl(const ActivityRequest* request, StatusCode* out);                  // 113, 0x527f40
    virtual int CheckProductAction(const ActivityRequest* request, int unused, StatusCode* out);          // 114, 0x5281c0
    // Master only: {0xa0,1} unless there is a current product.
    virtual int CheckHasProductImpl(const ActivityRequest* request, StatusCode* out);                     // 115, 0x528270
    virtual int CheckHasProduct(const ActivityRequest* request, StatusCode* out);                         // 116, 0x528360
    virtual int GetReserved(int unused, int* out);                                                        // 117, 0x528410
    virtual int CheckPlainImpl(const ActivityRequest* request, int unused, StatusCode* out);              // 118, 0x528430
    virtual int CheckPlain(const ActivityRequest* request, int unused, StatusCode* out);                  // 119, 0x5284e0
    virtual int CheckPlain2(const ActivityRequest* request, StatusCode* out);                             // 120, 0x5285a0
    // As 120, and the request's key must name an object (any view).
    virtual int CheckRequestedObject(const ActivityRequest* request, int unused, StatusCode* out);        // 121, 0x528650
    virtual int Virtual122() = 0;                                                                         // 122
    // The kind of product this manager builds; a facility serves it if its slot 129 returns the same value.
    virtual int GetFacilityKind() = 0;                                                                    // 123
    // ---- slots 124-128: orders ----
    // Destroys the current queue, then creates the products listed in `products` (location key -> record key).
    virtual int ReplaceQueue(const ActivityRequest* request, ObjectKeyValueSet* products, NotifyContext* ctx);  // 124, 0x529ab0
    // Rename check (slot 114), then renames the current product (if any) to `name`.
    virtual int RenameProduct(const ActivityRequest* request, String* name, NotifyContext* ctx);  // 125, 0x529b00
    virtual int CancelProducts(const ActivityRequest* request, NotifyContext* ctx);              // 126, 0x529ba0
    virtual int ReserveChecked(const ActivityRequest* request, int value, NotifyContext* ctx);   // 127, 0x529c30
    // Sets the target key to the request's key and moves every product of `products` below its listed parent.
    virtual int ReparentProducts(const ActivityRequest* request, ObjectKeyValueSet* products, NotifyContext* ctx);  // 128, 0x529cd0

    // ---- slots 129-139: OnXChanged (old, new, ctx) ----
    virtual int OnRemainingGameObjCountChanged(int oldValue, int newValue, NotifyContext* ctx);       // 129, 0x52b8c0
    virtual int OnCompletedPointCountChanged(int oldValue, int newValue, NotifyContext* ctx);         // 130, 0x52b960
    virtual int OnOverflowPointCountChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }       // 131
    virtual int OnSeedKeyChanged(NullableKey* oldKey, NullableKey* newKey, NotifyContext* ctx);       // 132, 0x52b9b0
    virtual int OnRequiredPointCountChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }       // 133
    virtual int OnTotalRequiredPointCountChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 134
    virtual int OnReservedChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }                 // 135
    virtual int OnProductKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);        // 136, 0x52b9c0
    virtual int OnDeploymentKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) { return 1; }  // 137
    virtual int OnTargetKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);         // 138, 0x52b9f0
    virtual int OnProductNameChanged(String* oldName, String* newName, NotifyContext* ctx);           // 139, 0x52ba00

    // ---- slots 140-150: OnXNotified ----
    virtual int OnRemainingGameObjCountNotified(int oldValue, int newValue, NotifyContext* ctx);       // 140, 0x52baa0
    virtual int OnCompletedPointCountNotified(int oldValue, int newValue, NotifyContext* ctx);         // 141, 0x52baf0
    virtual int OnOverflowPointCountNotified(int oldValue, int newValue, NotifyContext* ctx);          // 142, 0x52bb40
    virtual int OnSeedKeyNotified(NullableKey* oldKey, NullableKey* newKey, NotifyContext* ctx);       // 143, 0x52bb80
    virtual int OnRequiredPointCountNotified(int oldValue, int newValue, NotifyContext* ctx);          // 144, 0x52bbd0
    virtual int OnTotalRequiredPointCountNotified(int oldValue, int newValue, NotifyContext* ctx);     // 145, 0x52bc20
    virtual int OnReservedNotified(int oldValue, int newValue, NotifyContext* ctx);                    // 146, 0x52bc60
    virtual int OnProductKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);        // 147, 0x52bcb0
    virtual int OnDeploymentKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);     // 148, 0x52bd00
    virtual int OnTargetKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);         // 149, 0x52bd50
    virtual int OnProductNameNotified(String* oldName, String* newName, NotifyContext* ctx);           // 150, 0x52bd90

    // ==== SECTION core (manu_mgr_core.cpp) ====
    // Builds a request {side, 0, no key, 0x215} and runs CheckProductActionImpl on it.
    int CheckProductEvent215(int unused, int side, StatusCode* out);  // 0x528720
    ObjectKey* GetSeedKey(ObjectKey* out);  // 0x527c00
    // The system this manager works in (the parent), null if detached.
    GameObjNotifier* GetHostSystem();  // 0x52bed0
    ObjectKey* GetDeploymentKey(ObjectKey* out);  // 0x4591b0, copies m_deploymentKey
    // The product being built (looked up from ProductKey), null if none.
    GameObjNotifier* GetProduct();  // 0x52be30 (0x52bec0 is a jump to it)
    // A facility serves this manager if it builds the same kind of product, is usable and is on the same side.
    int IsCompatibleFacility(GameObj* facility);  // 0x52bde0
    int IsAcceptableProduct(GameObj* product);   // 0x528890: created, not completed/destroyed/enroute, built by this
    ObjectKeyValueSet* GetQueue();               // 0x528820: the data block's queue (master), else a static empty set
    int GetQueueCopy(ObjectKeyValueSet* out);    // 0x5287f0
    int IsQueued(const ObjectKey* key);          // 0x528930
    int IsQueueLengthConsistent();               // 0x5287d0
    int CountHostFacilities(int includeOutOfService);  // 0x528040
    int HasHostFacility(int includeOutOfService);      // 0x528100
    static int EnsureEventKinds();                              // 0x52ae30
    static int InitEventKinds(int unused, EventKindSet* out);   // 0x52aeb0
    static void ConstructEventKinds();                          // 0x52c090
    static void RegisterEventKindsAtExit();                     // 0x52c0a0
    static void StaticInit();                                   // 0x52c080

    // ==== SECTION setters (manu_mgr_setters.cpp) ====
    int SetRemainingGameObjCount(int value, NotifyContext* ctx);  // 0x529480
    int SetCompletedPointCount(int value, NotifyContext* ctx);    // 0x5294e0
    int SetOverflowPointCount(int value, NotifyContext* ctx);     // 0x529540
    int SetSeedKey(NullableKey* value, NotifyContext* ctx);       // 0x5295a0
    int SetRequiredPointCount(int value, NotifyContext* ctx);     // 0x529650
    int SetTotalRequiredPointCount(int value, NotifyContext* ctx);  // 0x5296b0
    int SetReserved(int value, NotifyContext* ctx);               // 0x529710
    int SetProductKey(ObjectKey* value, NotifyContext* ctx);      // 0x529770
    int SetDeploymentKey(ObjectKey* value, NotifyContext* ctx);   // 0x529820
    int SetTargetKey(ObjectKey* value, NotifyContext* ctx);       // 0x5298d0
    int SetProductName(String* value, NotifyContext* ctx);        // 0x529980

    // ==== SECTION apply (manu_mgr_apply.cpp) ====
    int ApplyRemainingGameObjCount(NotifyContext* ctx);  // 0x52aed0
    int ApplyCompletedPointCount(NotifyContext* ctx);    // 0x52af50
    int ApplyOverflowPointCount(NotifyContext* ctx);     // 0x52afd0
    int ApplySeedKey(NotifyContext* ctx);                // 0x52b050
    int ApplyRequiredPointCount(NotifyContext* ctx);     // 0x52b170
    int ApplyTotalRequiredPointCount(NotifyContext* ctx);  // 0x52b1f0
    int ApplyReserved(NotifyContext* ctx);               // 0x52b270
    int ApplyProductKey(NotifyContext* ctx);             // 0x52b2f0
    int ApplyDeploymentKey(NotifyContext* ctx);          // 0x52b410
    int ApplyTargetKey(NotifyContext* ctx);              // 0x52b530
    int ApplyProductName(NotifyContext* ctx);            // 0x52b650
    String* GetProductNameCopy(String* out);             // 0x52b760

    // ==== SECTION production (manu_mgr_production.cpp) ====
    int GetProductionRate(StatusCode* status, int* rate);  // 0x528960
    // Points the facilities deliver in `time` turns (adjusted by the work already under way when includeProgress).
    int GetPointsForTime(int time, int includeProgress, StatusCode* status, int* points);  // 0x528b30
    int EstimateBuildTime(const NullableKey* seed, int count, int subtractProgress, StatusCode* status, int* turns);  // 0x528d30
    int EstimateBuildTimeFresh(const NullableKey* seed, int count, StatusCode* status, int* turns);  // 0x529090
    int EstimateQueuedBuildTime(const ObjectKey* queued, StatusCode* status, int* turns);  // 0x529120
    int EstimateLastQueuedBuildTime(StatusCode* status, int* turns);  // 0x529360
    int OnProductChanged(const ObjectKey* key, int a, int b, NotifyContext* ctx);  // 0x529d20
    int MoveOverflowToCompleted(const ObjectKey* key, int unused, NotifyContext* ctx);  // 0x529d70
    int HandleProductEvent(const ObjectKey* key, int a, int b, int c, NotifyContext* ctx);  // 0x529cf0
    int RebalanceFacilities(NotifyContext* ctx);  // 0x529dd0
    int DestroyProducts(NotifyContext* ctx);     // 0x52a2c0
    int CreateProducts(ObjectKeyValueSet* products, int unused, NotifyContext* ctx);  // 0x52a140
    int RenameProductIfAny(String* name, NotifyContext* ctx);  // 0x52a240
    int ClearReserved(int value, NotifyContext* ctx);          // 0x52a360
    int ReparentQueue(ObjectKeyValueSet* products, const ObjectKey* target, NotifyContext* ctx);  // 0x52a380
    int AdvanceProgress(NotifyContext* ctx);                   // 0x52a430
    int UpdateQueueMembership(const ObjectKey* key, NotifyContext* ctx);  // 0x52a710
    int UpdateRemainingCount(NotifyContext* ctx);              // 0x52a7d0
    int UpdateSeedKey(NotifyContext* ctx);                     // 0x52a800
    int UpdateRequiredFromSeed(NotifyContext* ctx);            // 0x52a8f0
    int UpdateTotalRequired(NotifyContext* ctx);               // 0x52a9c0
    int ChooseProduct(NotifyContext* ctx);                     // 0x52aac0
    int UpdateDeploymentKey(NotifyContext* ctx);               // 0x52ac40
    int UpdateProductName(NotifyContext* ctx);                 // 0x52ad30
    int CompleteProduct(GameObj* product, NotifyContext* ctx);  // 0x52bee0

    // ---- fields ----
    int m_remainingGameObjCount;      // +0x58
    int m_completedPointCount;        // +0x5c
    int m_overflowPointCount;         // +0x60
    NullableKey m_seedKey;            // +0x64
    int m_requiredPointCount;         // +0x68
    int m_totalRequiredPointCount;    // +0x6c
    int m_reserved;                   // +0x70
    ObjectKey m_productKey;           // +0x74
    ObjectKey m_deploymentKey;        // +0x78
    ObjectKey m_targetKey;            // +0x7c
    String m_productName;             // +0x80
};

// Runs UpdateChildren over the manufacturing manager families (0xa0-0xb0) of `parent` (0x529a40, cdecl).
int UpdateManuMgrChildren(GameObj* parent, NotifyContext* ctx);

// Secondary-vtable WriteName (ObjectBase slot 0, 0x527b40): kind 5 appends the seed record's name, 6 the queue length.
void REB_THISCALL AppendManuMgrName(ObjectBase* base, String* out, int kind);

// Iterator over the manufacturing managers (families 0xa0-0xaf) below an object; VT 0x65fe54 (destructor only).
class ManuMgrIterator : public ChildIterator {
public:
    ManuMgrIterator(GameObjNotifier* parent);                          // 0x52c0d0
    ManuMgrIterator(GameObjNotifier* parent, uint32_t stateFilter);    // 0x52c170
    virtual ~ManuMgrIterator();                                        // 0x52c150 (deleting), 0x52c1f0
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ManuMgrData, 0x20);
REB_ASSERT_SIZE(ManuMgr, 0x8c);
REB_ASSERT_OFFSET(ManuMgr, m_remainingGameObjCount, 0x58);
REB_ASSERT_OFFSET(ManuMgr, m_seedKey, 0x64);
REB_ASSERT_OFFSET(ManuMgr, m_productKey, 0x74);
REB_ASSERT_OFFSET(ManuMgr, m_productName, 0x80);
REB_ASSERT_SIZE(ManuMgrIterator, 0x20);
#pragma clang diagnostic pop
