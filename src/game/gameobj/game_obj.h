#pragma once
#include <cstddef>
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj_data.h"
#include "gameobj/game_obj_root.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"

// GameObj (0x4f5ea0, VT 0x65d260, secondary ObjectBase VT 0x65d258+8) is the base of every strategic game object
// (characters, fleets, ships, systems, missions, ...). It continues GameObjNotifier's vtable from slot 12 and is
// abstract: slots 1, 5-9, 37 and 57 are still pure.
//
// Every game object exists up to three times (the "views": 0 = master, 1 = Alliance's copy, 2 = Empire's copy; the
// view is GameObjRoot::ViewSide()). Only the master is ever changed directly. Every property has the same four pieces:
//
//   SetX(value, ctx)    non-virtual validated setter, master view only: range-check, store, then
//                         view1->ApplyX(ctx); view2->ApplyX(ctx); and finally the virtual OnXChanged(old, new, ctx)
//   ApplyX(ctx)         copies the master's value into this side view (when ctx says this side is to be told and
//                         the side may see the object), then raises OnXNotified on this view
//   OnXChanged          virtual event on the master (slots 59-84); the default is a stub returning 1
//   OnXNotified         virtual event on a side view (slots 85-110, "GameObjXNotif"): debug-logs the change and
//                         forwards a UI/event notification (ids 0x100-0x107, 0x300-0x308, 0x500, ...)
//
// The properties, in vtable order (slot of OnXChanged / OnXNotified):
//   Name 59/85, ControlKind 60/86, Parent 61/87, BuilderKey 62/88, DepartureKey 63/89 (destination location at
//   departure), DestroyedReason 64/90, DeploymentCount 65/91, DestinationCount 66/92, ETA 67/93, then the 17 state
//   flag bits of m_stateFlags (+0x50): Usable 0x1, Created 0x2, Completed 0x4, Destroyed 0x8, Enroute 0x10,
//   EnrouteActive 0x20, Existing 0x40, ObservedByAlliance 0x80, ObservedByEmpire 0x100, Damaged 0x200,
//   HyperdriveActive 0x800, Autorouting 0x1000, AutoscrapRequest 0x2000, Locked 0x4000, ReadyForDelete 0x8000,
//   Constructed 0x10000, Deployed 0x20000 (68-84 / 94-110).

class Crc32;
class EventKindSet;

// The (side, ..., kind) record that CheckActivityRequest inspects. Orders build one from themselves (the order's
// side, its +0x38 argument, its target and its kind); commands keep a copy.
struct ActivityRequest {
    ActivityRequest();                             // 0x541e70
    ActivityRequest(const ActivityRequest& other);  // 0x541eb0

    ObjectKey* GetTarget(ObjectKey* out) const;  // 0x562ac0
    int Read(istream* in);      // 0x541ee0
    int Write(ostream* out);    // 0x541f20
    int Checksum(Crc32* crc);   // 0x541f60

    int m_side;          // +0x00
    int m_unk04;         // +0x04
    ObjectKey m_target;  // +0x08
    int m_kind;          // +0x0c
};

class GameObj : public GameObjNotifier, public ObjectBase {
public:
    GameObj();
    virtual ~GameObj();

    // ---- inherited slots GameObj overrides (0-11) ----
    virtual int IncludeInChecksum() override { return 1; }
    virtual int GetOwnerKey(ObjectKey* out) override;
    virtual int IsUnderSideRoot() override;
    virtual int CollectDependencies(void* context, int value, ObjectKeyValueSet* out) override;
    // Slots 5-9 stay pure (leaf classes implement them) but have bodies that those implementations call qualified.
    virtual int Read(istream* in) override = 0;               // body 0x4f9340
    virtual int Write(ostream* out) override = 0;             // body 0x4f9450
    virtual int Checksum(Crc32* crc) override = 0;            // body 0x4f9650
    virtual int Validate() override = 0;                      // body 0x4f9700
    virtual int ApplyAll(NotifyContext* ctx) override = 0;    // body 0x4f9540

    // ---- slots 12-57: queries and status checks (names provisional unless noted) ----
    virtual uint32_t GetRecordTypeBits();                    // 12: record's +0x30 & 0xfff, 0 without a record
    virtual int GetTransitSpeed(int arg);                          // 13: DAT_006b9050 if state flag 0x40 and arg, else 0
    virtual int IsConstructed();                         // 14: state flag 0x10000
    virtual int GetEventKinds(int unused, EventKindSet* out);            // 15: assigns the class's static kind set to *out
    virtual int Virtual16() { return 0; }                                 // 16: 0
    virtual int Virtual17();                                 // 17: !Virtual16()
    virtual int Virtual18() { return 0; }                                 // 18: 0
    virtual int IsObservable() { return 1; }                                 // 19: 1
    virtual int CanBeControlledBy(int arg) { return 1; }                          // 20: 1
    virtual int CheckSameControl(GameObj* other, StatusCode* out);  // 21: status {1,-1} if the control kinds agree else {1,0x24}
    virtual int CheckOrder(int a, int b, StatusCode* out);    // 22
    virtual int CheckRequestImpl(const ActivityRequest* request, StatusCode* out);           // 23
    virtual int CheckRequest(const ActivityRequest* request, StatusCode* out);           // 24: out = {1,-1}, then Virtual23
    virtual int CheckRenameImpl(const ActivityRequest* request, StatusCode* out);           // 25
    virtual int CheckRename(const ActivityRequest* request, int name, StatusCode* out);    // 26
    virtual int CheckUnknownImpl(const ActivityRequest* request, StatusCode* out);           // 27
    virtual int CheckUnknown(const ActivityRequest* request, StatusCode* out);           // 28
    virtual int Virtual29(int a, int b, int c, int d, StatusCode* out);               // 29
    virtual int Virtual30(const FamilyRange* range, int b, int* o1, int* o2, int* o3, StatusCode* out);  // 30
    virtual int Virtual31(int a, int b, int c, int d, StatusCode* out);               // 31
    virtual int Virtual32(const FamilyRange* range, int b, int* o1, int* o2, int* o3, StatusCode* out);  // 32
    virtual int UpdateUsable(NotifyContext* ctx);               // 33: derives Usable from Existing/Completed/Enroute
    virtual int UpdateExisting(NotifyContext* ctx);               // 34: derives Existing from Created/Destroyed
    virtual int UpdateObservedByAlliance(NotifyContext* ctx);               // 35
    virtual int UpdateObservedByEmpire(NotifyContext* ctx);               // 36
    virtual int CreateViews(int controlKind, NotifyContext* ctx) = 0;  // body 0x4f9190           // 37
    virtual int DestroyIfAllowed(const ActivityRequest* request, NotifyContext* ctx);        // 38
    virtual int TryRename(const ActivityRequest* request, int name, NotifyContext* ctx);  // 39
    virtual int Destroy(NotifyContext* ctx);               // 40
    virtual int Rename(String* name, NotifyContext* ctx);  // 41: SetName
    virtual int Reparent(ObjectKey* key, NotifyContext* ctx);  // 42
    virtual int ChangeDestroyedReason(int reason, NotifyContext* ctx);   // 43: SetDestroyedReason
    virtual int Virtual44(NotifyContext* ctx);               // 44
    virtual int Virtual45(int a, NotifyContext* ctx);        // 45
    virtual int Virtual46(NotifyContext* ctx);               // 46
    virtual int Virtual47(int a) { return 1; }                            // 47: 1
    virtual int Virtual48(int a) { return 1; }                            // 48: 1
    virtual int Virtual49(int a, int b, int c) { return 1; }              // 49: 1
    virtual int Virtual50(int a, int b, int c) { return 1; }              // 50: 1
    virtual int Virtual51(int a, int b, int c) { return 1; }              // 51: 1
    virtual int Virtual52(int a, int b, int c) { return 1; }              // 52: 1
    virtual int Virtual53(int a, int b, int c) { return 1; }              // 53: 1
    virtual int ApplyAllToViewOf(int view, int unused, NotifyContext* ctx);  // 54
    virtual int Virtual55(int a) { return 1; }                            // 55: 1
    virtual int AutoscrapIfRequested(NotifyContext* ctx);               // 56
    virtual GameObj* CreateViewObject() = 0;                 // 57: allocates an empty object of the leaf class

    // ---- slot 58: creates the data extension (m_data); the default allocates the 0x18-byte base class ----
    virtual GameObjData* CreateData();

    // ---- slots 59-84: OnXChanged events on the master ----
    virtual int OnNameChanged(String* oldName, String* newName, NotifyContext* ctx) { return 1; }                      // 59
    virtual int OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx);              // 60
    virtual int OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx);  // 61
    virtual int OnBuilderKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) { return 1; }             // 62
    virtual int OnDepartureKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);           // 63
    virtual int OnDestroyedReasonChanged(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx);        // 64
    virtual int OnDeploymentCountChanged(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx);        // 65
    virtual int OnDestinationCountChanged(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx) { return 1; }       // 66
    virtual int OnEtaChanged(int oldValue, int newValue, NotifyContext* ctx);                              // 67
    virtual int OnUsableChanged(int newValue, NotifyContext* ctx);                                         // 68
    virtual int OnCreatedChanged(int newValue, NotifyContext* ctx);                                        // 69
    virtual int OnCompletedChanged(int newValue, NotifyContext* ctx);                                      // 70
    virtual int OnDestroyedChanged(int newValue, NotifyContext* ctx);                                      // 71
    virtual int OnEnrouteChanged(int newValue, NotifyContext* ctx);                                        // 72
    virtual int OnEnrouteActiveChanged(int newValue, NotifyContext* ctx);                                  // 73
    virtual int OnExistingChanged(int newValue, NotifyContext* ctx);                                       // 74
    virtual int OnObservedByAllianceChanged(int newValue, NotifyContext* ctx);                             // 75
    virtual int OnObservedByEmpireChanged(int newValue, NotifyContext* ctx);                               // 76
    virtual int OnDamagedChanged(int newValue, NotifyContext* ctx);                                        // 77
    virtual int OnHyperdriveActiveChanged(int newValue, NotifyContext* ctx);                               // 78
    virtual int OnAutoroutingChanged(int newValue, NotifyContext* ctx);                                    // 79
    virtual int OnAutoscrapRequestChanged(int newValue, NotifyContext* ctx);                               // 80
    virtual int OnLockedChanged(int newValue, NotifyContext* ctx);                                         // 81
    virtual int OnReadyForDeleteChanged(int newValue, NotifyContext* ctx);                                 // 82
    virtual int OnConstructedChanged(int newValue, NotifyContext* ctx);                                    // 83
    virtual int OnDeployedChanged(int newValue, NotifyContext* ctx) { return 1; }                                       // 84

    // ---- slots 85-110: OnXNotified events on a side view ----
    virtual int OnNameNotified(String* oldName, String* newName, NotifyContext* ctx);                         // 85
    virtual int OnControlKindNotified(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx);             // 86
    virtual int OnParentNotified(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx);  // 87
    virtual int OnBuilderKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);            // 88
    virtual int OnDepartureKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);          // 89
    virtual int OnDestroyedReasonNotified(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx);       // 90
    virtual int OnDeploymentCountNotified(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx);       // 91
    virtual int OnDestinationCountNotified(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx);      // 92
    virtual int OnEtaNotified(int oldValue, int newValue, NotifyContext* ctx);                             // 93
    virtual int OnUsableNotified(int value, NotifyContext* ctx);                                        // 94
    virtual int OnCreatedNotified(int value, NotifyContext* ctx);                                       // 95
    virtual int OnCompletedNotified(int value, NotifyContext* ctx);                                     // 96
    virtual int OnDestroyedNotified(int value, NotifyContext* ctx);                                     // 97
    virtual int OnEnrouteNotified(int value, NotifyContext* ctx);                                       // 98
    virtual int OnEnrouteActiveNotified(int value, NotifyContext* ctx);                                 // 99
    virtual int OnExistingNotified(int value, NotifyContext* ctx);                                      // 100
    virtual int OnObservedByAllianceNotified(int value, NotifyContext* ctx);                            // 101
    virtual int OnObservedByEmpireNotified(int value, NotifyContext* ctx);                              // 102
    virtual int OnDamagedNotified(int value, NotifyContext* ctx);                                       // 103
    virtual int OnHyperdriveActiveNotified(int value, NotifyContext* ctx);                              // 104
    virtual int OnAutoroutingNotified(int value, NotifyContext* ctx);                                   // 105
    virtual int OnAutoscrapRequestNotified(int value, NotifyContext* ctx);                              // 106
    virtual int OnLockedNotified(int value, NotifyContext* ctx);                                        // 107
    virtual int OnReadyForDeleteNotified(int value, NotifyContext* ctx);                                // 108
    virtual int OnConstructedNotified(int value, NotifyContext* ctx);                                   // 109
    virtual int OnDeployedNotified(int value, NotifyContext* ctx);                                      // 110

    // ObjectBase slot 0 (secondary vtable): appends the object's name to *out; `kind` as in DatRecord::WriteName.
    virtual void WriteName(String* out, int kind) override;

    // ==== SECTION core (0x4f5ea0-0x4f6c00): see game_obj.cpp ====
    // The object's copy in `view` (0 master, 1 Alliance, 2 Empire); every view of a GameObj is a GameObj.
    GameObj* ViewAs(int view) { return static_cast<GameObj*>(GetViewObject(view)); }
    // The next object in key order whose family is in `range` (same view); with `resolve`, hidden ones are skipped.
    GameObjNotifier* FindNextSibling(const FamilyRange* range, int resolve);  // 0x4f6010
    // Copy of the object's name: its own, else the record's, else "".
    String* GetNameCopy(String* out);  // 0x4f6270
    String* GetName();                 // 0x4f62d0, same lookup, no copy (a static empty String if there is none)
    // Stores the parent in *out; true if there is one.
    int GetParent(GameObjNotifier** out);  // 0x4f6b50
    // Parent key into *out; 1 only if it is set.
    int GetValidParentKey(ObjectKey* out);  // 0x4f6b70
    // Child-iteration state filter: 1 usable, 2 en route, 3 existing, 4 always; other values are returned unchanged.
    uint32_t MatchesStateFilter(uint32_t mode);  // 0x4f6b90
    int CountChildren(const FamilyRange* range, uint32_t side, uint32_t filter);  // 0x4f6c00
    // Destroyed in all three views, childless, and attached to a side root.
    int IsDestructionComplete();  // 0x4f6c70
    int CanBeDeleted();           // 0x4f6da0

    // ==== SECTION setters (0x4f6c00-0x4f8880): see game_obj_setters.cpp ====
    // Data block accessors (m_data must exist).
    int GetDataField04(int* out);         // 0x4f6df0
    int GetLockCount(int* out);           // 0x4f6e10
    int GetDataKey(ObjectKey* out);       // 0x4f6e30
    void* GetDataExtra();                 // 0x4f6e50, the data block's +0x10 member or null
    int SetDataField04(int value);        // 0x4f7f60
    int SetLockCount(int value);          // 0x4f7f80
    int SetDataKey(const ObjectKey* key);  // 0x4f7fa0
    int IncrementLock(NotifyContext* ctx);  // 0x4f8040, sets Locked
    int DecrementLock(NotifyContext* ctx);  // 0x4f8090, clears Locked at zero
    int NotifyDataEvent387(int level, NotifyContext* ctx);   // 0x4f8010
    // Creates the object for `key` as a child of this one: Initialize, Reparent, SetCreated, SetCompleted.
    GameObj* CreateChild(const ObjectKey* key, int initArg, NotifyContext* ctx, int created, int completed);  // 0x4f7d50
    int CreateChildren(int count, const ObjectKey* key, int initArg, NotifyContext* ctx, int created, int completed);  // 0x4f7cf0
    int UpdateChildren(const FamilyRange* range, NotifyContext* ctx);  // 0x4f7e30
    int UpdateSelfAndDescendants(NotifyContext* ctx);                  // 0x4f7f20
    int InheritDestroyedReason(NotifyContext* ctx);                    // 0x4f7ed0
    int UpdateHyperdriveAndEta(NotifyContext* ctx);                    // 0x4f7fc0
    int UpdateDepartureKey(NotifyContext* ctx);                        // 0x4f80e0
    int UpdateEta(NotifyContext* ctx);                                 // 0x4f81c0
    int UpdateEnroute(NotifyContext* ctx);                             // 0x4f8240
    int RequestDeleteIfReady(NotifyContext* ctx);                      // 0x4f82d0, always 1
    int ReleaseIfDestroyed(NotifyContext* ctx);                        // 0x4f8310, always 1
    int SetName(String* value, NotifyContext* ctx);  // 0x4f6e60
    int SetControlKind(uint32_t value, NotifyContext* ctx);  // 0x4f6f40
    int SetParent(GameObjNotifier* value, NotifyContext* ctx);  // 0x4f6fd0
    int SetBuilderKey(ObjectKey* value, NotifyContext* ctx);  // 0x4f7070
    int SetDepartureKey(ObjectKey* value, NotifyContext* ctx);  // 0x4f7120
    int SetDestroyedReason(uint32_t value, NotifyContext* ctx);  // 0x4f71d0
    int SetDeploymentCount(uint32_t value, NotifyContext* ctx);  // 0x4f7260
    int SetDestinationCount(uint32_t value, NotifyContext* ctx);  // 0x4f72f0
    int SetEta(int value, NotifyContext* ctx);  // 0x4f7390
    int SetUsable(int on, NotifyContext* ctx);  // 0x4f7410
    int SetCreated(int on, NotifyContext* ctx);  // 0x4f7480
    int SetCompleted(int on, NotifyContext* ctx);  // 0x4f74f0
    int SetDestroyed(int on, NotifyContext* ctx);  // 0x4f7560
    int SetEnroute(int on, NotifyContext* ctx);  // 0x4f75d0
    int SetEnrouteActive(int on, NotifyContext* ctx);  // 0x4f7640
    int SetExisting(int on, NotifyContext* ctx);  // 0x4f76b0
    int SetObservedByAlliance(int on, NotifyContext* ctx);  // 0x4f7720
    int SetObservedByEmpire(int on, NotifyContext* ctx);  // 0x4f7790
    int SetDamaged(int on, NotifyContext* ctx);  // 0x4f7800
    int SetHyperdriveActive(int on, NotifyContext* ctx);  // 0x4f7870
    int SetAutorouting(int on, NotifyContext* ctx);  // 0x4f78e0
    int SetAutoscrapRequest(int on, NotifyContext* ctx);  // 0x4f7950
    int SetLocked(int on, NotifyContext* ctx);  // 0x4f79c0
    int SetReadyForDelete(int on, NotifyContext* ctx);  // 0x4f7a30
    int SetConstructed(int on, NotifyContext* ctx);  // 0x4f7aa0
    int SetDeployed(int on, NotifyContext* ctx);  // 0x4f7b10

    // ==== SECTION notif (0x4f8880-0x4fcd90): see game_obj_notif.cpp ====
    // Notification debug log (dead in the shipped game: guarded by a call that always returns 0). All return 1 when
    // logging is off. `unused` is passed through as the formatter's `extra` text.
    int LogFlag(const char* tag, const char* property, int value, int unused, NotifyContext* ctx);  // 0x4f8880
    int LogSide(const char* tag, const char* property, uint32_t newSide, uint32_t oldSide, int unused,
                NotifyContext* ctx);  // 0x4f88d0
    int LogInt(const char* tag, const char* property, int newValue, int oldValue, int unused,
               NotifyContext* ctx);  // 0x4f8980
    int LogString(const char* tag, const char* property, String* newValue, String* oldValue, int unused,
                  NotifyContext* ctx);  // 0x4f8a50
    int LogKey(const char* tag, const char* property, ObjectKey* newKey, ObjectKey* oldKey, int unused,
               NotifyContext* ctx);  // 0x4f8aa0
    int LogTypeKey(const char* tag, const char* property, ObjectKey* newKey, ObjectKey* oldKey, int unused,
                   NotifyContext* ctx);  // 0x4f8b80
    // The formatter itself (0x4f8c60): not reconstructed, never reached.
    int LogChange(const char* tag, const char* property, const char* newText, const char* oldText, const char* extra,
                  NotifyContext* ctx);
    // Checks whether an activity request (order/event with a side and kind) is allowed; fills *out.
    int CheckActivityRequest(const ActivityRequest* request, StatusCode* out);  // 0x4f9860
    // Re-evaluates which sides see this object (side mask of views 1 and 2).
    int UpdateViewVisibility(NotifyContext* ctx);  // 0x4f9a60
    ObjectKey* GetBuilderKey(ObjectKey* out);      // 0x48a640
    ObjectKey* GetDepartureKey(ObjectKey* out);    // 0x4fa150
    // Applies every property of the master to this side view's object `view` of the same key (1 if there is none).
    int ApplyAllToView(int view, NotifyContext* ctx);  // 0x4f9510
    // Creates m_name if absent; 1 if there is a name afterwards.
    int InstallName();  // 0x4fc680
    // Makes `child` a child of this object (it keeps the previous parent's bookkeeping straight).
    int AdoptChild(GameObjNotifier* child, NotifyContext* ctx);  // 0x4fc600
    int RefreshParentView(NotifyContext* ctx);        // 0x4fca30
    int ReleaseDestroyedSubtree(NotifyContext* ctx);  // 0x4fc710
    int ReleaseDestroyed(NotifyContext* ctx);         // 0x4fc840
    int MoveToSideRoot(NotifyContext* ctx);           // 0x4fcb50
    int ApplyName(NotifyContext* ctx);  // 0x4f9ba0
    int ApplyControlKind(NotifyContext* ctx);  // 0x4f9ca0
    int ApplyParent(NotifyContext* ctx);  // 0x4f9d40
    int ApplyBuilderKey(NotifyContext* ctx);  // 0x4f9f10
    int ApplyDepartureKey(NotifyContext* ctx);  // 0x4fa030
    int ApplyDestroyedReason(NotifyContext* ctx);  // 0x4fa170
    int ApplyDeploymentCount(NotifyContext* ctx);  // 0x4fa210
    int ApplyDestinationCount(NotifyContext* ctx);  // 0x4fa2a0
    int ApplyEta(NotifyContext* ctx);  // 0x4fa350
    int ApplyUsable(NotifyContext* ctx);  // 0x4fa3d0
    int ApplyCreated(NotifyContext* ctx);  // 0x4fa460
    int ApplyCompleted(NotifyContext* ctx);  // 0x4fa4f0
    int ApplyDestroyed(NotifyContext* ctx);  // 0x4fa580
    int ApplyEnroute(NotifyContext* ctx);  // 0x4fa620
    int ApplyEnrouteActive(NotifyContext* ctx);  // 0x4fa6b0
    int ApplyExisting(NotifyContext* ctx);  // 0x4fa740
    int ApplyObservedByAlliance(NotifyContext* ctx);  // 0x4fa7d0
    int ApplyObservedByEmpire(NotifyContext* ctx);  // 0x4fa860
    int ApplyDamaged(NotifyContext* ctx);  // 0x4fa8f0
    int ApplyHyperdriveActive(NotifyContext* ctx);  // 0x4fa980
    int ApplyAutorouting(NotifyContext* ctx);  // 0x4faa10
    int ApplyAutoscrapRequest(NotifyContext* ctx);  // 0x4faaa0
    int ApplyLocked(NotifyContext* ctx);  // 0x4fab30
    int ApplyReadyForDelete(NotifyContext* ctx);  // 0x4fabc0
    int ApplyConstructed(NotifyContext* ctx);  // 0x4fac50
    int ApplyDeployed(NotifyContext* ctx);  // 0x4face0
    // Calls CreateData() and stores the result in m_data; true if it is non-null.
    int InstallData();  // 0x4fc5c0
    // Deletes m_data, if any. Returns 1.
    int DestroyData();  // 0x4fc5e0

    // ---- fields ----
    String* m_name;               // +0x34, null = use the record's name
    ObjectKey m_builderKey;       // +0x38
    ObjectKey m_departureKey;     // +0x3c
    uint32_t m_packed;            // +0x40, byte 0 DestroyedReason, byte 1 DeploymentCount, byte 2 DestinationCount
    int m_eta;                    // +0x44
    int m_unk48;                  // +0x48
    int m_unk4c;                  // +0x4c
    uint32_t m_stateFlags;        // +0x50, the 17 flag bits listed above
    GameObjData* m_data;          // +0x54, created by CreateData() for master views only
};

// Runs UpdateChildren over every family (0x4f8340).
int UpdateAllChildren(GameObj* object, NotifyContext* ctx);
// Unregisters, detaches and deletes the three views of an object that is ready for deletion (0x4f83b0).
int DeleteObjectViews(GameObj* object);
// The default of the leaf classes' static event-kind sets: an empty set (0x4fad70).
int InitEventKinds(int unused, EventKindSet* out);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(GameObj, 0x58);
REB_ASSERT_OFFSET(GameObj, m_name, 0x34);
REB_ASSERT_OFFSET(GameObj, m_builderKey, 0x38);
REB_ASSERT_OFFSET(GameObj, m_departureKey, 0x3c);
REB_ASSERT_OFFSET(GameObj, m_packed, 0x40);
REB_ASSERT_OFFSET(GameObj, m_eta, 0x44);
REB_ASSERT_OFFSET(GameObj, m_stateFlags, 0x50);
REB_ASSERT_OFFSET(GameObj, m_data, 0x54);
#pragma clang diagnostic pop
