#pragma once
#include <cstdint>

#include "core/layout.h"
#include "data/dat_record.h"
#include "gameobj/child_iterator.h"
#include "gameobj/unit_obj_base.h"

// Role (VT 0x660d60, secondary ObjectBase VT 0x660d5c, 0x7c bytes) is the base of every "person" object: the
// characters (family 0x30-0x3b, class Character) and the special forces (0x3c-0x3f, class SpecialForces). It sits on
// UnitObjBase and adds the 8 base skill values, four mission keys, ten mission flags and the mission-related checks.
// Strings: "Role*Notif" (the OnXNotified events), "Invalid BaseLoyalty value!".
//
// Vtable: 179 slots. Slots 0-111 are UnitObjBase's; 112-134 are new non-property virtuals (names are provisional,
// "VirtualNNN" where the purpose is unknown), 135-156 are the OnXChanged events of the 22 Role properties and 157-178
// the OnXNotified events, in this order (the same order as the fields and the Set/Apply functions):
//   8 skills   BaseDiplomacy +0x58, BaseEspionage +0x5a, BaseShipyardRD +0x5c, BaseTrainingFacilRD +0x5e,
//              BaseConstructionYardRD +0x60, BaseCombat +0x62, BaseLeadership +0x64, BaseLoyalty +0x66 (0..100)
//   4 keys     MissionKey +0x68, MissionSeedKey +0x6c (NullableKey), ParentAtMissionCompletionKey +0x70,
//              LocationAtMissionCompletionKey +0x74
//   10 flags   m_flags78 bit i (mask 1 << i): Decoy, MovingBetweenMissions, MissionRemoveRequest,
//              MissionResignRequest, CanResignFromMission, IsDecoying, Adrift, OnMission (0x80), OnHiddenMission,
//              OnMandatoryMission
// Property pattern as in GameObj: SetX (master only, validates, stores, view1->ApplyX, view2->ApplyX, OnXChanged),
// ApplyX (copies master -> side view, raises OnXNotified).
// Pure slots left to the leaf classes: 1 (GetFamily), 5-9, 37, 57, and 119-127 (the effective skill values and 127).
// 5-9 have bodies here (Read 0x534c60, Write 0x534d20, Checksum 0x534ed0, Validate 0x534fa0, ApplyAll 0x534de0) that
// the leaf classes call qualified.

class Crc32;
class EventKindSet;

// Bits of Role::m_flags78.
namespace role_flag {
constexpr uint32_t kDecoy = 0x1;
constexpr uint32_t kMovingBetweenMissions = 0x2;
constexpr uint32_t kMissionRemoveRequest = 0x4;
constexpr uint32_t kMissionResignRequest = 0x8;
constexpr uint32_t kCanResignFromMission = 0x10;
constexpr uint32_t kIsDecoying = 0x20;
constexpr uint32_t kAdrift = 0x40;
constexpr uint32_t kOnMission = 0x80;
constexpr uint32_t kOnHiddenMission = 0x100;
constexpr uint32_t kOnMandatoryMission = 0x200;
}  // namespace role_flag

// The SD record of a person: {base, random spread} for each of the 8 skills; Role::OnCreatedChanged rolls the base
// skills from them. Abstract here (the concrete record classes belong to the data module).
struct RoleSkillRange {
    int m_base;    // +0x00
    int m_spread;  // +0x04
};

class RoleRecord : public SdRecord {
public:
    RoleSkillRange m_skills[8];  // +0x58: Diplomacy, Espionage, ShipyardRD, TrainingFacilRD, ConstructionYardRD, Combat, Leadership, Loyalty
};

class Role : public UnitObjBase {
public:
    Role();                                                           // 0x5336b0
    virtual ~Role();                                                  // 0x533750 (deleting), 0x533770

    // ---- overrides of GameObj / UnitObjBase slots ----
    virtual int GetTransitSpeed(int arg) override;                                                 // 13, 0x5338a0
    virtual int GetEventKinds(int unused, EventKindSet* out) override;                             // 15, 0x5338b0
    virtual int IsObservable() override { return 0; }                                              // 19
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;                        // 22, 0x5338d0
    virtual int CheckRequestImpl(const ActivityRequest* request, StatusCode* out) override;        // 23, 0x533990
    virtual int CheckRenameImpl(const ActivityRequest* request, StatusCode* out) override;                                  // 25, 0x533a00
    virtual int CheckUnknownImpl(const ActivityRequest* request, StatusCode* out) override;                                 // 27, 0x533a70
    virtual int CheckUnknown(const ActivityRequest* request, StatusCode* out) override;                                     // 28, 0x533ae0
    virtual int Virtual47(intptr_t a) override;                                                         // 47, 0x534a50
    virtual int Virtual48(intptr_t a) override;                                                         // 48, 0x534a50
    // `a` is really the NotifyContext*: clears the master's MissionKey, then re-applies it to both side views.
    virtual int Virtual55(intptr_t a) override;                                                         // 55, 0x534a60
    virtual GameObjData* CreateData() override;                                                    // 58, 0x536cc0
    virtual int OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx) override;  // 61, 0x536070
    virtual int OnUsableChanged(int newValue, NotifyContext* ctx) override;                        // 68, 0x536050
    virtual int OnCreatedChanged(int newValue, NotifyContext* ctx) override;                       // 69, 0x535e40
    virtual int OnDestroyedChanged(int newValue, NotifyContext* ctx) override;                     // 71, 0x536110
    virtual int OnEnrouteChanged(int newValue, NotifyContext* ctx) override;                       // 72, 0x5361e0
    virtual int OnAutoroutingChanged(int newValue, NotifyContext* ctx) override;                   // 79, 0x536200
    virtual int OnEnrouteActiveNotified(int value, NotifyContext* ctx) override;                   // 99, 0x5365b0

    // ---- the persistence/validation entry points: pure here, with bodies the leaf classes call qualified ----
    virtual int Read(istream* in) override = 0;                   // 5, body 0x534c60
    virtual int Write(ostream* out) override = 0;                 // 6, body 0x534d20
    virtual int Checksum(Crc32* crc) override = 0;                // 7, body 0x534ed0
    virtual int Validate() override = 0;                          // 8, body 0x534fa0
    virtual int ApplyAll(NotifyContext* ctx) override = 0;        // 9, body 0x534de0


    // ---- new non-property virtuals (slots 112-134) ----
    // Both activity checks run GameObj::CheckActivityRequest and add the status {0x30, 1} (busy on a mission) when the
    // OnMission flag is set.
    virtual int CheckActivity(const ActivityRequest* request, StatusCode* out);           // 112, 0x533ba0
    virtual int CheckActivityStatus(const ActivityRequest* request, StatusCode* out);     // 113, 0x533c60: out = {1,-1}, CheckActivity
    virtual int CheckActivity2(const ActivityRequest* request, StatusCode* out);          // 114, 0x533ce0, same body as 112
    virtual int CheckActivityStatus2(const ActivityRequest* request, int unused, StatusCode* out);  // 115, 0x533da0: out = {1,-1}, CheckActivity2
    virtual int Virtual116() { return 1; }                                                // 116
    virtual int Virtual117() { return 0; }                                                // 117
    virtual int Virtual118() { return 0; }                                                // 118
    // Effective skill values (the Character's are the "Enhanced" ones, the special forces' the base ones).
    virtual int GetDiplomacy() = 0;                                                       // 119
    virtual int GetEspionage() = 0;                                                       // 120
    virtual int GetShipyardRD() = 0;                                                      // 121
    virtual int GetTrainingFacilRD() = 0;                                                 // 122
    virtual int GetConstructionYardRD() = 0;                                              // 123
    virtual int GetCombat() = 0;                                                          // 124
    virtual int GetLeadership() = 0;                                                      // 125
    virtual int GetLoyalty() = 0;                                                         // 126
    virtual int Virtual127() = 0;                                                         // 127
    // `a` is an ActivityRequest*: CheckActivityStatus, then Virtual129 if it left the status at {1,-1}.
    virtual int Virtual128(const ActivityRequest* request, NotifyContext* ctx);                                    // 128, 0x534b70
    virtual int Virtual129(NotifyContext* ctx);                                           // 129, 0x534c00 (same body as Destroy: reason 0x14)
    // Requests the resignation from the mission (SetMissionResignRequest(1)) if CanResignFromMission is set and there is a mission.
    virtual int Virtual130(NotifyContext* ctx);                                           // 130, 0x534c20
    virtual int Virtual131(const ObjectKey* opponent, NotifyContext* ctx) { return 1; }                       // 131
    virtual int Virtual132(const ObjectKey* holder, NotifyContext* ctx) { return 1; }                       // 132
    virtual int Virtual133(NotifyContext* ctx) { return 1; }                                           // 133
    virtual int Virtual134(NotifyContext* ctx) { return 1; }                                           // 134

    // ---- OnXChanged events on the master (slots 135-156) ----
    virtual int OnBaseDiplomacyChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 135
    virtual int OnBaseEspionageChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 136
    virtual int OnBaseShipyardRDChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 137
    virtual int OnBaseTrainingFacilRDChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 138
    virtual int OnBaseConstructionYardRDChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 139
    virtual int OnBaseCombatChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 140
    virtual int OnBaseLeadershipChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 141
    virtual int OnBaseLoyaltyChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 142
    virtual int OnMissionChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);  // 143, 0x536220
    virtual int OnMissionSeedChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) { return 1; }  // 144
    virtual int OnParentAtMissionCompletionChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);  // 145, 0x536300
    virtual int OnLocationAtMissionCompletionChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) { return 1; }  // 146
    virtual int OnDecoyChanged(int newValue, NotifyContext* ctx) { return 1; }  // 147
    virtual int OnMovingBetweenMissionsChanged(int newValue, NotifyContext* ctx);  // 148, 0x5363b0
    virtual int OnMissionRemoveRequestChanged(int newValue, NotifyContext* ctx);  // 149, 0x5363d0
    virtual int OnMissionResignRequestChanged(int newValue, NotifyContext* ctx);  // 150, 0x536430
    virtual int OnCanResignFromMissionChanged(int newValue, NotifyContext* ctx) { return 1; }  // 151
    virtual int OnIsDecoyingChanged(int newValue, NotifyContext* ctx) { return 1; }  // 152
    virtual int OnAdriftChanged(int newValue, NotifyContext* ctx);  // 153, 0x536460
    virtual int OnOnMissionChanged(int newValue, NotifyContext* ctx);  // 154, 0x536490
    virtual int OnOnHiddenMissionChanged(int newValue, NotifyContext* ctx) { return 1; }  // 155
    virtual int OnOnMandatoryMissionChanged(int newValue, NotifyContext* ctx) { return 1; }  // 156

    // ---- OnXNotified events on a side view (slots 157-178) ----
    virtual int OnBaseDiplomacyNotified(int oldValue, int newValue, NotifyContext* ctx);  // 157, 0x536600
    virtual int OnBaseEspionageNotified(int oldValue, int newValue, NotifyContext* ctx);  // 158, 0x536640
    virtual int OnBaseShipyardRDNotified(int oldValue, int newValue, NotifyContext* ctx);  // 159, 0x536680
    virtual int OnBaseTrainingFacilRDNotified(int oldValue, int newValue, NotifyContext* ctx);  // 160, 0x5366c0
    virtual int OnBaseConstructionYardRDNotified(int oldValue, int newValue, NotifyContext* ctx);  // 161, 0x536700
    virtual int OnBaseCombatNotified(int oldValue, int newValue, NotifyContext* ctx);  // 162, 0x536740
    virtual int OnBaseLeadershipNotified(int oldValue, int newValue, NotifyContext* ctx);  // 163, 0x536780
    virtual int OnBaseLoyaltyNotified(int oldValue, int newValue, NotifyContext* ctx);  // 164, 0x5367c0
    virtual int OnMissionNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);  // 165, 0x536800
    virtual int OnMissionSeedNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);  // 166, 0x536880
    virtual int OnParentAtMissionCompletionNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);  // 167, 0x5368c0
    virtual int OnLocationAtMissionCompletionNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);  // 168, 0x536900
    virtual int OnDecoyNotified(int value, NotifyContext* ctx);  // 169, 0x536940
    virtual int OnMovingBetweenMissionsNotified(int value, NotifyContext* ctx);  // 170, 0x536980
    virtual int OnMissionRemoveRequestNotified(int value, NotifyContext* ctx);  // 171, 0x5369c0
    virtual int OnMissionResignRequestNotified(int value, NotifyContext* ctx);  // 172, 0x536a00
    virtual int OnCanResignFromMissionNotified(int value, NotifyContext* ctx);  // 173, 0x536a40
    virtual int OnIsDecoyingNotified(int value, NotifyContext* ctx);  // 174, 0x536a80
    virtual int OnAdriftNotified(int value, NotifyContext* ctx);  // 175, 0x536ac0
    virtual int OnOnMissionNotified(int value, NotifyContext* ctx);  // 176, 0x536b00
    virtual int OnOnHiddenMissionNotified(int value, NotifyContext* ctx);  // 177, 0x536b40
    virtual int OnOnMandatoryMissionNotified(int value, NotifyContext* ctx);  // 178, 0x536b80

    // ---- validated setters (master view only) and Apply functions (side views) ----
    int SetBaseDiplomacy(int value, NotifyContext* ctx);  // 0x533e20
    int SetBaseEspionage(int value, NotifyContext* ctx);  // 0x533ea0
    int SetBaseShipyardRD(int value, NotifyContext* ctx);  // 0x533f20
    int SetBaseTrainingFacilRD(int value, NotifyContext* ctx);  // 0x533fa0
    int SetBaseConstructionYardRD(int value, NotifyContext* ctx);  // 0x534020
    int SetBaseCombat(int value, NotifyContext* ctx);  // 0x5340a0
    int SetBaseLeadership(int value, NotifyContext* ctx);  // 0x534120
    int SetBaseLoyalty(int value, NotifyContext* ctx);  // 0x5341a0
    int SetMissionKey(ObjectKey* value, NotifyContext* ctx);  // 0x534230
    int SetMissionSeedKey(ObjectKey* value, NotifyContext* ctx);  // 0x5342e0
    int SetParentAtMissionCompletionKey(ObjectKey* value, NotifyContext* ctx);  // 0x534390
    int SetLocationAtMissionCompletionKey(ObjectKey* value, NotifyContext* ctx);  // 0x534440
    int SetDecoy(int on, NotifyContext* ctx);  // 0x5344f0
    int SetMovingBetweenMissions(int on, NotifyContext* ctx);  // 0x534560
    int SetMissionRemoveRequest(int on, NotifyContext* ctx);  // 0x5345d0
    int SetMissionResignRequest(int on, NotifyContext* ctx);  // 0x534640
    int SetCanResignFromMission(int on, NotifyContext* ctx);  // 0x5346b0
    int SetIsDecoying(int on, NotifyContext* ctx);  // 0x534720
    int SetAdrift(int on, NotifyContext* ctx);  // 0x534790
    int SetOnMission(int on, NotifyContext* ctx);  // 0x534800
    int SetOnHiddenMission(int on, NotifyContext* ctx);  // 0x534870
    int SetOnMandatoryMission(int on, NotifyContext* ctx);  // 0x5348e0
    int ApplyBaseDiplomacy(NotifyContext* ctx);  // 0x535000
    int ApplyBaseEspionage(NotifyContext* ctx);  // 0x535080
    int ApplyBaseShipyardRD(NotifyContext* ctx);  // 0x535100
    int ApplyBaseTrainingFacilRD(NotifyContext* ctx);  // 0x535180
    int ApplyBaseConstructionYardRD(NotifyContext* ctx);  // 0x535200
    int ApplyBaseCombat(NotifyContext* ctx);  // 0x535280
    int ApplyBaseLeadership(NotifyContext* ctx);  // 0x535300
    int ApplyBaseLoyalty(NotifyContext* ctx);  // 0x535380
    int ApplyMissionKey(NotifyContext* ctx);  // 0x535400
    int ApplyMissionSeedKey(NotifyContext* ctx);  // 0x535520
    int ApplyParentAtMissionCompletionKey(NotifyContext* ctx);  // 0x535640
    int ApplyLocationAtMissionCompletionKey(NotifyContext* ctx);  // 0x535780
    int ApplyDecoy(NotifyContext* ctx);  // 0x5358a0
    int ApplyMovingBetweenMissions(NotifyContext* ctx);  // 0x535930
    int ApplyMissionRemoveRequest(NotifyContext* ctx);  // 0x5359c0
    int ApplyMissionResignRequest(NotifyContext* ctx);  // 0x535a50
    int ApplyCanResignFromMission(NotifyContext* ctx);  // 0x535ae0
    int ApplyIsDecoying(NotifyContext* ctx);  // 0x535b70
    int ApplyAdrift(NotifyContext* ctx);  // 0x535c00
    int ApplyOnMission(NotifyContext* ctx);  // 0x535c90
    int ApplyOnHiddenMission(NotifyContext* ctx);  // 0x535d20
    int ApplyOnMandatoryMission(NotifyContext* ctx);  // 0x535db0

    // ---- other non-virtual members (names provisional) ----
    // kind 0/1/2 -> ShipyardRD / ConstructionYardRD / TrainingFacilRD: the virtual effective value (0x5337f0) and the
    // base value (0x533850); false for any other kind. *out is 0 first.
    int GetEffectiveRdForKind(int kind, int* out);  // 0x5337f0
    int GetBaseRdForKind(int kind, int* out);       // 0x533850
    // Same mapping for the setter of the base value (0x534990).
    int SetBaseRdForKind(int kind, int value, NotifyContext* ctx);  // 0x534990
    // Sets OnMission to (MissionKey is set || MovingBetweenMissions).
    int UpdateOnMission(NotifyContext* ctx);  // 0x534950
    // Rolls GetCombat() against `difficulty` (RollRlevadChance with the control side); on success Virtual130(ctx),
    // otherwise Virtual131(opponent, ctx). Returns 0 when the roll fails the table lookup.
    int RollCombatCheck(const ObjectKey* opponent, int difficulty, NotifyContext* ctx);  // 0x5349e0
    // The key getters (copies of +0x68 / +0x6c / +0x70 / +0x74). Only the +0x70 one has an address of its own; the
    // others are identical functions folded into getters of other classes (0x42d170, 0x4a25f0, 0x40d760).
    ObjectKey* GetMissionKey(ObjectKey* out) {
        out->m_value = m_missionKey.m_value;
        return out;
    }
    ObjectKey* GetMissionSeedKey(ObjectKey* out) {
        out->m_value = m_missionSeedKey.m_value;
        return out;
    }
    ObjectKey* GetParentAtMissionCompletionKey(ObjectKey* out);  // 0x535760
    ObjectKey* GetLocationAtMissionCompletionKey(ObjectKey* out) {
        out->m_value = m_locationAtMissionCompletionKey.m_value;
        return out;
    }
    // The seed key is a NullableKey but every function handles it as an ObjectKey (4 bytes, same code).
    ObjectKey* MissionSeedKeyAsObjectKey() { return reinterpret_cast<ObjectKey*>(&m_missionSeedKey); }
    Role* ViewAsRole(int view) { return static_cast<Role*>(ViewAs(view)); }
    int HasMissionFlag(uint32_t mask) const { return (m_flags78 & mask) != 0; }
    // The event-kind set of Role (g_roleEventKinds, 0x6b9028): built on first use by 0x536bc0, which returns whether
    // it succeeded. Derived classes' static sets start from a copy of it (0x536ca0).
    static int BuildEventKinds();                                      // 0x536bc0
    static int InitEventKinds(int unused, EventKindSet* out);          // 0x536ca0
    // Static initializer of g_roleEventKinds: constructs it (0x536d40) and registers the atexit destructor (0x536d50).
    static void InitStatics();               // 0x536d30
    static void ConstructEventKinds();       // 0x536d40
    static void RegisterEventKindsAtExit();  // 0x536d50

    // ---- fields ----
    int16_t m_baseDiplomacy;                // +0x58
    int16_t m_baseEspionage;                // +0x5a
    int16_t m_baseShipyardRD;               // +0x5c
    int16_t m_baseTrainingFacilRD;          // +0x5e
    int16_t m_baseConstructionYardRD;       // +0x60
    int16_t m_baseCombat;                   // +0x62
    int16_t m_baseLeadership;               // +0x64
    int16_t m_baseLoyalty;                  // +0x66
    ObjectKey m_missionKey;                 // +0x68
    NullableKey m_missionSeedKey;           // +0x6c
    ObjectKey m_parentAtMissionCompletionKey;    // +0x70
    ObjectKey m_locationAtMissionCompletionKey;  // +0x74
    uint32_t m_flags78;                     // +0x78, ten mission flags, bit i = 1 << i in FLAGS order
};

// Iterator over the children of a game object in the person families 0x30-0x3f (vtable 0x66102c).
class RoleIterator : public ChildIterator {
public:
    RoleIterator(GameObjNotifier* parent, uint32_t stateFilter);                 // 0x536da0
    RoleIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side);  // 0x536e20
    virtual ~RoleIterator();                                                     // 0x536d80 (deleting), 0x536ea0
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(RoleSkillRange, 0x08);
REB_ASSERT_OFFSET(RoleRecord, m_skills, 0x58);
REB_ASSERT_SIZE(Role, 0x7c);
REB_ASSERT_OFFSET(Role, m_baseDiplomacy, 0x58);
REB_ASSERT_OFFSET(Role, m_baseLoyalty, 0x66);
REB_ASSERT_OFFSET(Role, m_missionKey, 0x68);
REB_ASSERT_OFFSET(Role, m_missionSeedKey, 0x6c);
REB_ASSERT_OFFSET(Role, m_locationAtMissionCompletionKey, 0x74);
REB_ASSERT_OFFSET(Role, m_flags78, 0x78);
REB_ASSERT_SIZE(RoleIterator, 0x20);
#pragma clang diagnostic pop

