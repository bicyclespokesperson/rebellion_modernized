#pragma once
#include <cstdint>

#include "galaxy/system_neighbours.h"
#include "gameobj/game_obj.h"
#include "gameobj/tracked_stat.h"
#include "orders/route_context.h"

class DatRecord;
class EventKindSet;
class SystemNeighbourTable;

// The rules manager (family 0xf9, key 0xf9000007; VT 0x662158, secondary ObjectBase VT 0x662154 at +0x30; 0x6c bytes):
// the third of the three singleton managers below the game root (mission manager f8, rules manager f9, character
// manager fa). It decides where things go: the capacity checks of moves, the days a trip takes, where a unit that
// loses its container is relocated to, and the random system incidents its two timers (events 0x38f / 0x390) cause.
// It owns the table of every system's neighbours by distance (+0x68, built at game setup by BuildNeighbourTable) that
// the relocation search walks.
//
// Nearly every member below is called with the manager in ECX; a few do not use it. The names of the ones that are only
// known by what they do are descriptive.
class RulesMgr : public GameObj {
public:
    RulesMgr();           // 0x554020
    virtual ~RulesMgr();  // 0x5540c0 (scalar deleting destructor 0x5540a0)

    virtual uint32_t GetFamily() override;                          // 1: 0x556f00, 0xf9
    virtual int Read(istream* in) override;                         // 5: 0x556eb0 -> ReadBody
    virtual int Write(ostream* out) override;                       // 6: 0x556ec0 -> WriteBody
    virtual int Checksum(Crc32* crc) override;                      // 7: 0x556ef0 -> ChecksumBody
    virtual int ApplyAll(NotifyContext* ctx) override;              // 9: 0x556ed0 -> ApplyAllBody
    virtual int Validate() override;                                // 8: shared stub 0x569870
    virtual int GetEventKinds(int unused, EventKindSet* out) override;  // 15: 0x556200
    virtual int CheckOrder(int a, int b, StatusCode* out) override;     // 22: 0x556220
    virtual int UpdateObservedByAlliance(NotifyContext* ctx) override;  // 35: 0x556f10
    virtual int UpdateObservedByEmpire(NotifyContext* ctx) override;    // 36: 0x556f20
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;  // 37: 0x556e90 -> CreateViewsBody
    virtual GameObj* CreateViewObject() override;                       // 57: 0x556e20

    // ---- bodies behind the virtual thunks ----
    int ReadBody(istream* in);               // 0x5570c0
    int WriteBody(ostream* out);             // 0x5570f0
    int ChecksumBody(Crc32* crc);            // 0x557150
    int ApplyAllBody(NotifyContext* ctx);    // 0x557120
    int CreateViewsBody(int controlKind, NotifyContext* ctx);  // 0x556fa0, arms the two timers

    // Replaces the neighbour table with a freshly built one. Returns whether it was built.
    int BuildNeighbourTable(NotifyContext* ctx);  // 0x5562e0 (the context is not used)
    // Points `out` at the neighbours of the system `system` names; 0 without a table.
    int GetNeighbours(const ObjectKey* system, SystemNeighbourCursor* out);  // 0x554150

    // ---- capacity checks of moves ----
    // Whether `unit` still fits in its own parent (ownership of the legs inverted).
    int CheckParentCapacity(GameObj* unit, StatusCode* status);  // 0x554170
    // Whether `unit` can move to `destination` as an order's request asks / by itself / inside an existing route.
    int CheckRequestMove(GameObj* unit, const ObjectKey* destination, ActivityRequest* request,
                         StatusCode* status);  // 0x5542f0
    int CheckUnitMove(GameObj* unit, const ObjectKey* destination, StatusCode* status);  // 0x5543c0
    int CheckMoveInRoute(GameObj* unit, const ObjectKey* destination, RouteContext* route,
                         StatusCode* status);  // 0x554490

    // ---- relocation ----
    // Looks for a place for `unit` that lost its container: first its ancestors (if `tryAncestors`), then the ancestors
    // of `target`, then the friendly systems around `home`, their fleets, the blockaded ones, the neutral systems and
    // finally the enemy ones. Stops at the first that fits (status {1,-1}); `out` receives the tightest key.
    int FindDestination(GameObj* unit, const ObjectKey* home, const ObjectKey* target, int flag, int tryAncestors,
                        ObjectKey* out, StatusCode* status);  // 0x5545e0
    int TryAncestorsOfUnit(GameObj* unit, int flag, ObjectKey* out, StatusCode* status);                       // 0x554740
    int TryAncestorsOfTarget(GameObj* unit, const ObjectKey* target, ObjectKey* out, StatusCode* status);      // 0x554ab0
    int TryFriendlySystems(GameObj* unit, const ObjectKey* home, ObjectKey* out, StatusCode* status);          // 0x554e80
    int TryFriendlyFleets(GameObj* unit, const ObjectKey* home, ObjectKey* out, StatusCode* status);           // 0x554f80
    int TryBlockadedSystems(GameObj* unit, const ObjectKey* home, ObjectKey* out, StatusCode* status);         // 0x5550e0
    int TryNeutralSystems(GameObj* unit, const ObjectKey* home, ObjectKey* out, StatusCode* status);           // 0x5551e0
    int TryEnemySystems(GameObj* unit, const ObjectKey* home, ObjectKey* out, StatusCode* status);             // 0x5552e0
    // Whether `unit` fits into `candidate` (and, unless `ignoreParent`, once it has left its parent); *tightest gets
    // the key of the tightest leg. status becomes {1,-1} or {1,1}.
    int TryPlaceUnit(GameObj* unit, GameObjNotifier* candidate, int ignoreParent, ObjectKey* tightest,
                     StatusCode* status);  // 0x5571c0

    // ---- owners and locations ----
    // *same = whether the two objects' owner objects have the same key (1 if either has none). Returns the second
    // look-up's success.
    int AreInSameOwner(GameObjNotifier* a, GameObjNotifier* b, uint32_t* same);      // 0x555460
    int AreKeysInSameOwner(const ObjectKey* a, const ObjectKey* b, uint32_t* same);  // 0x555410
    // *out = the object the owner key of `object` names (null if unset); same body as ::GetOwnerObject.
    int GetOwnerObject(GameObjNotifier* object, GameObjNotifier** out);  // 0x555540

    // ---- travel ----
    // Days of a trip between the two locations, as seen by view side `view`, at `speed` percent: the map distance's
    // days between two systems, 1 between two owners that are not, 0 within one owner. Returns whether both
    // locations exist.
    int ComputeTravelDays(int speed, int view, const ObjectKey* from, const ObjectKey* to, int* days);  // 0x555b30
    // The trip of `unit` (a unit with a speed of its own, else the default speed) between two locations.
    int ComputeUnitTravelDays(GameObj* unit, const ObjectKey* from, const ObjectKey* to, int* days);  // 0x555d30
    int ComputeRecordTravelDays(DatRecord* record, uint32_t viewSide, const ObjectKey* from, const ObjectKey* to,
                                int* days);  // 0x555d90
    // *out = today + the build time of the last product queued at `manager`.
    int QueryLastBuildDay(GameObjNotifier* manager, StatusCode* status, int* out);  // 0x555e60
    // Days a new item takes at `manager` (ManuMgr::EstimateBuildTimeFresh); the status starts at {1,-1}.
    int EstimateBuildDays(GameObjNotifier* manager, const NullableKey* item, int count, StatusCode* status,
                          int* days);  // 0x555f00
    // Days an item takes from a manager to its deployment target.
    int EstimateDeploymentDays(GameObjNotifier* manager, const ObjectKey* item, StatusCode* status, int* days);  // 0x555f90
    // *out = the arrival day of an en route unit (its ETA plus the trip from its departure to its owner).
    int QueryArrivalDay(GameObj* unit, StatusCode* status, int* out);  // 0x5560b0

    // ---- movement commands (see events/command_classes2.cpp) ----
    int CheckMoveEnabled(const ObjectKey* object, StatusCode* status);  // 0x5555e0
    int CheckMoveValid(const ObjectKey* object, const ObjectKey* destination, StatusCode* status);  // 0x555920
    // Starts the trip of `unit` between two locations that belong to different systems.
    int BeginTravel(GameObj* unit, const ObjectKey* from, const ObjectKey* to, NotifyContext* ctx);  // 0x556430
    // Moves the unit named by `key` to the location `location` (reparenting it, en route if the owners differ).
    int MoveUnitTo(const ObjectKey* key, ObjectKey* location, NotifyContext* ctx);  // 0x556390
    // Resumes a trip to the unit's departure key. *notHyperdrive = 1 unless it is already in hyperspace.
    int ResumeTrip(GameObj* unit, uint32_t* notHyperdrive, NotifyContext* ctx);  // 0x556620
    // Evicts a unit from a container it can no longer stay in (destroyed, wrong control, no room, blockade).
    int EnforceLocation(GameObj* unit, NotifyContext* ctx);  // 0x556710
    // The two timers: a random existing system suffers an incident of one kind or the other.
    int FireDisasterTimer(NotifyContext* ctx);  // 0x556b50
    int FireResourceTimer(NotifyContext* ctx);  // 0x556be0
    // Brings the view of `side` of the unit named by `key` up to date when it left a system for a non-system.
    int SyncUnitView(const ObjectKey* key, int side, NotifyContext* ctx);  // 0x556c70

    TrackedStat m_timerA;  // +0x58, event 0x38f
    TrackedStat m_timerB;  // +0x60, event 0x390
    SystemNeighbourTable* m_neighbours;  // +0x68
};


GameObjNotifier* CreateRulesMgr();  // 0x556f30, the factory of family 0xf9
