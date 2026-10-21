#pragma once
class ObjectNameList;
#include <cstdint>

#include "character/character.h"
#include "character/role.h"
#include "data/random.h"
#include "fleet/capship.h"
#include "fleet/fleet_iterator.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_queries.h"
#include "gameobj/tracked_stat.h"
#include "mission/mission.h"

// MissionMgr (family 0xf8, key 0xf8000006, ctor 0x541fc0, VT 0x661c08, secondary ObjectBase VT 0x661c04, 0x74 bytes) is
// the mission manager and the keeper of the story. It is created once with the game (the game root's cached member
// +0xb8). It owns
//   * twelve story flags (word at +0x58) that follow the course of the Luke / Leia / Han / Darth / Emperor story line,
//   * two TrackedStats (+0x5c, +0x64) that pace the bounty hunter timers (events 0x392 and 0x393),
//   * a table with one entry per mission type (+0x6c count, +0x70 array of 12-byte entries: the type's record key and
//   the
//     two check callbacks its missions report in slot 111), built when the master is created.
// and decides which missions the strategic game starts, resumes and finishes (the mission execution slice,
// 0x545180-0x549af0).
//
// The story flags are properties with the usual pattern (SetX on the master, ApplyX on a side view, OnXChanged on the
// master, OnXNotified on a side view). Their bits skip 0x2, 0x8 and 0x10. Setting a flag is how one step of the story
// triggers the next: the OnXChanged handlers create the story's missions (Dagobah, Palace, Pickup) or run the final
// battle.
//
// Vtable (136 slots): the first 111 are GameObj's. 111 creates a mission for an order request, 112-123 are the
// OnXChanged events of the twelve flags and 124-135 their OnXNotified events (in the order of the bits).

namespace story_flag
{
constexpr uint32_t kLukeDagobahRequired = 0x1;
constexpr uint32_t kLukeDagobah = 0x4;
constexpr uint32_t kLukePalace = 0x20;
constexpr uint32_t kHanCapturedAtPalace = 0x40;
constexpr uint32_t kLeiaPalace = 0x80;
constexpr uint32_t kChewbaccaPalace = 0x100;
constexpr uint32_t kDarthPickup = 0x200;
constexpr uint32_t kDarthToLukeFinalBattle = 0x400;
constexpr uint32_t kDarthToEmperorFinalBattle = 0x800;
constexpr uint32_t kFinalBattleReady = 0x1000;
constexpr uint32_t kFinalBattle = 0x2000;
constexpr uint32_t kBountyHuntersActive = 0x4000;
} // namespace story_flag

class MissionMgr;
class MissionPhaseLevels;
class ObjectRefList;
class DatRecord;
struct MissionMemberSummary;
struct MissionCheckFunctions;

namespace mission_exec
{

constexpr uint32_t kStateDestroyed = 0x8;
constexpr uint32_t kStateEnrouteActive = 0x20;
constexpr uint32_t kStateExisting = 0x40;
constexpr uint32_t kStateAutorouting = 0x1000;

constexpr uint32_t kPersonFlagAdrift = role_flag::kAdrift;
constexpr uint32_t kCharacterCaptured = 0x1;

constexpr uint32_t kFamilyPersonBegin = 0x30;
constexpr uint32_t kFamilyPersonEnd = 0x40;
constexpr uint32_t kFamilyUnitBegin = 0x10;
constexpr uint32_t kFamilySystemBegin = 0x90;
constexpr uint32_t kFamilySystemEnd = 0x98;
constexpr uint32_t kFamilySystemOrNextEnd = 0xa0;

constexpr int kSideAlliance = 1;
constexpr int kSideEmpire = 2;

inline bool InFamilies(const ObjectKey* key, uint32_t first, uint32_t last)
{
  const uint32_t family = key->Family();
  return family >= first && family < last;
}

inline bool InFamilies(uint32_t family, uint32_t first, uint32_t last)
{
  return family >= first && family < last;
}

// 0 -> 0, side 1 -> view 1, side 2 -> view 2 (the "which view to look through" mapping of a control side).
inline int ViewOfSide(uint32_t side)
{
  return side == 1 ? 1 : (side == 2 ? 2 : 0);
}

} // namespace mission_exec

// One entry of the mission-type table: the type's record key and the two check callbacks (member check, target check)
// that a mission of that type reports through GetCheckFunctions.
struct MissionTypeCheckEntry {
  ObjectKey m_recordKey; // +0x00
  void* m_checkMember;   // +0x04
  void* m_checkTarget;   // +0x08
};

class MissionMgr : public GameObj
{
  public:
  MissionMgr();           // 0x541fc0
  ~MissionMgr() override; // 0x542040 (deleting), 0x542060

  // ---- overrides of GameObj slots ----
  uint32_t GetFamily() override;                                  // 1, 0x549be0: 0xf8
  int GetOwnerKey(ObjectKey* out) override;                       // 3, shared 0x5214e0: clears *out
  int Read(istream* in) override;                                 // 5, 0x549b80
  int Write(ostream* out) override;                               // 6, 0x549b90
  int Checksum(Crc32* crc) override;                              // 7, 0x549bc0
  int Validate() override;                                        // 8, 0x549bd0 (jump thunk to 0x54a030)
  int ApplyAll(NotifyContext* ctx) override;                      // 9, 0x549ba0
  int GetEventKinds(int unused, EventKindSet* out) override;      // 15, 0x543a10
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x543a30
  int UpdateObservedByAlliance(NotifyContext* ctx) override;      // 35, shared 0x5620a0
  int UpdateObservedByEmpire(NotifyContext* ctx) override;        // 36, shared 0x5620c0
  int CreateViews(int controlKind, NotifyContext* ctx) override;  // 37, 0x549b60
  GameObj* CreateViewObject() override;                           // 57, 0x549af0

  // ---- new virtuals ----
  // Creates the mission of `typeKey` for the order `request` (the request's target is the objective): the origin is the
  // common location of the team. Returns CreateMission's result.
  virtual int CreateMissionForRequest(const ActivityRequest* request, int userId, int userId2, int side,
                                      ObjectKey* typeKey, ObjectKey* targetKey, ObjectKeyValueSet* team,
                                      ObjectKeyValueSet* decoys,
                                      NotifyContext* ctx);                    // 111, 0x549bf0
  virtual int OnLukeDagobahRequiredChanged(int on, NotifyContext* ctx);       // 112, 0x54a800
  virtual int OnLukeDagobahChanged(int on, NotifyContext* ctx);               // 113, 0x54a860
  virtual int OnLukePalaceChanged(int on, NotifyContext* ctx);                // 114, 0x54a910
  virtual int OnHanCapturedAtPalaceChanged(int on, NotifyContext* ctx);       // 115, 0x54a9c0
  virtual int OnLeiaPalaceChanged(int on, NotifyContext* ctx);                // 116, 0x54aa60
  virtual int OnChewbaccaPalaceChanged(int on, NotifyContext* ctx);           // 117, 0x54ab10
  virtual int OnDarthPickupChanged(int on, NotifyContext* ctx);               // 118, 0x54abc0
  virtual int OnDarthToLukeFinalBattleChanged(int on, NotifyContext* ctx);    // 119, 0x54ac70
  virtual int OnDarthToEmperorFinalBattleChanged(int on, NotifyContext* ctx); // 120, 0x54adf0
  virtual int OnFinalBattleReadyChanged(int on, NotifyContext* ctx)
  {
    return 1;
  } // 121, shared stub 0x524fb0
  virtual int OnFinalBattleChanged(int on, NotifyContext* ctx);                // 122, 0x54b040
  virtual int OnBountyHuntersActiveChanged(int on, NotifyContext* ctx);        // 123, 0x54b740
  virtual int OnLukeDagobahRequiredNotified(int on, NotifyContext* ctx);       // 124, 0x54b770
  virtual int OnLukeDagobahNotified(int on, NotifyContext* ctx);               // 125, 0x54b7b0
  virtual int OnLukePalaceNotified(int on, NotifyContext* ctx);                // 126, 0x54b800
  virtual int OnHanCapturedAtPalaceNotified(int on, NotifyContext* ctx);       // 127, 0x54b840
  virtual int OnLeiaPalaceNotified(int on, NotifyContext* ctx);                // 128, 0x54b880
  virtual int OnChewbaccaPalaceNotified(int on, NotifyContext* ctx);           // 129, 0x54b8c0
  virtual int OnDarthPickupNotified(int on, NotifyContext* ctx);               // 130, 0x54b900
  virtual int OnDarthToLukeFinalBattleNotified(int on, NotifyContext* ctx);    // 131, 0x54b940
  virtual int OnDarthToEmperorFinalBattleNotified(int on, NotifyContext* ctx); // 132, 0x54b980
  virtual int OnFinalBattleReadyNotified(int on, NotifyContext* ctx);          // 133, 0x54b9c0
  virtual int OnFinalBattleNotified(int on, NotifyContext* ctx);               // 134, 0x54ba00
  virtual int OnBountyHuntersActiveNotified(int on, NotifyContext* ctx);       // 135, 0x54ba50

  // ==== SECTION flags (0x543af0-0x543fc0, 0x54a140-0x54a770): the twelve story flag properties ====
  int SetLukeDagobahRequired(int on, NotifyContext* ctx);       // 0x543af0
  int SetLukeDagobah(int on, NotifyContext* ctx);               // 0x543b60
  int SetLukePalace(int on, NotifyContext* ctx);                // 0x543bd0
  int SetHanCapturedAtPalace(int on, NotifyContext* ctx);       // 0x543c40
  int SetLeiaPalace(int on, NotifyContext* ctx);                // 0x543cb0
  int SetChewbaccaPalace(int on, NotifyContext* ctx);           // 0x543d20
  int SetDarthPickup(int on, NotifyContext* ctx);               // 0x543d90
  int SetDarthToLukeFinalBattle(int on, NotifyContext* ctx);    // 0x543e00
  int SetDarthToEmperorFinalBattle(int on, NotifyContext* ctx); // 0x543e70
  int SetFinalBattleReady(int on, NotifyContext* ctx);          // 0x543ee0
  int SetFinalBattle(int on, NotifyContext* ctx);               // 0x543f50
  int SetBountyHuntersActive(int on, NotifyContext* ctx);       // 0x543fc0
  int ApplyLukeDagobahRequired(NotifyContext* ctx);             // 0x54a140
  int ApplyLukeDagobah(NotifyContext* ctx);                     // 0x54a1d0
  int ApplyLukePalace(NotifyContext* ctx);                      // 0x54a260
  int ApplyHanCapturedAtPalace(NotifyContext* ctx);             // 0x54a2f0
  int ApplyLeiaPalace(NotifyContext* ctx);                      // 0x54a380
  int ApplyChewbaccaPalace(NotifyContext* ctx);                 // 0x54a410
  int ApplyDarthPickup(NotifyContext* ctx);                     // 0x54a4a0
  int ApplyDarthToLukeFinalBattle(NotifyContext* ctx);          // 0x54a530
  int ApplyDarthToEmperorFinalBattle(NotifyContext* ctx);       // 0x54a5c0
  int ApplyFinalBattleReady(NotifyContext* ctx);                // 0x54a650
  int ApplyFinalBattle(NotifyContext* ctx);                     // 0x54a6e0
  int ApplyBountyHuntersActive(NotifyContext* ctx);             // 0x54a770

  // ==== SECTION persistence (0x549d60-0x54a140) ====
  int CreateViewsBody(int controlKind, NotifyContext* ctx); // 0x549d60
  int ReadBody(istream* in);                                // 0x549ea0
  int WriteBody(ostream* out);                              // 0x549f00
  int ApplyAllBody(NotifyContext* ctx);                     // 0x549f50
  int ChecksumBody(Crc32* crc);                             // 0x549fe0
  int ValidateBody();                                       // 0x54a030
  // The mission-type table (0x54cb10-0x54cb70).
  int ClearTypeTable();                                                     // 0x54cb10
  int FreeTypeTable();                                                      // 0x54cb20
  int BuildTypeTable();                                                     // 0x54cb70
  int FindTypeChecks(const ObjectKey* typeKey, MissionCheckFunctions* out); // 0x54c720, copies the two callbacks

  static int BuildEventKinds();     // 0x54ba90, g_missionMgrEventKinds at 0x6b92e8
  static GameObjNotifier* Create(); // 0x549cf0

  // ==== SECTION story (0x5434b0-0x5445d0, 0x544a20-0x544fc0): the finders of the six story characters, the predicates
  // and the steps that derive the flags from the state of the characters ====
  int FindLuke(GameObjNotifier** character, GameObjNotifier** location, ObjectKey* locationKey);        // 0x5434b0
  int FindLeia(GameObjNotifier** character, GameObjNotifier** location, ObjectKey* locationKey);        // 0x543550
  int FindHan(GameObjNotifier** character, GameObjNotifier** location, ObjectKey* locationKey);         // 0x5435f0
  int FindCharacter38(GameObjNotifier** character, GameObjNotifier** location, ObjectKey* locationKey); // 0x543690
  int FindDarth(GameObjNotifier** character, GameObjNotifier** location, ObjectKey* locationKey);       // 0x543760
  int FindEmperor(GameObjNotifier** character, GameObjNotifier** location, ObjectKey* locationKey);     // 0x543800
  static int IsStoryCharacterKey(const ObjectKey* key);                                                 // 0x5438a0
  static int IsStoryCharacterFree(Character* character); // 0x5439b0: not captured, usable, not autorouting, not adrift
  static int IsStoryCharacterIdle(Character* character); // 0x5439e0: free and not on a mission
  int UpdateHanCapturedAtPalace(NotifyContext* ctx);     // 0x5442f0
  int UpdateLukeDagobahStep(NotifyContext* ctx);         // 0x544030
  int UpdateLukePalaceStep(NotifyContext* ctx);          // 0x544130
  int UpdateLeiaPalaceStep(NotifyContext* ctx);          // 0x5443f0
  int UpdateChewbaccaPalaceStep(NotifyContext* ctx);     // 0x5444e0
  int UpdateDarthPickupStep(NotifyContext* ctx);         // 0x5445d0
  int UpdateDarthToLukeStep(NotifyContext* ctx);         // 0x544a20
  int UpdateDarthToEmperorStep(NotifyContext* ctx);      // 0x544be0
  int UpdateFinalBattleReadyStep(NotifyContext* ctx);    // 0x544da0
  int UpdateFinalBattleStep(NotifyContext* ctx);         // 0x544fc0

  // ==== SECTION orders (0x5420d0-0x543410, 0x54bac0-0x54c720): the checks and candidate lists of the mission orders
  // ==== Candidate lists of the order of the team `team` / `characters` (the two key sets of the order): the keys of
  // the mission types the team could start, and the same with the names of the types that suit the request's objective.
  int CollectMissionTypesForTeam(const ActivityRequest* request, ObjectKeyValueSet* team, ObjectKeyValueSet* characters,
                                 ObjectRefList* out,
                                 StatusCode* status); // 0x5420d0
  int CollectMissionTypeNamesAtTarget(const ActivityRequest* request, ObjectKeyValueSet* team,
                                      ObjectKeyValueSet* characters, ObjectNameList* out,
                                      StatusCode* status); // 0x5422f0
  // Gathers the people of the order into `everyone` and finds how the mission travels: *resultKey and the travel mode
  // that is stored as the value of every node of `everyone`.
  int ResolveMissionTravel(const ActivityRequest* request, ObjectKey* resultKey, ObjectKeyValueSet* team,
                           ObjectKeyValueSet* characters, ObjectKeyValueSet* everyone,
                           StatusCode* status); // 0x5425e0
  // The hyperdrive modifier of a mission: GNPRTB 1 unless Han travels with the team (no special forces).
  int GetMissionHyperdriveModifier(int linked, int hasForces, int* modifier); // 0x542990
  int CheckMissionOrderMembers(const ActivityRequest* request, int a, int b, ObjectKeyValueSet* team,
                               ObjectKeyValueSet* characters,
                               StatusCode* status); // 0x5429e0
  int CheckMissionOrder(const ActivityRequest* request, int a, int b, ObjectKey* missionKey, ObjectKey* secondKey,
                        ObjectKeyValueSet* team, ObjectKeyValueSet* characters,
                        StatusCode* status); // 0x542b60
  // Where a person that is released from `mission` goes (the place of the mission, else its own destination).
  int FindPersonMissionPoint(Role* person, Mission* mission, int flag, ObjectKey* out); // 0x542ee0
  int FindPersonDestination(GameObj* person, int flag, ObjectKey* out);                 // 0x543040
  int FindPersonSourceKey(GameObj* person, ObjectKey* out);                             // 0x5431e0
  int FindPersonOriginKey(GameObj* person, ObjectKey* out);                             // 0x543360
  // Locates the mission of the type `missionKey` for the target through the type's member check: *mission and *status.
  int FindMissionForTarget(int side, NullableKey* missionKey, GameObjNotifier* target, GameObjNotifier** mission,
                           StatusCode* status); // 0x543410
  // Splits the people of an order: members (without the captured), decoys, captured and the union of all.
  int GatherMissionMembers(const ActivityRequest* request, ObjectKeyValueSet* team, ObjectKeyValueSet* characters,
                           ObjectKeyValueSet* members, ObjectKeyValueSet* decoys, ObjectKeyValueSet* captured,
                           ObjectKeyValueSet* everyone,
                           StatusCode* status);              // 0x54bb90
  int AddRecordName(DatRecord* record, ObjectNameList* out); // 0x54bac0
  // The location all people share (*out unset if there is none; the status says why).
  int FindCommonOrigin(ObjectKeyValueSet* team, ObjectKey* out, StatusCode* status);                  // 0x54bf00
  int CheckTeamActivity(const ActivityRequest* request, ObjectKeyValueSet* team, StatusCode* status); // 0x54c110
  int CheckTeamActivities(const ActivityRequest* request, ObjectKeyValueSet* members, ObjectKeyValueSet* decoys,
                          ObjectKeyValueSet* captured,
                          StatusCode* status); // 0x54c200
  int CheckMissionTypeForTeam(int side, ObjectKeyValueSet* members, ObjectKeyValueSet* decoys,
                              ObjectKeyValueSet* captured, MissionMemberSummary* summary, DatRecord* record,
                              StatusCode* status); // 0x54c440
  // Frees persons of `object` that are stuck in autorouting or adrift: the ones that need a new home are collected into
  // `persons` (all with the same side, adrift state and destination, which are reported in *side, *flag and *key).
  int CollectStrandedPersons(GameObj* object, int* found, uint32_t* side, uint32_t* flag, ObjectKey* key,
                             ObjectKeyValueSet* persons,
                             NotifyContext* ctx); // 0x54c7d0
  int CheckMissionTypeAtTarget(int side, ObjectKeyValueSet* members, ObjectKeyValueSet* decoys,
                               ObjectKeyValueSet* captured, MissionMemberSummary* summary, GameObjNotifier* target,
                               DatRecord* record,
                               StatusCode* status); // 0x54c590

  // ==== SECTION events (0x545180-0x546f80): reactions of the missions to object events ====
  int OnCharacterEvent(ObjectKey* key, int unused, NotifyContext* ctx); // 0x545180
  int UpdateMissionsForObject(ObjectKey* key, intptr_t unused, ObjectKey* location, int skipMissions,
                              NotifyContext* ctx); // 0x545240
  int UpdateMissionsForObjective(ObjectKey* key, intptr_t unused, intptr_t unused2, int skipMissions,
                                 NotifyContext* ctx);                                                     // 0x545540
  int UpdateMissionsForTargetOrObjective(ObjectKey* key, int unused, int unused2, NotifyContext* ctx);    // 0x545670
  int RunStoryPipelineForStoryKey(ObjectKey* key, intptr_t unused, intptr_t unused2, NotifyContext* ctx); // 0x5457f0
  int UpdatePersonMission(ObjectKey* key, int unused, NotifyContext* ctx);                                // 0x545820
  int OnSystemEvent(ObjectKey* key, int unused, NotifyContext* ctx);                                      // 0x545950
  int OnSystemEventNoop(intptr_t a, int b, intptr_t c);                                                   // 0x5459d0
  int UpdateMissionsAtSystemA(ObjectKey* key, int enabled, NotifyContext* ctx);                           // 0x545a10
  int UpdateMissionsAtSystemB(ObjectKey* key, int enabled, NotifyContext* ctx);                           // 0x545b20
  int UpdateMissionsAtLocation53(ObjectKey* key, int unused, NotifyContext* ctx);                         // 0x545c30
  int UpdateMissionsAtLocation50to60(ObjectKey* key, int unused, NotifyContext* ctx);                     // 0x545d30
  int OnPersonStateChanged(ObjectKey* key, int released, NotifyContext* ctx);                             // 0x545df0
  int OnPersonRemoved(ObjectKey* key, int unused, NotifyContext* ctx);                                    // 0x545e90
  int ProcessResignRequest(ObjectKey* key, int enabled, NotifyContext* ctx);                              // 0x546000
  int ProcessRemoveRequest(ObjectKey* key, int enabled, NotifyContext* ctx);                              // 0x546240
  int OnPersonArrived(ObjectKey* key, int unused, int expectedState, NotifyContext* ctx);                 // 0x546370
  int RunStoryPipelineForStoryKey2(ObjectKey* key, int unused, NotifyContext* ctx);                       // 0x546e30
  int AdvanceMissionPhase(ObjectKey* missionKey, int enabled, NotifyContext* ctx);                        // 0x546e70
  int RunMissionPhase(ObjectKey* missionKey, int fromPhase, int toPhase, NotifyContext* ctx);             // 0x546ea0
  int UpdateStoryFlags(NotifyContext* ctx);                                                               // 0x546f80

  // ==== SECTION creation (0x546fa0-0x547ba0): missions created for characters ====
  int CreateStoryMissionAtRandom(NotifyContext* ctx);                                                   // 0x546fa0
  int CreateMissionType41ForCharacter(Role* person, NotifyContext* ctx);                                // 0x547180
  int CreateMissionType41(int side, ObjectKey* targetKey, ObjectKeyValueSet* team, NotifyContext* ctx); // 0x547340
  int CreateMissionType41WithLevels(int side, ObjectKey* targetKey, ObjectKeyValueSet* team,
                                    NotifyContext* ctx); // 0x547480
  int CreateMissionType42(int side, MissionPhaseLevels* levels, ObjectKey* targetKey, ObjectKeyValueSet* team,
                          int mandatory,
                          NotifyContext* ctx); // 0x5475f0
  int CreateMissionType43(int side, ObjectKey* targetKey, ObjectKeyValueSet* team, int impliedTeam,
                          NotifyContext* ctx);                                                          // 0x547740
  int CreateMissionType44(int side, ObjectKey* targetKey, ObjectKeyValueSet* team, NotifyContext* ctx); // 0x547880
  int CreateMissionForCharacter(ObjectKey* objectiveKey, ObjectKey* typeKey, ObjectKey* personKey,
                                NotifyContext* ctx); // 0x5479c0
  int CreateMission(int userId, int userId2, int side, ObjectKey* objectiveKey, ObjectKey* typeKey,
                    ObjectKey* targetKey, ObjectKey* originKey, ObjectKeyValueSet* team, ObjectKeyValueSet* decoys,
                    MissionPhaseLevels* levels, int impliedTeam, int mandatory,
                    NotifyContext* ctx); // 0x547ba0

  // ==== SECTION phase (0x547f60-0x548840): bookkeeping when a mission changes phase ====
  int ReleaseObjectiveSystem(Mission* mission, NotifyContext* ctx);                                // 0x547f60
  int DetachMissionMembers(Mission* mission, NotifyContext* ctx);                                  // 0x548120
  int ApplyMissionOutcomeModifiers(Mission* mission, NotifyContext* ctx);                          // 0x548370
  int UpdateSystemMissionScores(Mission* mission, int fromPhase, int toPhase, NotifyContext* ctx); // 0x5484d0
  int OnMissionPhaseStory(Mission* mission, int fromPhase, int toPhase, NotifyContext* ctx);       // 0x548840

  // ==== SECTION members (0x548910-0x549850): moving persons whose mission ended ====
  int ReassignMissionMembers(Mission* mission, int* moved, NotifyContext* ctx); // 0x548910
  int ReleasePersonsOfKey(ObjectKey* key, NotifyContext* ctx);                  // 0x548da0
  int ReleaseAdriftPersonsOfSystem(ObjectKey* systemKey, NotifyContext* ctx);   // 0x548e90
  int ReleaseSystemPersons(GameObj* system, int* moved, NotifyContext* ctx);    // 0x549230
  int ReleasePersons(ObjectKeyValueSet* persons, NotifyContext* ctx);           // 0x549580
  int ReleaseFreePersons(ObjectKeyValueSet* persons, NotifyContext* ctx);       // 0x549850

  // ==== SECTION story pipeline (0x549910-0x549a20) ====
  int RunStoryPipeline(NotifyContext* ctx);     // 0x549910
  int InjureStoryCharacter(NotifyContext* ctx); // 0x549a20

  // ---- fields ----
  uint32_t m_storyFlags;              // +0x58, the twelve story flags (story_flag::k*)
  TrackedStat m_statA;                // +0x5c, paces the timer 0x392
  TrackedStat m_statB;                // +0x64, paces the timer 0x393
  int m_typeCount;                    // +0x6c, number of entries of the mission-type table
  MissionTypeCheckEntry* m_typeTable; // +0x70, one entry per mission-type record (family 0x40-0x7f)
};
