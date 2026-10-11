#pragma once
// The capacity check of a move, 0x551060-0x553130: a RouteContext collects one CapacityLeg per object that takes
// part (the moving units, the containers they leave and the destination) and answers whether the move fits.
//
// The context is a set of legs keyed by object key. Its own status (+0x0c) is {1, -1} while it is usable; the
// operations below report refusals by filling a StatusCode.
#include <cstdint>

#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "orders/route_capacity.h"

class RouteContext : public ObjectKeySet {
public:
    RouteContext();           // 0x551060
    ~RouteContext() override;  // 0x5510b0

    // Sets side, argument, kind and the state filter the legs are built with; the status becomes {1, -1}. Returns 1.
    int Init(int side, int arg, int kind, const int* filter);  // 0x551100
    // Init for the side and kind of an order's request.
    int InitFromRequest(const ActivityRequest* request, int arg, const int* filter);  // 0x551190
    // Init for the side that controls `object`; kind 0x241 for a character or special force, else 0x204.
    int InitForObject(GameObj* object, int arg, const int* filter);  // 0x5511d0

    // Adds legs for the unit named by `key` and for its parent (a leg is added only if the key has none yet).
    int AddUnitLegs(const ObjectKey* key);  // 0x551270
    // Adds legs for the container of the unit named by `key`, that container's parent, and every fleet and capital ship
    // inside the container.
    int AddContainerLegs(const ObjectKey* key);  // 0x5513a0
    // Marks every incoming slot of the leg for `key` as over capacity. Returns 1 if the key has no leg.
    int MarkOverCapacity(const ObjectKey* key);  // 0x5515d0

    // Moves the unit `unit` to `destination`: counts it, frees its parent, and checks and commits the room at the
    // destination. *slot gets the tightest room found, *tightest the key of the leg that gave it.
    int CheckMove(const ObjectKey* unit, const ObjectKey* destination, StatusCode* status, CapacitySlot* slot,
                  ObjectKey* tightest);  // 0x551630
    // The same for the two sets of units `units` and `others`, moving to `destination`.
    int CheckMoveSets(const ObjectKeySet* units, const ObjectKeySet* others, const ObjectKey* destination,
                      StatusCode* status, CapacitySlot* slot, ObjectKey* tightest);  // 0x551770
    // As CheckMove but without freeing the parent.
    int CheckArrival(const ObjectKey* unit, const ObjectKey* destination, StatusCode* status, CapacitySlot* slot,
                     ObjectKey* tightest);  // 0x551900
    // As CheckArrival for a unit of the family named by the top byte of `unitKey` (nothing is resolved).
    int CheckArrivalOfFamily(const ObjectKey* unitKey, const ObjectKey* destination, StatusCode* status,
                             CapacitySlot* slot, ObjectKey* tightest);  // 0x551a20
    int CheckMoveSimple(const ObjectKey* unit, const ObjectKey* destination, StatusCode* status,
                        CapacitySlot* slot);  // 0x551b50
    // Counts what is *not* matching the state filter in `unit` and checks the room at the destination.
    int CheckMoveInverted(const ObjectKey* unit, const ObjectKey* destination, StatusCode* status, CapacitySlot* slot);  // 0x551c80
    int CheckFamilyArrival(const ObjectKey* unitKey, const ObjectKey* destination, StatusCode* status,
                           CapacitySlot* slot);  // 0x551dc0
    int CheckUnitArrival(const ObjectKey* unit, const ObjectKey* destination, StatusCode* status, CapacitySlot* slot);  // 0x551ee0
    // Frees the room that the units in `units` take in their parents.
    int FreeParentsOf(const ObjectKeySet* units, StatusCode* status);  // 0x552150
    int FreeCarriedParents(GameObj* object, StatusCode* status);  // 0x552210
    // Frees the room that the unit `key` takes in its parent: counts it (not inverted) and gives it back.
    int FreeParent(const ObjectKey* key, StatusCode* status);  // 0x552000
    // The room left at `destination` once `counts` are put in (consume) or taken out; also looks into the system's
    // fleets and their ships when the destination is a system.
    int CheckRoom(UnitCountTable* counts, int consume, const ObjectKey* destination, StatusCode* status,
                  CapacitySlot* slot, ObjectKey* tightest);  // 0x552300
    int CheckRoomCounts(UnitCountTable* counts, int consume, const ObjectKey* destination, StatusCode* status,
                        CapacitySlot* slot);  // 0x552860
    int CountUnit(const ObjectKey* key, StatusCode* status, UnitCountTable* counts);  // 0x5529a0
    int CountUnits(const ObjectKeySet* keys, StatusCode* status, UnitCountTable* counts);  // 0x552a50
    // Folds the room that the leg of `object` leaves into *slot; *best is the leg that gave the tightest room so far.
    int MinRoomAt(GameObj* object, UnitCountTable* counts, int consume, CapacityLeg** best, CapacitySlot* slot);  // 0x552b10
    // Builds the leg of `object` and adds it to the set. Returns 0 if the leg cannot be built.
    int AddLeg(GameObj* object);  // 0x552d10
    // Applies `counts` to the leg of `key` and its parent leg.
    int Commit(UnitCountTable* counts, int consume, const ObjectKey* key);  // 0x552dd0
    int ResolveObject(const ObjectKey* key, StatusCode* status, GameObjNotifier** out);  // 0x552e80
    int ResolveUnit(const ObjectKey* key, StatusCode* status, GameObjNotifier** out);    // 0x552f40
    int ResolveContainer(GameObj* object, StatusCode* status, GameObjNotifier** out);    // 0x552ff0
    // The leg of `key` and the leg of its parent.
    int FindLegAndParent(const ObjectKey* key, CapacityLeg** leg, CapacityLeg** parent);  // 0x5530d0
    // Looks `key` up in the object database view of the context's side. Returns 0 if the side is not 1 or 2.
    int ResolveInSideView(const ObjectKey* key, GameObjNotifier** out);  // 0x553130
    // {1, 0x25} unless `object` is a system, a 0x98-0x9f object, a fleet or a capital ship. Returns 1.
    int CheckObjectKind(GameObj* object, StatusCode* status);  // 0x5531b0

    int m_flag;          // +0x08, 1
    StatusCode m_status;  // +0x0c
    int m_side;          // +0x14
    int m_arg;           // +0x18
    int m_kind;          // +0x1c
    int m_filter;        // +0x20
};
