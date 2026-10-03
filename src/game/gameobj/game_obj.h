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
    virtual int Virtual13(int arg);                          // 13: DAT_006b9050 if state flag 0x40 and arg, else 0
    virtual int IsConstructedFlag();                         // 14: state flag 0x10000
    virtual int GetEventKinds(EventKindSet* out);            // 15: assigns the class's static kind set to *out
    virtual int Virtual16() { return 0; }                                 // 16: 0
    virtual int Virtual17();                                 // 17: !Virtual16()
    virtual int Virtual18() { return 0; }                                 // 18: 0
    virtual int Virtual19() { return 1; }                                 // 19: 1
    virtual int Virtual20(int arg) { return 1; }                          // 20: 1
    virtual int Virtual21(GameObj* other, StatusCode* out);  // 21: status {1,-1} if the control kinds agree else {1,0x24}
    virtual int Virtual22(int a, int b, StatusCode* out);    // 22
    virtual int Virtual23(int a, StatusCode* out);           // 23
    virtual int Virtual24(int a, StatusCode* out);           // 24: out = {1,-1}, then Virtual23
    virtual int Virtual25(int a, StatusCode* out);           // 25
    virtual int Virtual26(int a, int b, StatusCode* out);    // 26
    virtual int Virtual27(int a, StatusCode* out);           // 27
    virtual int Virtual28(int a, StatusCode* out);           // 28
    virtual int Virtual29(int a, int b, int c, StatusCode* out);               // 29
    virtual int Virtual30(int a, int b, int* o1, int* o2, int* o3, StatusCode* out);  // 30
    virtual int Virtual31(int a, int b, int c, StatusCode* out);               // 31
    virtual int Virtual32(int a, int b, int* o1, int* o2, int* o3, StatusCode* out);  // 32
    virtual int Virtual33(NotifyContext* ctx);               // 33: derives Usable from Existing/Completed/Enroute
    virtual int Virtual34(NotifyContext* ctx);               // 34: derives Existing from Created/Destroyed
    virtual int Virtual35(NotifyContext* ctx);               // 35
    virtual int Virtual36(NotifyContext* ctx);               // 36
    virtual int Virtual37(NotifyContext* ctx) = 0;           // 37
    virtual int Virtual38(int a, NotifyContext* ctx);        // 38
    virtual int Virtual39(int a, int b, NotifyContext* ctx);  // 39
    virtual int Virtual40(NotifyContext* ctx);               // 40
    virtual int Virtual41(String* name, NotifyContext* ctx);  // 41: SetName
    virtual int Virtual42(ObjectKey* key, NotifyContext* ctx);  // 42
    virtual int Virtual43(int reason, NotifyContext* ctx);   // 43: SetDestroyedReason
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
    virtual int Virtual54(int a, int b, NotifyContext* ctx);  // 54
    virtual int Virtual55(int a) { return 1; }                            // 55: 1
    virtual int Virtual56(NotifyContext* ctx);               // 56
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
    virtual int OnUsableChanged(int oldValue, NotifyContext* ctx);                                         // 68
    virtual int OnCreatedChanged(int oldValue, NotifyContext* ctx);                                        // 69
    virtual int OnCompletedChanged(int oldValue, NotifyContext* ctx);                                      // 70
    virtual int OnDestroyedChanged(int oldValue, NotifyContext* ctx);                                      // 71
    virtual int OnEnrouteChanged(int oldValue, NotifyContext* ctx);                                        // 72
    virtual int OnEnrouteActiveChanged(int oldValue, NotifyContext* ctx);                                  // 73
    virtual int OnExistingChanged(int oldValue, NotifyContext* ctx);                                       // 74
    virtual int OnObservedByAllianceChanged(int oldValue, NotifyContext* ctx);                             // 75
    virtual int OnObservedByEmpireChanged(int oldValue, NotifyContext* ctx);                               // 76
    virtual int OnDamagedChanged(int oldValue, NotifyContext* ctx);                                        // 77
    virtual int OnHyperdriveActiveChanged(int oldValue, NotifyContext* ctx);                               // 78
    virtual int OnAutoroutingChanged(int oldValue, NotifyContext* ctx);                                    // 79
    virtual int OnAutoscrapRequestChanged(int oldValue, NotifyContext* ctx);                               // 80
    virtual int OnLockedChanged(int oldValue, NotifyContext* ctx);                                         // 81
    virtual int OnReadyForDeleteChanged(int oldValue, NotifyContext* ctx);                                 // 82
    virtual int OnConstructedChanged(int oldValue, NotifyContext* ctx);                                    // 83
    virtual int OnDeployedChanged(int oldValue, NotifyContext* ctx) { return 1; }                                       // 84

    // ---- slots 85-110: OnXNotified events on a side view ----
    virtual int OnNameNotified(String* name, String* oldName, NotifyContext* ctx);                         // 85
    virtual int OnControlKindNotified(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx);             // 86
    virtual int OnParentNotified(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx);  // 87
    virtual int OnBuilderKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);            // 88
    virtual int OnDepartureKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);          // 89
    virtual int OnDestroyedReasonNotified(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx);       // 90
    virtual int OnDeploymentCountNotified(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx);       // 91
    virtual int OnDestinationCountNotified(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx);      // 92
    virtual int OnEtaNotified(int oldValue, int newValue, NotifyContext* ctx);                             // 93
    virtual int OnUsableNotified(int oldValue, NotifyContext* ctx);                                        // 94
    virtual int OnCreatedNotified(int oldValue, NotifyContext* ctx);                                       // 95
    virtual int OnCompletedNotified(int oldValue, NotifyContext* ctx);                                     // 96
    virtual int OnDestroyedNotified(int oldValue, NotifyContext* ctx);                                     // 97
    virtual int OnEnrouteNotified(int oldValue, NotifyContext* ctx);                                       // 98
    virtual int OnEnrouteActiveNotified(int oldValue, NotifyContext* ctx);                                 // 99
    virtual int OnExistingNotified(int oldValue, NotifyContext* ctx);                                      // 100
    virtual int OnObservedByAllianceNotified(int oldValue, NotifyContext* ctx);                            // 101
    virtual int OnObservedByEmpireNotified(int oldValue, NotifyContext* ctx);                              // 102
    virtual int OnDamagedNotified(int oldValue, NotifyContext* ctx);                                       // 103
    virtual int OnHyperdriveActiveNotified(int oldValue, NotifyContext* ctx);                              // 104
    virtual int OnAutoroutingNotified(int oldValue, NotifyContext* ctx);                                   // 105
    virtual int OnAutoscrapRequestNotified(int oldValue, NotifyContext* ctx);                              // 106
    virtual int OnLockedNotified(int oldValue, NotifyContext* ctx);                                        // 107
    virtual int OnReadyForDeleteNotified(int oldValue, NotifyContext* ctx);                                // 108
    virtual int OnConstructedNotified(int oldValue, NotifyContext* ctx);                                   // 109
    virtual int OnDeployedNotified(int oldValue, NotifyContext* ctx);                                      // 110

    // ObjectBase slot 0 (secondary vtable): appends the object's name to *out; `kind` as in DatRecord::WriteName.
    virtual void WriteName(String* out, int kind) override;

    // ==== SECTION core (0x4f5ea0-0x4f6c00): see game_obj.cpp ====

    // ==== SECTION setters (0x4f6c00-0x4f8880): see game_obj_setters.cpp ====
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
    bool InstallData();  // 0x4fc5c0
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
