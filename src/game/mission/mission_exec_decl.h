#pragma once
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
#include "mission/mission.h"

// The mission-execution part of the mission manager (family 0xf8, ctor 0x541fc0, 0x74 bytes), range 0x545180-0x549af0.
// The class itself belongs to the story module: this header declares only what this slice needs and can be dropped
// once that declaration exists. Everything declared as a MissionMgr member here is reconstructed in
// mission_exec_*.cpp; every other original function this slice calls is declared below by its Ghidra default name
// (FUN_xxxxxxxx resolves to the original address at link time) and carries the exact stack signature of the original.
// Names of the MissionMgr members are provisional.

class MissionMgr;
class MissionPhaseLevels;

namespace mission_exec {

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

inline bool InFamilies(const ObjectKey* key, uint32_t first, uint32_t last) {
    const uint32_t family = key->Family();
    return family >= first && family < last;
}

inline bool InFamilies(uint32_t family, uint32_t first, uint32_t last) { return family >= first && family < last; }

// 0 -> 0, side 1 -> view 1, side 2 -> view 2 (the "which view to look through" mapping of a control side).
inline int ViewOfSide(uint32_t side) { return side == 1 ? 1 : (side == 2 ? 2 : 0); }

}  // namespace mission_exec


// ---- mission leaf iterators whose class is owned by the leaf-class slices: constructed and destroyed by address ----
// Each is a ChildIterator (0x20 bytes) over one family range of the children of the parent.
#define REB_EXTERNAL_MISSION_ITERATOR(NAME, CTOR, DTOR)                                  \
    void* __thiscall FUN_##CTOR(void* iterator, GameObjNotifier* parent);                \
    void __thiscall FUN_##DTOR(void* iterator);                                           \
    class NAME {                                                                          \
    public:                                                                               \
        explicit NAME(GameObjNotifier* parent) { FUN_##CTOR(m_storage, parent); }         \
        ~NAME() { FUN_##DTOR(m_storage); }                                                \
        NAME(const NAME&) = delete;                                                       \
        NAME& operator=(const NAME&) = delete;                                            \
        ChildIterator* get() { return reinterpret_cast<ChildIterator*>(m_storage); }      \
        Mission* Current() { return static_cast<Mission*>(get()->Current()); }            \
                                                                                          \
    private:                                                                              \
        alignas(8) unsigned char m_storage[sizeof(ChildIterator)];                        \
    }
REB_EXTERNAL_MISSION_ITERATOR(MissionIteratorFam53, 0056d1b0, 0056d250);  // family 0x53
REB_EXTERNAL_MISSION_ITERATOR(MissionIteratorFam58, 00571650, 005716f0);  // family 0x58
REB_EXTERNAL_MISSION_ITERATOR(MissionIteratorFam50to60, 00586b70, 00586c10);  // families 0x50-0x5f
REB_EXTERNAL_MISSION_ITERATOR(MissionIteratorFam56, 00571c20, 00571cc0);  // family 0x56
REB_EXTERNAL_MISSION_ITERATOR(MissionIteratorFam57, 00569e10, 00569eb0);  // family 0x57
#undef REB_EXTERNAL_MISSION_ITERATOR

class MissionMgr : public GameObj {
public:
    // ==== SECTION events (0x545180-0x546f80): reactions of the missions to object events ====
    int OnCharacterEvent(ObjectKey* key, int unused, NotifyContext* ctx);                                     // 0x545180
    int UpdateMissionsForObject(ObjectKey* key, int unused, ObjectKey* location, int skipMissions, NotifyContext* ctx);  // 0x545240
    int UpdateMissionsForObjective(ObjectKey* key, int unused, int unused2, int skipMissions, NotifyContext* ctx);  // 0x545540
    int UpdateMissionsForTargetOrObjective(ObjectKey* key, int unused, int unused2, NotifyContext* ctx);      // 0x545670
    int RunStoryPipelineForStoryKey(ObjectKey* key, int unused, int unused2, NotifyContext* ctx);             // 0x5457f0
    int UpdatePersonMission(ObjectKey* key, int unused, NotifyContext* ctx);                                    // 0x545820
    int OnSystemEvent(ObjectKey* key, int unused, NotifyContext* ctx);                                          // 0x545950
    int OnSystemEventNoop(int a, int b, int c);                                                                // 0x5459d0
    int UpdateMissionsAtSystemA(ObjectKey* key, int enabled, NotifyContext* ctx);                               // 0x545a10
    int UpdateMissionsAtSystemB(ObjectKey* key, int enabled, NotifyContext* ctx);                               // 0x545b20
    int UpdateMissionsAtLocation53(ObjectKey* key, int unused, NotifyContext* ctx);                             // 0x545c30
    int UpdateMissionsAtLocation50to60(ObjectKey* key, int unused, NotifyContext* ctx);                         // 0x545d30
    int OnPersonStateChanged(ObjectKey* key, int released, NotifyContext* ctx);                                 // 0x545df0
    int OnPersonRemoved(ObjectKey* key, int unused, NotifyContext* ctx);                                        // 0x545e90
    int ProcessResignRequest(ObjectKey* key, int enabled, NotifyContext* ctx);                                  // 0x546000
    int ProcessRemoveRequest(ObjectKey* key, int enabled, NotifyContext* ctx);                                  // 0x546240
    int OnPersonArrived(ObjectKey* key, int unused, int expectedState, NotifyContext* ctx);                     // 0x546370
    int RunStoryPipelineForStoryKey2(ObjectKey* key, int unused, NotifyContext* ctx);                           // 0x546e30
    int AdvanceMissionPhase(ObjectKey* missionKey, int enabled, NotifyContext* ctx);                            // 0x546e70
    int RunMissionPhase(ObjectKey* missionKey, int fromPhase, int toPhase, NotifyContext* ctx);                    // 0x546ea0
    int UpdateStoryFlags(NotifyContext* ctx);                                                                  // 0x546f80

    // ==== SECTION creation (0x546fa0-0x547ba0): missions created for characters ====
    int CreateStoryMissionAtRandom(NotifyContext* ctx);                                                        // 0x546fa0
    int CreateMissionType41ForCharacter(Character* person, NotifyContext* ctx);                                // 0x547180
    int CreateMissionType41(int side, ObjectKey* targetKey, ObjectKeyValueSet* team, NotifyContext* ctx);       // 0x547340
    int CreateMissionType41WithLevels(int side, ObjectKey* targetKey, ObjectKeyValueSet* team, NotifyContext* ctx);  // 0x547480
    int CreateMissionType42(int side, MissionPhaseLevels* levels, ObjectKey* targetKey, ObjectKeyValueSet* team,
                            int mandatory, NotifyContext* ctx);                                                // 0x5475f0
    int CreateMissionType43(int side, ObjectKey* targetKey, ObjectKeyValueSet* team, int impliedTeam,
                            NotifyContext* ctx);                                                               // 0x547740
    int CreateMissionType44(int side, ObjectKey* targetKey, ObjectKeyValueSet* team, NotifyContext* ctx);       // 0x547880
    int CreateMissionForCharacter(ObjectKey* objectiveKey, ObjectKey* typeKey, ObjectKey* personKey,
                                  NotifyContext* ctx);                                                         // 0x5479c0
    int CreateMission(int userId, int userId2, int side, ObjectKey* objectiveKey, ObjectKey* typeKey, ObjectKey* targetKey,
                      ObjectKey* originKey, ObjectKeyValueSet* team, ObjectKeyValueSet* decoys,
                      MissionPhaseLevels* levels, int impliedTeam, int mandatory, NotifyContext* ctx);          // 0x547ba0

    // ==== SECTION phase (0x547f60-0x548840): bookkeeping when a mission changes phase ====
    int ReleaseObjectiveSystem(Mission* mission, NotifyContext* ctx);                                           // 0x547f60
    int DetachMissionMembers(Mission* mission, NotifyContext* ctx);                                             // 0x548120
    int ApplyMissionOutcomeModifiers(Mission* mission, NotifyContext* ctx);                                     // 0x548370
    int UpdateSystemMissionScores(Mission* mission, int fromPhase, int toPhase, NotifyContext* ctx);            // 0x5484d0
    int OnMissionPhaseStory(Mission* mission, int fromPhase, int toPhase, NotifyContext* ctx);                       // 0x548840

    // ==== SECTION members (0x548910-0x549850): moving persons whose mission ended ====
    int ReassignMissionMembers(Mission* mission, int* moved, NotifyContext* ctx);                               // 0x548910
    int ReleasePersonsOfKey(ObjectKey* key, NotifyContext* ctx);                                                // 0x548da0
    int ReleaseAdriftPersonsOfSystem(ObjectKey* systemKey, NotifyContext* ctx);                                 // 0x548e90
    int ReleaseSystemPersons(GameObj* system, int* moved, NotifyContext* ctx);                                  // 0x549230
    int ReleasePersons(ObjectKeyValueSet* persons, NotifyContext* ctx);                                         // 0x549580
    int ReleaseFreePersons(ObjectKeyValueSet* persons, NotifyContext* ctx);                                     // 0x549850

    // ==== SECTION story (0x549910-0x549a20) ====
    int RunStoryPipeline(NotifyContext* ctx);                                                                  // 0x549910
    int InjureStoryCharacter(NotifyContext* ctx);                                                              // 0x549a20
};

// Factory registered for family 0xf8 (see the registry at 0x51ebf0).
GameObj* CreateMissionMgr();  // 0x549af0

// ---- slot 120 of Mission is a pure virtual that takes (team, decoys, ctx); mission.h declares it without arguments ----
inline int CallMissionSlot120(Mission* mission, ObjectKeyValueSet* team, ObjectKeyValueSet* decoys, NotifyContext* ctx) {
    using Slot = int(__thiscall*)(Mission*, ObjectKeyValueSet*, ObjectKeyValueSet*, NotifyContext*);
    return reinterpret_cast<Slot*>(*reinterpret_cast<void***>(mission))[120](mission, team, decoys, ctx) ? 1 : 0;
}

// ---- Mission members whose declaration in mission.h differs from the original signature ----
// Mission::GetTransitionFlags: (person, &a, &b, &c); it reads the person's flags and the mission's completion status.
int __thiscall FUN_00521220(Mission* mission, Role* person, int* a, int* b, int* c);  // 0x521220
// Mission::FillStatusFlags: fills the four phase levels.
int __thiscall FUN_00520ec0(Mission* mission, MissionPhaseLevels* levels);  // 0x520ec0
// Mission::AssignPhaseLevels with its real argument type.
int __thiscall FUN_00522a60(Mission* mission, MissionPhaseLevels* levels);  // 0x522a60
// Record key of the object ((record family << 24) | record id), 0x402d80; returns `out`.
ObjectKey* __thiscall FUN_00402d80(GameObjNotifier* object, ObjectKey* out);
// Owner key of an object: GameObjRoot::GetOwnerKey into `out`, returns `out` (0x50c640).
ObjectKey* __thiscall FUN_0050c640(GameObjNotifier* object, ObjectKey* out);

// ---- the rules object (game root member +0xbc, type 0xf9, 0x554020-0x557410) ----
int __thiscall FUN_00556710(GameObjNotifier* rules, GameObj* person, NotifyContext* ctx);
int __thiscall FUN_00556390(GameObjNotifier* rules, ObjectKey* personKey, ObjectKey* locationKey, NotifyContext* ctx);
int __thiscall FUN_00556c70(GameObjNotifier* rules, ObjectKey* key, int a, NotifyContext* ctx);

// ---- MissionMgr story checks (0x541fc0-0x545180) and the mission creation helpers (0x549af0-0x54cd80) ----
int __stdcall FUN_005438a0(const ObjectKey* key);   // key is one of the six named story characters
int __stdcall FUN_005442f0(NotifyContext* ctx);
int __thiscall FUN_00544da0(MissionMgr* mgr, NotifyContext* ctx);
int __thiscall FUN_00544030(MissionMgr* mgr, NotifyContext* ctx);
int __thiscall FUN_00544130(MissionMgr* mgr, NotifyContext* ctx);
int __thiscall FUN_005443f0(MissionMgr* mgr, NotifyContext* ctx);
int __thiscall FUN_005444e0(MissionMgr* mgr, NotifyContext* ctx);
int __thiscall FUN_005445d0(MissionMgr* mgr, NotifyContext* ctx);
int __thiscall FUN_00544a20(MissionMgr* mgr, NotifyContext* ctx);
int __thiscall FUN_00544be0(MissionMgr* mgr, NotifyContext* ctx);
int __thiscall FUN_00544fc0(MissionMgr* mgr, NotifyContext* ctx);
int __thiscall FUN_00543af0(MissionMgr* mgr, int on, NotifyContext* ctx);
int __thiscall FUN_00543040(MissionMgr* mgr, GameObj* person, int unused, ObjectKey* out);
int __thiscall FUN_00542990(MissionMgr* mgr, int a, int b, int* out);
int __thiscall FUN_00542ee0(MissionMgr* mgr, Role* person, Mission* mission, int c, ObjectKey* out);
int __thiscall FUN_005435f0(MissionMgr* mgr, GameObjNotifier** first, GameObjNotifier** parent, ObjectKey* parentKey);
int __thiscall FUN_005434b0(MissionMgr* mgr, GameObjNotifier** first, GameObjNotifier** parent, ObjectKey* parentKey);
int __thiscall FUN_0054bf00(MissionMgr* mgr, ObjectKeyValueSet* team, ObjectKey* out, StatusCode* status);
int __thiscall FUN_0054c7d0(MissionMgr* mgr, GameObj* object, int* a, uint32_t* side, uint32_t* flag, ObjectKey* key,
                            ObjectKeyValueSet* set, NotifyContext* ctx);
MissionMgr* __thiscall FUN_00541fc0(void* memory);  // the MissionMgr constructor

// ---- per-class checks of the 0x58 mission family ----
int __thiscall FUN_00570470(Mission* mission, NotifyContext* ctx);
int __thiscall FUN_005704b0(Mission* mission, NotifyContext* ctx);

extern int DAT_006bb5b8;  // GNPRTB 0x1806, a percentage
