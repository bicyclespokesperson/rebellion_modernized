#pragma once
// Capacity bookkeeping for moves that put units into a container (a system, a fleet or a capital ship), 0x54f830-0x550fb0.
//
// The container's room is tracked in nine "buckets", one per range of object families (see GetCapacityBucketRange):
// capital ships, fighters, facilities, characters, troops, fleets, missions and so on. A CapacitySlot says how much room
// a bucket has left (or that it is unlimited) together with the two statuses that explain a refusal; a UnitCount says
// how many units a move adds to (or takes from) a bucket. A CapacityLeg holds the slots for one moving unit, split into
// what the destination accepts ("incoming") and what leaving frees ("outgoing").
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"

constexpr int kCapacityBuckets = 9;

struct UnitCount;

// The family range of bucket `index` (0..8).
const FamilyRange* GetCapacityBucketRange(int index);  // 0x54f5b0

// Room left in one bucket. VT 0x6620dc.
class CapacitySlot {
public:
    CapacitySlot();                           // 0x54f830
    CapacitySlot(const CapacitySlot& other);  // 0x54f8d0
    virtual ~CapacitySlot();                  // 0x54f880

    // Marks the slot as refusing everything: m_status = {1, 0x29}. Returns 1.
    int MarkOverCapacity();  // 0x54f940
    CapacitySlot& operator=(const CapacitySlot& other);  // 0x54fae0
    // Unlimited, nothing left over, both statuses ok. Returns 1.
    int Reset();  // 0x54fb20
    // True if `other` should replace this when the smallest room is being searched for: an error in `other` beats
    // none, then a limited slot beats an unlimited one, then less room beats more.
    int IsReplacedBy(const CapacitySlot& other) const;  // 0x54fbb0
    // The slot's refusal: m_status if it is an error, else m_limitStatus if it is one, else {1, -1}.
    StatusCode* GetError(StatusCode* out) const;  // 0x54fc40
    // Sets this (a scratch slot) to the room that `capacity` has left once `count` units are put in (`consume`) or
    // taken out (not `consume`). Over-filling copies the slot's limit status. Returns 1.
    int Compute(const CapacitySlot& capacity, const UnitCount& count, int consume);  // 0x54fca0
    // Compute, then stores the remaining room back into `capacity` if both are limited and nothing went wrong.
    int Commit(CapacitySlot* capacity, const UnitCount& count, int consume);  // 0x54fd90

    int m_unlimited;          // +0x04, 1 until a limit is known
    int m_remaining;          // +0x08
    StatusCode m_status;      // +0x0c, can the bucket take the kind at all (GameObj::Virtual29/31)
    StatusCode m_limitStatus;  // +0x14, the reason reported when the limit is exceeded (Virtual30/32)
};

// The scratch slot used while comparing; same layout, its own vtable (0x6620e4).
class CapacityScratch : public CapacitySlot {
public:
    CapacityScratch();  // 0x54fa40
    virtual ~CapacityScratch();  // 0x54fa90
};

// How many units a move adds to a bucket. VT 0x6620e0.
struct UnitCount {
    UnitCount();                       // 0x54f9b0
    UnitCount(const UnitCount& other);  // 0x54fa00
    virtual ~UnitCount();              // 0x54f9f0

    // Back to "nothing counted". Returns 1.
    int Reset();  // 0x54fa20
    int IsNonNegative() const;  // 0x54fa30

    int m_counted;  // +0x04
    int m_amount;   // +0x08
};

class CapacitySlotArray {
public:
    CapacitySlotArray();                              // 0x54fe10
    CapacitySlotArray(const CapacitySlotArray& other);  // 0x54fe80
    virtual ~CapacitySlotArray();                     // 0x54fe60

    CapacitySlotArray& operator=(const CapacitySlotArray& other);  // 0x54ff00
    int MarkAllOverCapacity();  // 0x54ff30

    CapacitySlot m_slots[kCapacityBuckets];  // +0x04
};

class UnitCountArray {
public:
    UnitCountArray();           // 0x54ff50
    virtual ~UnitCountArray();  // 0x54ffa0

    int ResetAll();            // 0x54ffc0
    int AllNonNegative() const;  // 0x54fff0

    UnitCount m_counts[kCapacityBuckets];  // +0x04
};

class CapacityTable {
public:
    CapacityTable();           // 0x550020
    virtual ~CapacityTable();  // 0x550090

    int MarkAllIncomingOverCapacity();  // 0x5500f0

    CapacitySlotArray m_incoming;  // +0x004
    CapacitySlotArray m_outgoing;  // +0x104
};

// Units a move adds, per bucket: the moving object itself and what it carries.
class UnitCountTable {
public:
    UnitCountTable();           // 0x550100
    virtual ~UnitCountTable();  // 0x550170

    int ResetAll();            // 0x5501c0
    int AllNonNegative() const;  // 0x5501e0
    // Counts `object` and its children into the buckets. A child or the object itself is counted when it passes the
    // state filter *filter (or, with `invert`, when it fails it). Refuses with {1, 0x19} an object of no bucket's
    // families, {1, 0x12} a destroyed one and {1, 0x1a} one that was never created.
    int CountObject(GameObj* object, const int* filter, int invert, StatusCode* status);  // 0x550200
    // Adds `delta` units to the bucket that holds `family`.
    int AddFamily(uint32_t family, int delta, StatusCode* status);  // 0x5504d0

    UnitCountArray m_own;       // +0x04
    UnitCountArray m_carried;   // +0x74
};

// The capacity needed by one moving unit. Keyed by the unit's key (+0x18).
class CapacityLeg : public ObjectKeyNode {
public:
    CapacityLeg();           // 0x550620
    virtual ~CapacityLeg();  // 0x5506a0

    // Fills the table from the unit's own answers (GameObj::Virtual29-32) for the move kind `kind` of side `side`.
    int Build(int side, int arg, int kind, const int* filter, GameObj* unit);  // 0x550700
    // The tightest room in `slots` once `counts` are added (released): into *out. The leg itself is unused.
    int MinRemaining(const CapacitySlotArray* slots, const UnitCountArray* counts, int consume, CapacitySlot* out);  // 0x550af0
    // Applies `counts` to every slot of `slots` (see CapacitySlot::Compute for `consume`); stops at the first failure.
    // Returns 1 if all fit.
    int CommitAll(CapacitySlotArray* slots, const UnitCountArray* counts, int consume);  // 0x550bd0
    // Tightest incoming room into *tightest, and *incoming; the outgoing room folds into *tightest.
    int MinRemainingBoth(const UnitCountTable* counts, int consume, CapacitySlot* tightest, CapacitySlot* incoming);  // 0x550c90
    int MinRemainingOutgoing(const UnitCountTable* counts, int consume, CapacitySlot* out);  // 0x550de0
    int CommitBoth(UnitCountTable* counts, int consume);       // 0x550eb0
    int CommitOutgoing(UnitCountTable* counts, int consume);  // 0x550fb0
    int MarkAllIncomingOverCapacity();  // 0x551050

    int m_flag;                 // +0x1c, 1
    ObjectKey m_parentKey;      // +0x20
    CapacityTable m_table;      // +0x24
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(CapacitySlot, 0x1c);
REB_ASSERT_SIZE(CapacityScratch, 0x1c);
REB_ASSERT_SIZE(UnitCount, 0x0c);
REB_ASSERT_SIZE(CapacitySlotArray, 0x100);
REB_ASSERT_SIZE(UnitCountArray, 0x70);
REB_ASSERT_SIZE(CapacityTable, 0x204);
REB_ASSERT_SIZE(UnitCountTable, 0xe4);
REB_ASSERT_OFFSET(CapacitySlot, m_status, 0x0c);
REB_ASSERT_OFFSET(CapacitySlot, m_limitStatus, 0x14);
REB_ASSERT_OFFSET(CapacityTable, m_outgoing, 0x104);
REB_ASSERT_OFFSET(UnitCountTable, m_carried, 0x74);
REB_ASSERT_SIZE(CapacityLeg, 0x228);
REB_ASSERT_OFFSET(CapacityLeg, m_table, 0x24);
#pragma clang diagnostic pop
