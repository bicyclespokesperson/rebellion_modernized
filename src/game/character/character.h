#pragma once
#include <cstdint>

#include "character/role.h"
#include "core/layout.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_obj_data.h"
#include "gameobj/unit_obj_base.h"

// Character (VT 0x65ca70, secondary ObjectBase VT 0x65ca6c, 0xb0 bytes) is the base of every individual character
// (family 0x30-0x3b): the leaf classes are the minor characters, the named story characters (Luke, Leia, Han, ...) and
// the major characters (MajorCharacter, 0x594520). It extends Role with the "enhanced" skills (the Role skills plus
// the Force and experience modifiers), the Force and injury state, the command and captivity state, four keys and ten
// flags. Strings: "Character*Notif", "charac.cpp: Invalid EnhancedLoyalty value!", "Invalid CommandKind value!",
// "Invalid State value!", "Invalid MissionHyperdriveModifier value!".
//
// Vtable: 251 slots. 0-178 are Role's; 179-190 are new non-property virtuals (provisional names), 191-220 are the
// OnXChanged events of the 30 properties and 221-250 the OnXNotified events, in the order of the fields:
//   16 values  int16 at +0x7c + 2*i: EnhancedDiplomacy, EnhancedEspionage, EnhancedShipyardRD, EnhancedTrainingFacilRD,
//              EnhancedConstructionYardRD, EnhancedCombat, EnhancedLeadership, EnhancedLoyalty, Force (+0x8c),
//              ForceExperience, ForceTraining, LeadershipAdjustment, Injury (+0x94), CommandKind (+0x96), State
//              (+0x98, constructed as 4), MissionHyperdriveModifier (+0x9a, constructed as the global at 0x6b9050)
//   4 keys     Encounter +0x9c, TraitorDiscovered +0xa0, ForceUserDiscovered +0xa4, Commanding +0xa8
//   10 flags   m_flagsAc masks Captured 0x1, CanHeal 0x2, FastHeal 0x4, ForceAware 0x10, ForcePotential 0x20, Healing 0x40,
//              DiscoveringForceUser 0x80, CanEscape 0x100, EscapeRequest 0x200, EscapeAttempt 0x400 (0x8 is unused)
// The Set/Apply/OnChanged/OnNotified pattern is the one of GameObj (see role.h).
// The secondary vtable's slot (ObjectBase::WriteName) is the free function AppendCharacterName (0x4ed060), like
// GameObj's AppendGameObjName: it takes the ObjectBase subobject (this + 0x30).

class Crc32;
class EventKindSet;
class MajorCharacterSD;

// The data block of Character master views (VT 0x661ac0, 0x38 bytes, ctor 0x540750 -- outside the Character ranges and
// not reconstructed here; it derives from the pass-through unit data block through the class at 0x583df0). The three
// stats are raised as events 0x388-0x38a by OnHealingChanged and friends.
class CharacterData : public UnitObjData {
public:
    CharacterData();                  // 0x540750

    TrackedStat m_healingStat;        // +0x18, event 0x388
    TrackedStat m_forceDiscoveryStat;  // +0x20, event 0x389
    TrackedStat m_escapeStat;         // +0x28, event 0x38a
    ObjectKey m_holderKey;            // +0x30, reset whenever the State changes
    int m_field34;                    // +0x34
};

class Character : public Role {
public:
    Character();                                                      // 0x4eceb0
    virtual ~Character();                                             // 0x4ecfb0 (deleting), 0x4ecfd0

    // ---- overrides of GameObj / UnitObjBase / Role slots ----
    virtual void SetRecord(DatRecord* record) override;                // 10, 0x50e1c0 (returns an int in the original)
    virtual int GetTransitSpeed(int arg) override;                     // 13, 0x4ed370: the hyperdrive modifier while existing
    virtual int GetEventKinds(int unused, EventKindSet* out) override;  // 15, 0x4ed350
    virtual int Virtual17() override { return 0; }                     // 17
    virtual int CanBeControlledBy(int side) override;                  // 20, 0x4ed450
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;  // 22, 0x4ed390
    virtual int Virtual44(NotifyContext* ctx) override;                // 44, 0x4ef260
    virtual int Virtual45(int a, NotifyContext* ctx) override;         // 45, 0x4ef2e0
    virtual int Virtual46(NotifyContext* ctx) override { return 1; }  // 46
    virtual int Virtual47(int a) override;                             // 47, 0x4ef250
    virtual int Virtual48(int a) override;                             // 48, 0x4ef250
    virtual GameObjData* CreateData() override;                        // 58, 0x4f24e0
    virtual int OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx) override;  // 61, 0x4f11c0
    virtual int OnUsableChanged(int newValue, NotifyContext* ctx) override;       // 68, 0x4f1310
    virtual int OnCreatedChanged(int newValue, NotifyContext* ctx) override;      // 69, 0x4f11f0
    virtual int OnDestroyedChanged(int newValue, NotifyContext* ctx) override;    // 71, 0x4f1340
    virtual int OnEnrouteChanged(int newValue, NotifyContext* ctx) override;      // 72, 0x4f12e0
    virtual int OnAutoroutingChanged(int newValue, NotifyContext* ctx) override;  // 79, 0x4f1420
    virtual int CheckActivity(const ActivityRequest* request, StatusCode* out) override;   // 112, 0x4ed460
    virtual int CheckActivity2(const ActivityRequest* request, StatusCode* out) override;  // 114, 0x4ed560
    virtual int Virtual117() override;                                 // 117, 0x4edc70: the Captured flag
    virtual int Virtual118() override { return 1; }                    // 118
    virtual int GetDiplomacy() override;                               // 119, 0x4edbf0, the Enhanced values
    virtual int GetEspionage() override;                               // 120, 0x4edc00
    virtual int GetShipyardRD() override;                              // 121, 0x4edc10
    virtual int GetTrainingFacilRD() override;                         // 122, 0x4edc20
    virtual int GetConstructionYardRD() override;                      // 123, 0x4edc30
    virtual int GetCombat() override;                                  // 124, 0x4edc40
    virtual int GetLeadership() override;                              // 125, 0x4edc50
    virtual int GetLoyalty() override;                                 // 126, 0x4edc60
    virtual int Virtual130(NotifyContext* ctx) override;               // 130, 0x4ef450
    virtual int Virtual131(int a, NotifyContext* ctx) override;        // 131, 0x4ef480
    virtual int Virtual132(int a, NotifyContext* ctx) override;        // 132, 0x4ef510
    virtual int Virtual133(int a) override;                            // 133, 0x4ef570
    virtual int Virtual134(int a) override;                            // 134, 0x4ef5b0

    // ---- overrides of Role's OnXChanged events ----
    virtual int OnBaseDiplomacyChanged(int oldValue, int newValue, NotifyContext* ctx) override;  // 135, 0x4f1520
    virtual int OnBaseEspionageChanged(int oldValue, int newValue, NotifyContext* ctx) override;  // 136, 0x4f1550
    virtual int OnBaseShipyardRDChanged(int oldValue, int newValue, NotifyContext* ctx) override;  // 137, 0x4f1580
    virtual int OnBaseTrainingFacilRDChanged(int oldValue, int newValue, NotifyContext* ctx) override;  // 138, 0x4f15b0
    virtual int OnBaseConstructionYardRDChanged(int oldValue, int newValue, NotifyContext* ctx) override;  // 139, 0x4f15e0
    virtual int OnBaseCombatChanged(int oldValue, int newValue, NotifyContext* ctx) override;  // 140, 0x4f1610
    virtual int OnBaseLeadershipChanged(int oldValue, int newValue, NotifyContext* ctx) override;  // 141, 0x4f1640
    virtual int OnBaseLoyaltyChanged(int oldValue, int newValue, NotifyContext* ctx) override;  // 142, 0x4f1670
    virtual int OnMissionChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) override;  // 143, 0x4f14d0
    virtual int OnMovingBetweenMissionsChanged(int newValue, NotifyContext* ctx) override;  // 148, 0x4f1470
    virtual int OnAdriftChanged(int newValue, NotifyContext* ctx) override;  // 153, 0x4f1440
    virtual int OnOnMissionChanged(int newValue, NotifyContext* ctx) override;  // 154, 0x4f1490

    // ---- new non-property virtuals (slots 179-190; names provisional) ----
    virtual int Virtual179(int a, int* out);                                              // 179, 0x4ed680
    virtual int CheckActivity3(const ActivityRequest* request, StatusCode* out);          // 180, 0x4ed750
    virtual int CheckActivityStatus3(const ActivityRequest* request, StatusCode* out);    // 181, 0x4ed950: out = {1,-1}, returns 180's result
    virtual int CheckActivity4(const ActivityRequest* request, StatusCode* out);          // 182, 0x4ed9d0
    virtual int CheckActivityStatus4(const ActivityRequest* request, StatusCode* out);    // 183, 0x4edb70: out = {1,-1}, returns 182's result
    virtual int Virtual184() { return 0; }                                                // 184
    virtual int Virtual185(NotifyContext* ctx);                                          // 185, 0x4ef5f0
    virtual int Virtual186(NotifyContext* ctx);                                          // 186, 0x4ef640
    virtual int Virtual187(NotifyContext* ctx);                                          // 187, 0x4ef690
    virtual int Virtual188(int a, int b, NotifyContext* ctx);                             // 188, 0x4ef6d0
    virtual int Virtual189(int a, int b, int c);                                          // 189, 0x4ef340
    virtual int Virtual190(int a, int b, NotifyContext* ctx);                            // 190, 0x4ef400

    // ---- OnXChanged events on the master (slots 191-220) ----
    virtual int OnEnhancedDiplomacyChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 191
    virtual int OnEnhancedEspionageChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 192
    virtual int OnEnhancedShipyardRDChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 193
    virtual int OnEnhancedTrainingFacilRDChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 194
    virtual int OnEnhancedConstructionYardRDChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 195
    virtual int OnEnhancedCombatChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 196
    virtual int OnEnhancedLeadershipChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 197
    virtual int OnEnhancedLoyaltyChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 198
    virtual int OnForceChanged(int oldValue, int newValue, NotifyContext* ctx);  // 199, 0x4f16a0
    virtual int OnForceExperienceChanged(int oldValue, int newValue, NotifyContext* ctx);  // 200, 0x4f1700
    virtual int OnForceTrainingChanged(int oldValue, int newValue, NotifyContext* ctx);  // 201, 0x4f1700
    virtual int OnLeadershipAdjustmentChanged(int oldValue, int newValue, NotifyContext* ctx);  // 202, 0x4f1710
    virtual int OnInjuryChanged(int oldValue, int newValue, NotifyContext* ctx);  // 203, 0x4f1720
    virtual int OnCommandKindChanged(int oldValue, int newValue, NotifyContext* ctx);  // 204, 0x4f17f0
    virtual int OnStateChanged(int oldValue, int newValue, NotifyContext* ctx);  // 205, 0x4f18e0
    virtual int OnMissionHyperdriveModifierChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 206
    virtual int OnEncounterChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);  // 207, 0x4f19e0
    virtual int OnTraitorDiscoveredChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) { return 1; }  // 208
    virtual int OnForceUserDiscoveredChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) { return 1; }  // 209
    virtual int OnCommandingChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) { return 1; }  // 210
    virtual int OnCapturedChanged(int newValue, NotifyContext* ctx);  // 211, 0x4f1a80
    virtual int OnCanHealChanged(int newValue, NotifyContext* ctx);  // 212, 0x4f1ae0
    virtual int OnFastHealChanged(int newValue, NotifyContext* ctx);  // 213, 0x4f1af0
    virtual int OnForceAwareChanged(int newValue, NotifyContext* ctx);  // 214, 0x4f1b30
    virtual int OnForcePotentialChanged(int newValue, NotifyContext* ctx);  // 215, 0x4f1b30 (same body as the neighbour)
    virtual int OnHealingChanged(int newValue, NotifyContext* ctx);  // 216, 0x4f1b40
    virtual int OnDiscoveringForceUserChanged(int newValue, NotifyContext* ctx);  // 217, 0x4f1b70
    virtual int OnCanEscapeChanged(int newValue, NotifyContext* ctx);  // 218, 0x4f1ba0
    virtual int OnEscapeRequestChanged(int newValue, NotifyContext* ctx);  // 219, 0x4f1bd0
    virtual int OnEscapeAttemptChanged(int newValue, NotifyContext* ctx);  // 220, 0x4f1be0
    // 200 (ForceExperience) and 201 (ForceTraining) share 0x4f1700, 214 (ForceAware) and 215 (ForcePotential) share 0x4f1b30.

    // ---- OnXNotified events on a side view (slots 221-250) ----
    virtual int OnEnhancedDiplomacyNotified(int oldValue, int newValue, NotifyContext* ctx);  // 221, 0x4f1c00
    virtual int OnEnhancedEspionageNotified(int oldValue, int newValue, NotifyContext* ctx);  // 222, 0x4f1c40
    virtual int OnEnhancedShipyardRDNotified(int oldValue, int newValue, NotifyContext* ctx);  // 223, 0x4f1c80
    virtual int OnEnhancedTrainingFacilRDNotified(int oldValue, int newValue, NotifyContext* ctx);  // 224, 0x4f1cc0
    virtual int OnEnhancedConstructionYardRDNotified(int oldValue, int newValue, NotifyContext* ctx);  // 225, 0x4f1d00
    virtual int OnEnhancedCombatNotified(int oldValue, int newValue, NotifyContext* ctx);  // 226, 0x4f1d40
    virtual int OnEnhancedLeadershipNotified(int oldValue, int newValue, NotifyContext* ctx);  // 227, 0x4f1d80
    virtual int OnEnhancedLoyaltyNotified(int oldValue, int newValue, NotifyContext* ctx);  // 228, 0x4f1dc0
    virtual int OnForceNotified(int oldValue, int newValue, NotifyContext* ctx);  // 229, 0x4f1e00
    virtual int OnForceExperienceNotified(int oldValue, int newValue, NotifyContext* ctx);  // 230, 0x4f1e50
    virtual int OnForceTrainingNotified(int oldValue, int newValue, NotifyContext* ctx);  // 231, 0x4f1ea0
    virtual int OnLeadershipAdjustmentNotified(int oldValue, int newValue, NotifyContext* ctx);  // 232, 0x4f1ef0
    virtual int OnInjuryNotified(int oldValue, int newValue, NotifyContext* ctx);  // 233, 0x4f1f30
    virtual int OnCommandKindNotified(int oldValue, int newValue, NotifyContext* ctx);  // 234, 0x4f1f80
    virtual int OnStateNotified(int oldValue, int newValue, NotifyContext* ctx);  // 235, 0x4f1fd0
    virtual int OnMissionHyperdriveModifierNotified(int oldValue, int newValue, NotifyContext* ctx);  // 236, 0x4f2000
    virtual int OnEncounterNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);  // 237, 0x4f2040
    virtual int OnTraitorDiscoveredNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);  // 238, 0x4f2090
    virtual int OnForceUserDiscoveredNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);  // 239, 0x4f20e0
    virtual int OnCommandingNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);  // 240, 0x4f2130
    virtual int OnCapturedNotified(int value, NotifyContext* ctx);  // 241, 0x4f2180
    virtual int OnCanHealNotified(int value, NotifyContext* ctx);  // 242, 0x4f21c0
    virtual int OnFastHealNotified(int value, NotifyContext* ctx);  // 243, 0x4f2200
    virtual int OnForceAwareNotified(int value, NotifyContext* ctx);  // 244, 0x4f2240
    virtual int OnForcePotentialNotified(int value, NotifyContext* ctx);  // 245, 0x4f2280
    virtual int OnHealingNotified(int value, NotifyContext* ctx);  // 246, 0x4f22c0
    virtual int OnDiscoveringForceUserNotified(int value, NotifyContext* ctx);  // 247, 0x4f2300
    virtual int OnCanEscapeNotified(int value, NotifyContext* ctx);  // 248, 0x4f2340
    virtual int OnEscapeRequestNotified(int value, NotifyContext* ctx);  // 249, 0x4f2380
    virtual int OnEscapeAttemptNotified(int value, NotifyContext* ctx);  // 250, 0x4f23c0

    // ---- validated setters (master view only) and Apply functions (side views) ----
    int SetEnhancedDiplomacy(int value, NotifyContext* ctx);  // 0x4edcb0
    int SetEnhancedEspionage(int value, NotifyContext* ctx);  // 0x4edd30
    int SetEnhancedShipyardRD(int value, NotifyContext* ctx);  // 0x4eddb0
    int SetEnhancedTrainingFacilRD(int value, NotifyContext* ctx);  // 0x4ede30
    int SetEnhancedConstructionYardRD(int value, NotifyContext* ctx);  // 0x4edeb0
    int SetEnhancedCombat(int value, NotifyContext* ctx);  // 0x4edf30
    int SetEnhancedLeadership(int value, NotifyContext* ctx);  // 0x4edfb0
    int SetEnhancedLoyalty(int value, NotifyContext* ctx);  // 0x4ee030
    int SetForce(int value, NotifyContext* ctx);  // 0x4ee0d0
    int SetForceExperience(int value, NotifyContext* ctx);  // 0x4ee150
    int SetForceTraining(int value, NotifyContext* ctx);  // 0x4ee1d0
    int SetLeadershipAdjustment(int value, NotifyContext* ctx);  // 0x4ee250
    int SetInjury(int value, NotifyContext* ctx);  // 0x4ee2d0
    int SetCommandKind(int value, NotifyContext* ctx);  // 0x4ee350
    int SetState(int value, NotifyContext* ctx);  // 0x4ee3e0
    int SetMissionHyperdriveModifier(int value, NotifyContext* ctx);  // 0x4ee470
    int SetEncounterKey(ObjectKey* value, NotifyContext* ctx);  // 0x4ee500
    int SetTraitorDiscoveredKey(ObjectKey* value, NotifyContext* ctx);  // 0x4ee5c0
    int SetForceUserDiscoveredKey(ObjectKey* value, NotifyContext* ctx);  // 0x4ee680
    int SetCommandingKey(ObjectKey* value, NotifyContext* ctx);  // 0x4ee740
    int SetCaptured(int on, NotifyContext* ctx);  // 0x4ee800
    int SetCanHeal(int on, NotifyContext* ctx);  // 0x4ee870
    int SetFastHeal(int on, NotifyContext* ctx);  // 0x4ee8e0
    int SetForceAware(int on, NotifyContext* ctx);  // 0x4ee950
    int SetForcePotential(int on, NotifyContext* ctx);  // 0x4ee9c0
    int SetHealing(int on, NotifyContext* ctx);  // 0x4eea30
    int SetDiscoveringForceUser(int on, NotifyContext* ctx);  // 0x4eeaa0
    int SetCanEscape(int on, NotifyContext* ctx);  // 0x4eeb10
    int SetEscapeRequest(int on, NotifyContext* ctx);  // 0x4eeb80
    int SetEscapeAttempt(int on, NotifyContext* ctx);  // 0x4eebf0
    int ApplyEnhancedDiplomacy(NotifyContext* ctx);  // 0x4efdc0
    int ApplyEnhancedEspionage(NotifyContext* ctx);  // 0x4efe40
    int ApplyEnhancedShipyardRD(NotifyContext* ctx);  // 0x4efec0
    int ApplyEnhancedTrainingFacilRD(NotifyContext* ctx);  // 0x4eff50
    int ApplyEnhancedConstructionYardRD(NotifyContext* ctx);  // 0x4effe0
    int ApplyEnhancedCombat(NotifyContext* ctx);  // 0x4f0070
    int ApplyEnhancedLeadership(NotifyContext* ctx);  // 0x4f0100
    int ApplyEnhancedLoyalty(NotifyContext* ctx);  // 0x4f0190
    int ApplyForce(NotifyContext* ctx);  // 0x4f0220
    int ApplyForceExperience(NotifyContext* ctx);  // 0x4f02b0
    int ApplyForceTraining(NotifyContext* ctx);  // 0x4f0340
    int ApplyLeadershipAdjustment(NotifyContext* ctx);  // 0x4f03d0
    int ApplyInjury(NotifyContext* ctx);  // 0x4f0460
    int ApplyCommandKind(NotifyContext* ctx);  // 0x4f04f0
    int ApplyState(NotifyContext* ctx);  // 0x4f0580
    int ApplyMissionHyperdriveModifier(NotifyContext* ctx);  // 0x4f0610
    int ApplyEncounterKey(NotifyContext* ctx);  // 0x4f06a0
    int ApplyTraitorDiscoveredKey(NotifyContext* ctx);  // 0x4f07f0
    int ApplyForceUserDiscoveredKey(NotifyContext* ctx);  // 0x4f0940
    int ApplyCommandingKey(NotifyContext* ctx);  // 0x4f0a90
    int ApplyCaptured(NotifyContext* ctx);  // 0x4f0be0
    int ApplyCanHeal(NotifyContext* ctx);  // 0x4f0c70
    int ApplyFastHeal(NotifyContext* ctx);  // 0x4f0d00
    int ApplyForceAware(NotifyContext* ctx);  // 0x4f0d90
    int ApplyForcePotential(NotifyContext* ctx);  // 0x4f0e20
    int ApplyHealing(NotifyContext* ctx);  // 0x4f0eb0
    int ApplyDiscoveringForceUser(NotifyContext* ctx);  // 0x4f0f40
    int ApplyCanEscape(NotifyContext* ctx);  // 0x4f0fe0
    int ApplyEscapeRequest(NotifyContext* ctx);  // 0x4f1080
    int ApplyEscapeAttempt(NotifyContext* ctx);  // 0x4f1120

    // ---- persistence/validation: bodies the leaf classes call qualified (the Role slots 5-9 stay pure) ----
    virtual int Read(istream* in) override = 0;               // 5, body 0x4ef7e0
    virtual int Write(ostream* out) override = 0;             // 6, body 0x4ef940
    virtual int Checksum(Crc32* crc) override = 0;            // 7, body 0x4efbc0
    virtual int Validate() override = 0;                      // 8, body 0x4efd30
    virtual int ApplyAll(NotifyContext* ctx) override = 0;    // 9, body 0x4efaa0
    // The body of slot 37 (CreateViews) that the leaf classes call: runs 0x5774f0 and then initializes the data block's
    // stats and the DiscoveringForceUser flag.
    int CreateViewsImpl(int controlKind, NotifyContext* ctx);  // 0x4ef750
    // The event-kind set of Character (g_characterEventKinds, 0x6b2940); 0x4f2400 builds it on first use, 0x4f24c0 is the
    // static initializer's entry used by the leaf classes, 0x4f2550 registers the destructor (static init).
    static int BuildEventKinds();                                  // 0x4f2400
    static int InitEventKinds(int unused, EventKindSet* out);      // 0x4f24c0
    static void InitStatics();                                     // 0x4f2550

    // ---- other non-virtual members: added by the implementer of 0x4eceb0-0x4f26d0 (keep this marker) ----
    // The object's copy in `view` (0 master, 1 Alliance, 2 Empire).
    Character* ViewAsCharacter(int view) { return static_cast<Character*>(ViewAs(view)); }
    // The SD record (MJCHARSD/MNCHARSD share one layout) or null.
    MajorCharacterSD* GetCharacterRecord() const;
    // Record flags: whether the character may take the Admiral / Commander / General command (0 without a record).
    int RecordCanBeAdmiral();        // 0x4ed1c0, record +0xa8
    int RecordCanBeCommander();      // 0x4ed1e0, record +0xac
    int RecordCanBeGeneral();        // 0x4ed200, record +0xb0
    int RecordIsJediTrainer();       // 0x4ed220, record +0xb8
    int RecordIsUnableToBetray();    // 0x4ed240, record +0xb4
    // The side that may control the character: 1 Alliance only, 2 Empire only, 0 for neither, both or no record.
    int GetRecordSide();             // 0x4edc80
    // *out = RecordIsJediTrainer() && HasForcePotentialA15(Force). Returns 1.
    int CanTrainForce(int* out);     // 0x4ed310
    // The data block's holder key (a fresh unset key without a data block) and its setter/getter pair.
    ObjectKey* GetHolderKey(ObjectKey* out);  // 0x4ed270
    int GetDataField34();                     // 0x4ed300
    int SetHolderKey(const ObjectKey* key);   // 0x4ef190, master only (the key is stored regardless)
    void SetDataField34(int value);           // 0x4ef1c0
    // Getters of the four keys (copy into *out).
    ObjectKey* GetEncounterKey(ObjectKey* out);            // 0x4f07c0
    ObjectKey* GetTraitorDiscoveredKey(ObjectKey* out);    // 0x4f0910
    ObjectKey* GetForceUserDiscoveredKey(ObjectKey* out);  // 0x4f0a60
    ObjectKey* GetCommandingKey(ObjectKey* out);           // 0x4f0bb0
    // Derived values: each recomputes the property from the others and calls its validated setter. Most return the
    // setter's result (the failure of an intermediate step ends the chain).
    int UpdateControlKind(NotifyContext* ctx);              // 0x4eec60, the record side, swapped while Captured
    int UpdateEnhancedDiplomacy(NotifyContext* ctx);        // 0x4eecb0, base skill + Force
    int UpdateEnhancedEspionage(NotifyContext* ctx);        // 0x4eecf0
    int UpdateEnhancedShipyardRD(NotifyContext* ctx);       // 0x4eed30, copies the base value
    int UpdateEnhancedTrainingFacilRD(NotifyContext* ctx);  // 0x4eed50
    int UpdateEnhancedConstructionYardRD(NotifyContext* ctx);  // 0x4eed70
    int UpdateEnhancedCombat(NotifyContext* ctx);           // 0x4eed90, base skill + Force - Injury
    int UpdateEnhancedLeadership(NotifyContext* ctx);       // 0x4eede0, base skill + LeadershipAdjustment
    int UpdateEnhancedLoyalty(NotifyContext* ctx);          // 0x4eee20, copies the base value
    int UpdateForce(NotifyContext* ctx);                    // 0x4eee40, ForceExperience + ForceTraining
    int UpdateForceExperience(NotifyContext* ctx);          // 0x4eee60, rolls the initial value once for a new Force user
    int UpdateCommandKind(NotifyContext* ctx);              // 0x4eeed0, clears the command when it can no longer be held
    int UpdateCaptured(NotifyContext* ctx);                 // 0x4eef70, Captured = (State == 0)
    int UpdateCanHeal(NotifyContext* ctx);                  // 0x4eef90
    int UpdateFastHeal(NotifyContext* ctx);                 // 0x4eeff0
    int UpdateHealing(NotifyContext* ctx);                  // 0x4ef030
    int UpdateDiscoveringForceUser(NotifyContext* ctx);     // 0x4ef070
    int RaiseEscapeRequest(NotifyContext* ctx);             // 0x4ef0c0, runs slot 133 for a captured, usable, idle character
    int RecoverFromInjury(NotifyContext* ctx);              // 0x4ef100, Injury -= the per-tick recovery
    int PulseEscapeAttempt(NotifyContext* ctx);             // 0x4ef150, EscapeAttempt on, then off again
    // Loyalty += amount for `side` == the record side, -= amount otherwise (never for side 3 or unable-to-betray ones).
    int AdjustLoyaltyForSide(int amount, int side, NotifyContext* ctx);  // 0x4ef1e0

    // ---- fields ----
    int16_t m_enhancedDiplomacy;            // +0x7c
    int16_t m_enhancedEspionage;            // +0x7e
    int16_t m_enhancedShipyardRD;           // +0x80
    int16_t m_enhancedTrainingFacilRD;      // +0x82
    int16_t m_enhancedConstructionYardRD;   // +0x84
    int16_t m_enhancedCombat;               // +0x86
    int16_t m_enhancedLeadership;           // +0x88
    int16_t m_enhancedLoyalty;              // +0x8a
    int16_t m_force;                        // +0x8c
    int16_t m_forceExperience;              // +0x8e
    int16_t m_forceTraining;                // +0x90
    int16_t m_leadershipAdjustment;         // +0x92
    int16_t m_injury;                       // +0x94
    int16_t m_commandKind;                  // +0x96
    int16_t m_state;                        // +0x98
    int16_t m_missionHyperdriveModifier;    // +0x9a
    ObjectKey m_encounterKey;               // +0x9c
    ObjectKey m_traitorDiscoveredKey;       // +0xa0
    ObjectKey m_forceUserDiscoveredKey;     // +0xa4
    ObjectKey m_commandingKey;              // +0xa8
    uint32_t m_flagsAc;                     // +0xac, ten flags (Captured = 1, CanHeal = 2, FastHeal = 4, ...)
};

// Mission-member bit of a character (0x10000) used by the mission member-mask builder (0x582fb0).
int GetCharacterMemberMask();  // 0x4ed260
// Static initializer pieces of the event-kind set (see Character::InitStatics).
void ConstructCharacterEventKinds();        // 0x4f2560
void RegisterCharacterEventKindsAtExit();   // 0x4f2570

// The AppendCharacterName free function (secondary vtable slot, takes the ObjectBase subobject).
void __thiscall AppendCharacterName(ObjectBase* base, String* out, int kind);  // 0x4ed060

// Iterator over the children of a game object in the character families 0x30-0x3b (vtable 0x65ce5c).
class CharacterIterator : public ChildIterator {
public:
    CharacterIterator(GameObjNotifier* parent, uint32_t stateFilter);                 // 0x4f25c0
    CharacterIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side);  // 0x4f2640
    virtual ~CharacterIterator();                                                     // 0x4f25a0 (deleting), 0x4f26c0
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(CharacterData, 0x38);
REB_ASSERT_SIZE(Character, 0xb0);
REB_ASSERT_OFFSET(Character, m_enhancedDiplomacy, 0x7c);
REB_ASSERT_OFFSET(Character, m_force, 0x8c);
REB_ASSERT_OFFSET(Character, m_injury, 0x94);
REB_ASSERT_OFFSET(Character, m_missionHyperdriveModifier, 0x9a);
REB_ASSERT_OFFSET(Character, m_encounterKey, 0x9c);
REB_ASSERT_OFFSET(Character, m_commandingKey, 0xa8);
REB_ASSERT_OFFSET(Character, m_flagsAc, 0xac);
REB_ASSERT_SIZE(CharacterIterator, 0x20);
#pragma clang diagnostic pop

