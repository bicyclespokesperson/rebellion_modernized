#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "economy/production_facility.h"
#include "gameobj/game_obj.h"
#include "gameobj/key_queue.h"
#include "gameobj/object_key.h"
#include "gameobj/tracked_stat.h"

// ObjectNameList (VT 0x660ae8, 0x10 bytes): an IdAvlTree of ObjectNameNodes with the same "last operation worked" flag
// as ObjectRefList. The object-type database builds one for its record queries. (The link order put its code
// in front of the Side class, 0x52d720-0x52d760.)
class ObjectNameList : public IdAvlTree {
public:
    ObjectNameList();           // 0x52d720
    ~ObjectNameList() override;  // 0x52d760 (scalar deleting destructor 0x52d740)

    int m_ok;  // +0x0c
};

class System;
class ManuMgr;

// Notification event ids the Side raises (FUN_0053fa60 / 0x53fcf0 / 0x53fd60 / 0x53fdd0).
namespace side_event {
constexpr int kMaintState = 0x600;
constexpr int kMaintRequired = 0x120;
constexpr int kRawMaterials = 0x121;
constexpr int kRefinedMaterials = 0x122;
constexpr int kWaitingRefineries = 0x123;
constexpr int kWaitingManufacturers = 0x124;
constexpr int kShipyardRdOrder = 0x125;
constexpr int kTrainingFacilRdOrder = 0x126;
constexpr int kConstructionYardRdOrder = 0x127;
constexpr int kVictoryConditions = 0x128;
constexpr int kShipyardRdDone = 0x129;
constexpr int kTrainingFacilRdDone = 0x12a;
constexpr int kConstructionYardRdDone = 0x12b;
constexpr int kRecruitmentDone = 0x12c;
constexpr int kMaintDeficit = 0x382;
constexpr int kTrackedStatReset = 0x381;
}  // namespace side_event

// Research/manufacturing "kind" indices used by the Side's accessors: 0 Shipyard, 1 ConstructionYard, 2 TrainingFacil
// (the same order as the ManuMgr slots and the characters' RD skills).
namespace rd_kind {
constexpr int kShipyard = 0;
constexpr int kConstructionYard = 1;
constexpr int kTrainingFacil = 2;
}  // namespace rd_kind

// Side (family 0xf3, ctor 0x52d7b0, VT 0x660af8, secondary ObjectBase VT 0x660af4, 0xdc bytes): the per-player economy
// object. Master view: the data the game logic changes; the Alliance's and the Empire's views mirror it through
// the usual SetX -> ApplyX -> OnXChanged / OnXNotified scheme (see game_obj.h). The master exists once per side
// (ids 0xf and 0x10; the id is set when the views are created, 0x530d70).
//
// It derives from GameObj and adds 21 properties, in vtable order (OnXChanged slots 111-131, OnXNotified 132-152):
//
//   #  field                       Set / Apply         notes
//   0  m_maintState      +0x58 pair  0x52ec90 / 0x5315e0   "MaintState (Capacity/Allocated)", event 0x600
//   1  m_mineState       +0x60 pair  0x52ed50 / 0x531710   sum of the usable family-0x2c facilities' pairs (mines)
//   2  m_refineryState   +0x68 pair  0x52ee10 / 0x531840   same for family 0x2d (refineries)
//   3  m_maintRequired   +0x70       0x52eed0 / 0x531970   "Maint Required": sum of the units' record field 0x4c
//   4  m_maintRequiredB  +0x74       0x52ef30 / 0x5319f0   always equal to the one above (checked by Validate)
//   5  m_rawMaterials    +0x78       0x52ef90 / 0x531a70   stock handed out to refineries (queue A waits for it)
//   6  m_refinedMaterials +0x7c      0x52eff0 / 0x531af0   stock handed out to manufacturing facilities (queue B)
//   7  m_waitingRefineries +0x80     0x52f050 / 0x531b70   length of queue A
//   8  m_waitingManufacturers +0x84  0x52f0c0 / 0x531c00   length of queue B
//   9  m_shipyardRdProgress +0x90    0x52f130 / 0x531c90   accumulated research points per kind (kind 0)
//  10  m_trainingFacilRdProgress +0x94  0x52f1a0 / 0x531d20   (kind 2)
//  11  m_constructionYardRdProgress +0x98  0x52f210 / 0x531db0   (kind 1)
//  12  m_shipyardRdOrder +0x9c       0x52f280 / 0x531e40   the current research level per kind
//  13  m_trainingFacilRdOrder +0xa0  0x52f2f0 / 0x531ed0
//  14  m_constructionYardRdOrder +0xa4  0x52f360 / 0x531f60
//  15  m_maintDeficit   +0xa8       0x52f3d0 / 0x531ff0   1 when the maintenance need exceeds the capacity
//  16-18 RdDone flags +0xac/+0xb0/+0xb4 (shipyard, training facility, construction yard)  0x52f440.. / 0x532080..
//  19  m_recruitmentDone +0xb8      0x52f590 / 0x532230
//  20  m_victoryConditions +0xbc    0x52f600 / 0x5322c0   bit set, formatted by FormatVictoryConditions
//
// Not properties: m_rdScalePercent +0xc0 (100), m_unkC4 +0xc4 (1), the two key queues +0x88/+0x8c, the object reference
// list +0xc8 and two tracked stats +0xcc/+0xd4. The queues hold the process facilities (families 0x2d / 0x28-0x2b)
// waiting for materials. (The refinery/mine/raw/refined readings are provisional: they follow the data flow.)
class Side : public GameObj {
public:
    Side();           // 0x52d7b0
    ~Side() override;  // 0x52d880 (scalar deleting destructor 0x52d860)

    // ---- overrides of GameObj / GameObjRoot slots ----
    uint32_t GetFamily() override;                         // 1, 0x52e4e0 (0xf3)
    void WriteName(std::string* out, int kind) override;       // ObjectBase slot 0, 0x52d920 (AppendSideName)
    int Read(istream* in) override;                        // 5, 0x530d10
    int Write(ostream* out) override;                      // 6, 0x530d20
    int Checksum(Crc32* crc) override;                     // 7, 0x530d50
    int Validate() override;                               // 8, 0x530d60 (jump to 0x531520)
    int ApplyAll(NotifyContext* ctx) override;             // 9, 0x530d30
    int Virtual16() override { return 1; }                 // 16, 0x40f340 (shared)
    int CreateViews(int controlKind, NotifyContext* ctx) override;  // 37, 0x530cf0
    GameObj* CreateViewObject() override;                  // 57, 0x531570
    int OnExistingChanged(int newValue, NotifyContext* ctx) override;  // 74, 0x532350

    // ---- slots 111-131: OnXChanged on the master (arguments: old, new, ctx) ----
    virtual int OnMaintStateChanged(const IntPair* oldValue, const IntPair* newValue, NotifyContext* ctx);  // 111, 0x532680
    virtual int OnMineStateChanged(const IntPair* oldValue, const IntPair* newValue, NotifyContext* ctx);   // 112, 0x5323c0
    virtual int OnRefineryStateChanged(const IntPair* oldValue, const IntPair* newValue, NotifyContext* ctx);  // 113, 0x532520
    virtual int OnMaintRequiredChanged(int oldValue, int newValue, NotifyContext* ctx);        // 114, 0x532680
    virtual int OnMaintRequiredBChanged(int oldValue, int newValue, NotifyContext* ctx);       // 115, 0x532680
    virtual int OnRawMaterialsChanged(int oldValue, int newValue, NotifyContext* ctx);         // 116, 0x532690
    virtual int OnRefinedMaterialsChanged(int oldValue, int newValue, NotifyContext* ctx);     // 117, 0x5326c0
    virtual int OnWaitingRefineriesChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }     // 118, 0x524b10
    virtual int OnWaitingManufacturersChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 119
    virtual int OnShipyardRdProgressChanged(int oldValue, int newValue, NotifyContext* ctx);       // 120, 0x5326f0
    virtual int OnTrainingFacilRdProgressChanged(int oldValue, int newValue, NotifyContext* ctx);  // 121, 0x5327f0
    virtual int OnConstructionYardRdProgressChanged(int oldValue, int newValue, NotifyContext* ctx);  // 122, 0x5328f0
    virtual int OnShipyardRdOrderChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }       // 123
    virtual int OnTrainingFacilRdOrderChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 124
    virtual int OnConstructionYardRdOrderChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 125
    virtual int OnMaintDeficitChanged(int oldValue, int newValue, NotifyContext* ctx);         // 126, 0x5329f0
    virtual int OnShipyardRdDoneChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }         // 127
    virtual int OnTrainingFacilRdDoneChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }    // 128
    virtual int OnConstructionYardRdDoneChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 129
    virtual int OnRecruitmentDoneChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }        // 130
    virtual int OnVictoryConditionsChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }      // 131

    // ---- slots 132-152: OnXNotified on a side view ----
    virtual int OnMaintStateNotified(const IntPair* oldValue, const IntPair* newValue, NotifyContext* ctx);  // 132, 0x532a20
    virtual int OnMineStateNotified(const IntPair* oldValue, const IntPair* newValue, NotifyContext* ctx) { return IsSideView(); }      // 133, 0x55b1f0
    virtual int OnRefineryStateNotified(const IntPair* oldValue, const IntPair* newValue, NotifyContext* ctx) { return IsSideView(); }  // 134
    virtual int OnMaintRequiredNotified(int oldValue, int newValue, NotifyContext* ctx);       // 135, 0x532c50
    virtual int OnMaintRequiredBNotified(int oldValue, int newValue, NotifyContext* ctx) { return IsSideView(); }  // 136
    virtual int OnRawMaterialsNotified(int oldValue, int newValue, NotifyContext* ctx);        // 137, 0x532ca0
    virtual int OnRefinedMaterialsNotified(int oldValue, int newValue, NotifyContext* ctx);    // 138, 0x532cd0
    virtual int OnWaitingRefineriesNotified(int oldValue, int newValue, NotifyContext* ctx);   // 139, 0x532d00
    virtual int OnWaitingManufacturersNotified(int oldValue, int newValue, NotifyContext* ctx);  // 140, 0x532d30
    virtual int OnShipyardRdProgressNotified(int oldValue, int newValue, NotifyContext* ctx) { return IsSideView(); }       // 141
    virtual int OnTrainingFacilRdProgressNotified(int oldValue, int newValue, NotifyContext* ctx) { return IsSideView(); }  // 142
    virtual int OnConstructionYardRdProgressNotified(int oldValue, int newValue, NotifyContext* ctx) { return IsSideView(); }  // 143
    virtual int OnShipyardRdOrderNotified(int oldValue, int newValue, NotifyContext* ctx);      // 144, 0x532d60
    virtual int OnTrainingFacilRdOrderNotified(int oldValue, int newValue, NotifyContext* ctx);  // 145, 0x532db0
    virtual int OnConstructionYardRdOrderNotified(int oldValue, int newValue, NotifyContext* ctx);  // 146, 0x532e00
    virtual int OnMaintDeficitNotified(int oldValue, int newValue, NotifyContext* ctx) { return IsSideView(); }  // 147
    virtual int OnShipyardRdDoneNotified(int oldValue, int newValue, NotifyContext* ctx);       // 148, 0x532e50
    virtual int OnTrainingFacilRdDoneNotified(int oldValue, int newValue, NotifyContext* ctx);  // 149, 0x532ea0
    virtual int OnConstructionYardRdDoneNotified(int oldValue, int newValue, NotifyContext* ctx);  // 150, 0x532ef0
    virtual int OnRecruitmentDoneNotified(int oldValue, int newValue, NotifyContext* ctx);      // 151, 0x532f40
    virtual int OnVictoryConditionsNotified(int oldValue, int newValue, NotifyContext* ctx);    // 152, 0x532f90

    // ---- persistence and validation bodies behind slots 5-9, 37 ----
    int ReadImpl(istream* in);                                  // 0x530dd0
    int WriteImpl(ostream* out);                                // 0x531020
    int ChecksumImpl(Crc32* crc);                               // 0x531330
    int ValidateImpl();                                         // 0x531520
    int ApplyAllImpl(NotifyContext* ctx);                       // 0x531230
    int CreateViewsImpl(int controlKind, NotifyContext* ctx);   // 0x530d70

    // ---- the containers (queues, reference list) ----
    int InitState();  // 0x533300: resets every field and allocates the queues and the reference list
    int FreeState();  // 0x5332a0: always 1
    int HasContainers() const;  // 0x533080

    // ---- consistency checks used by Validate ----
    int CountQueuedA(KeyQueueNode* probe);  // 0x52e4f0, 0 without queue
    int CountQueuedB(KeyQueueNode* probe);  // 0x52e510
    int MineStateMatches();                 // 0x52e640: m_mineState equals the sum over usable mines
    int RefineryStateMatches();             // 0x52e760
    int MaintRequiredMatches();             // 0x52e860: m_maintRequired equals the units' summed field 0x4c
    int MaintRequiredBMatches();            // 0x52e920
    int QueuesMatchFacilities();            // 0x5330b0: queue A/B hold exactly the waiting facilities

    // ---- queries ----
    // *out = 1 for a completed, existing unit of this side with a maintenance cost (a scrap candidate); always 1.
    int IsScrapCandidate(GameObj* unit, int* out);  // 0x52e530
    // Collects the research records of family `kind` (0x28-0x2a) at this side's current level into `out`.
    int CollectRdRecords(int kind, IdAvlTree* out);  // 0x52e580
    // Whether `count` more units of record `key` fit into the maintenance capacity; *status becomes {0xf3,1} if not.
    int CheckMaintCapacity(const ObjectKey* key, int count, StatusCode* status);  // 0x52e970
    int GetRdProgress(int kind, int* out);  // 0x52ea70 (kind 0 shipyard, 1 construction yard, 2 training facility)
    int GetRdOrder(int kind, int* out);     // 0x52eac0
    int GetRdDone(int kind, int* out);      // 0x52eb10

    // Copies of the pair properties (the galaxy view's top bar reads them).
    IntPair* GetMineStateCopy(IntPair* out);      // 0x4292e0
    IntPair* GetRefineryStateCopy(IntPair* out);  // 0x429300

    // ---- setters (master view only) ----
    int SetMaintState(const IntPair* value, NotifyContext* ctx);  // 0x52ec90
    int SetMineState(const IntPair* value, NotifyContext* ctx);   // 0x52ed50
    int SetRefineryState(const IntPair* value, NotifyContext* ctx);  // 0x52ee10
    int SetMaintRequired(int value, NotifyContext* ctx);      // 0x52eed0
    int SetMaintRequiredB(int value, NotifyContext* ctx);     // 0x52ef30
    int SetRawMaterials(int value, NotifyContext* ctx);       // 0x52ef90
    int SetRefinedMaterials(int value, NotifyContext* ctx);   // 0x52eff0
    int SetWaitingRefineries(int value, NotifyContext* ctx);  // 0x52f050
    int SetWaitingManufacturers(int value, NotifyContext* ctx);  // 0x52f0c0
    int SetShipyardRdProgress(int value, NotifyContext* ctx);       // 0x52f130
    int SetTrainingFacilRdProgress(int value, NotifyContext* ctx);  // 0x52f1a0
    int SetConstructionYardRdProgress(int value, NotifyContext* ctx);  // 0x52f210
    int SetShipyardRdOrder(int value, NotifyContext* ctx);       // 0x52f280
    int SetTrainingFacilRdOrder(int value, NotifyContext* ctx);  // 0x52f2f0
    int SetConstructionYardRdOrder(int value, NotifyContext* ctx);  // 0x52f360
    int SetMaintDeficit(int value, NotifyContext* ctx);          // 0x52f3d0
    int SetShipyardRdDone(int value, NotifyContext* ctx);        // 0x52f440
    int SetTrainingFacilRdDone(int value, NotifyContext* ctx);   // 0x52f4b0
    int SetConstructionYardRdDone(int value, NotifyContext* ctx);  // 0x52f520
    int SetRecruitmentDone(int value, NotifyContext* ctx);       // 0x52f590
    int SetVictoryConditions(int value, NotifyContext* ctx);     // 0x52f600
    // Sets the progress of research kind 0/1/2 (shipyard / construction yard / training facility); 0 for any other kind.
    int SetRdProgress(int kind, int value, NotifyContext* ctx);  // 0x530600
    // Both return whether the value was accepted.
    int SetRdScalePercent(int percent);  // 0x5305c0, ignored unless positive
    int SetUnknownC4(int value);         // 0x5305e0, ignored when zero

    // ---- applying the master's values to a side view ----
    int ApplyMaintState(NotifyContext* ctx);  // 0x5315e0
    int ApplyMineState(NotifyContext* ctx);   // 0x531710
    int ApplyRefineryState(NotifyContext* ctx);  // 0x531840
    int ApplyMaintRequired(NotifyContext* ctx);   // 0x531970
    int ApplyMaintRequiredB(NotifyContext* ctx);  // 0x5319f0
    int ApplyRawMaterials(NotifyContext* ctx);    // 0x531a70
    int ApplyRefinedMaterials(NotifyContext* ctx);  // 0x531af0
    int ApplyWaitingRefineries(NotifyContext* ctx);  // 0x531b70
    int ApplyWaitingManufacturers(NotifyContext* ctx);  // 0x531c00
    int ApplyShipyardRdProgress(NotifyContext* ctx);       // 0x531c90
    int ApplyTrainingFacilRdProgress(NotifyContext* ctx);  // 0x531d20
    int ApplyConstructionYardRdProgress(NotifyContext* ctx);  // 0x531db0
    int ApplyShipyardRdOrder(NotifyContext* ctx);       // 0x531e40
    int ApplyTrainingFacilRdOrder(NotifyContext* ctx);  // 0x531ed0
    int ApplyConstructionYardRdOrder(NotifyContext* ctx);  // 0x531f60
    int ApplyMaintDeficit(NotifyContext* ctx);          // 0x531ff0
    int ApplyShipyardRdDone(NotifyContext* ctx);        // 0x532080
    int ApplyTrainingFacilRdDone(NotifyContext* ctx);   // 0x532110
    int ApplyConstructionYardRdDone(NotifyContext* ctx);  // 0x5321a0
    int ApplyRecruitmentDone(NotifyContext* ctx);       // 0x532230
    int ApplyVictoryConditions(NotifyContext* ctx);     // 0x5322c0

    // ---- economy logic (master) ----
    // MaintDeficit = 1 unless both required values fit into the maintenance capacity.
    int UpdateMaintDeficit(NotifyContext* ctx);  // 0x52f670
    // Moves the mines'/refineries' allocation towards the required maintenance (the facilities adjust their pairs).
    int AllocateMines(NotifyContext* ctx);       // 0x52f6b0
    int AllocateRefineries(NotifyContext* ctx);  // 0x52f8f0
    // Takes one raw/refined unit if one is available (*took), returns the setter's result; always 1 before game setup.
    int TakeRawMaterial(int* took, NotifyContext* ctx);      // 0x52fb30
    int TakeRefinedMaterial(int* took, NotifyContext* ctx);  // 0x52fb80
    // Return the setter's result when the node was queued, else 0.
    int EnqueueRefinery(KeyQueueNode* node, NotifyContext* ctx);      // 0x52fbb0
    int EnqueueManufacturer(KeyQueueNode* node, NotifyContext* ctx);  // 0x52fbd0
    // Dequeues the oldest waiting facility and starts it if it is still current; *more says whether to go on.
    int ServeRefineryQueue(int* more, NotifyContext* ctx);      // 0x52fbf0
    int ServeManufacturerQueue(int* more, NotifyContext* ctx);  // 0x52fd30
    int ScheduleRefineryEvent();      // 0x52fe50
    int ScheduleManufacturerEvent();  // 0x52ff20
    // Recomputes mine/refinery pairs and the maintenance requirement from the units.
    int RecomputeMaintenance(NotifyContext* ctx);  // 0x52fff0
    // A unit scrapped for reason 0x14-0x16 refunds half its record field 0x48 as refined materials.
    int RefundScrappedUnit(GameObj* unit, NotifyContext* ctx);  // 0x530270
    // Counts a reference to `key` (adds it to the list if new); *count receives the new count. 0 without lists.
    int AddObjectRef(const ObjectKey* key, int* count, NotifyContext* unused);  // 0x5302c0
    // While maintenance exceeds capacity, picks a random candidate unit and requests its scrapping.
    int ScrapRandomUnit(NotifyContext* ctx);  // 0x530350
    // Adds the facilities' per-kind counts to the research progress counters.
    int UpdateRdProgress(NotifyContext* ctx);  // 0x530460
    // The manufacturing manager (in the facility's system) responsible for a manufacturing facility (families
    // 0x28-0x2b); *out is null if there is none. Does not use `this`.
    int FindFacilityManuMgr(GameObj* facility, ManuMgr** out);  // 0x52eb60
    // Clears a releasable facility's waiting flag and lets its manager continue.
    int ReleaseFacilityToManager(ProductionFacility* facility, NotifyContext* ctx);  // 0x530950, the result of the last step
    int ReleaseRawMaterial(ProductionFacility* facility, NotifyContext* ctx);       // 0x530670
    int AcquireRawMaterial(ProductionFacility* facility, NotifyContext* ctx);        // 0x5306b0
    int ReleaseRefinedMaterial(ProductionFacility* facility, NotifyContext* ctx);   // 0x5307e0
    int AcquireRefinedMaterial(ProductionFacility* facility, NotifyContext* ctx);    // 0x530820
    // Pushes a queue entry (key and the facility's request number) / pops the oldest.
    KeyQueueNode* PopRefineryQueue(NotifyContext* ctx);      // 0x5335c0
    KeyQueueNode* PopManufacturerQueue(NotifyContext* ctx);  // 0x533660
    int PushRefineryQueue(KeyQueueNode* node, NotifyContext* ctx);      // 0x533570
    int PushManufacturerQueue(KeyQueueNode* node, NotifyContext* ctx);  // 0x533610

    // Fills the class's static event kind set once is not needed: the Side uses GameObj's.
    // ---- fields ----
    IntPair m_maintState;           // +0x58 {capacity, allocated}
    IntPair m_mineState;            // +0x60
    IntPair m_refineryState;        // +0x68
    int m_maintRequired;            // +0x70
    int m_maintRequiredB;           // +0x74
    int m_rawMaterials;             // +0x78
    int m_refinedMaterials;         // +0x7c
    int m_waitingRefineries;        // +0x80
    int m_waitingManufacturers;     // +0x84
    KeyQueue* m_refineryQueue;      // +0x88
    KeyQueue* m_manufacturerQueue;  // +0x8c
    int m_shipyardRdProgress;       // +0x90
    int m_trainingFacilRdProgress;  // +0x94
    int m_constructionYardRdProgress;  // +0x98
    int m_shipyardRdOrder;          // +0x9c
    int m_trainingFacilRdOrder;     // +0xa0
    int m_constructionYardRdOrder;  // +0xa4
    int m_maintDeficit;             // +0xa8
    int m_shipyardRdDone;           // +0xac
    int m_trainingFacilRdDone;      // +0xb0
    int m_constructionYardRdDone;   // +0xb4
    int m_recruitmentDone;          // +0xb8
    int m_victoryConditions;        // +0xbc
    int m_rdScalePercent;           // +0xc0
    int m_unkC4;                    // +0xc4
    ObjectRefList* m_objectRefs;    // +0xc8
    TrackedStat m_statCc;           // +0xcc
    TrackedStat m_statD4;           // +0xd4
};

// The factory the family registry holds for 0xf3 (0x5309b0).
GameObjNotifier* CreateSide();
// Maps a record key's id (0xf Alliance, 0x10 Empire) to the side number 1/2, 0 for anything else (0x530a20, cdecl).
int SideNumberFromKey(const ObjectKey* key);
// "AD LC MC ..." style listing of the victory condition bits into *out (0x530a50, cdecl); always 1.
int FormatVictoryConditions(uint32_t conditions, std::string* out);
// The secondary vtable's WriteName for the Side (0x52d920; `base` is the ObjectBase subobject).
void AppendSideName(ObjectBase* base, std::string* out, int kind);
// Helpers of the process facilities that live in this range (0x52e740, 0x52e8b0, 0x530650).
GameObjNotifier* FindNextUnit(GameObjNotifier* unit);
int MarkFacilityWaiting(ProductionFacility* facility, NotifyContext* ctx);
