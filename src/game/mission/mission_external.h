#pragma once
#include <cstdint>

#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/object_queries.h"
#include "mission/mission.h"
#include "rules/rules_mgr.h"

// Things mission code uses that belong to other modules, and the small records the mission checks pass around.

inline RulesMgr* GetRulesMgr() { return static_cast<RulesMgr*>(GetGameRootMember_bc()); }

// The people on a mission as the checks see them (0x14 bytes, zero-initialised by 0x568fe0).
struct MissionMemberSummary {
    uint32_t m_specialForceMissionBits;  // +0x00 union of the special forces' SD mission bits
    uint32_t m_characterAbilityMask;     // +0x04 union of the characters' member masks
    uint32_t m_hasAlliance;              // +0x08 a team member is controlled by the Alliance
    uint32_t m_hasEmpire;                // +0x0c a team member is controlled by the Empire
    uint32_t m_kind;                     // +0x10 1 = Alliance, 2 = Empire (set by FillMemberSummary)

    // Copies fields +0x04..+0x10 but not +0x00 (0x582b60).
    void CopyAllButFirst(const MissionMemberSummary& other);
};

// The record handed to the checker callbacks (VT 0x66a090, 0x3c bytes, ctor 0x582b90, deleting dtor 0x582c00, dtor
// 0x582c20).
class MissionCheckArgs {
public:
    MissionCheckArgs(int a04, int a08, int a0c, int controlSide, DatRecord* record, void* refs1, void* refs2, void* refs3,
                     ObjectKeyValueSet* team, ObjectKeyValueSet* decoys, ObjectKeyValueSet* captured,
                     MissionMemberSummary* summary, GameObj* target, GameObj* objective);  // 0x582b90
    virtual ~MissionCheckArgs();                                                              // 0x582c20

    int m_flag04;                   // +0x04
    int m_flag08;                   // +0x08
    int m_flag0c;                   // +0x0c
    int m_controlSide;              // +0x10
    DatRecord* m_record;            // +0x14
    void* m_refs[3];                // +0x18
    ObjectKeyValueSet* m_team;      // +0x24
    ObjectKeyValueSet* m_decoys;    // +0x28
    ObjectKeyValueSet* m_captured;  // +0x2c
    MissionMemberSummary* m_summary;  // +0x30
    GameObj* m_target;              // +0x34
    GameObj* m_objective;           // +0x38
};

// The pair of check callbacks Mission::GetCheckFunctions reports: (0x523670 default) and (0x523450 default).
typedef int(* MissionMemberCheckFn)(int a, intptr_t value, intptr_t* out, StatusCode* status);
typedef int(* MissionTargetCheckFn)(MissionCheckArgs* args, StatusCode* status, int* code);


// Fills `summary` from the team, decoys and captured people (0x5830a0); false and a status in *status on failure.
int FillMemberSummaryForMission(ObjectKeyValueSet* team, ObjectKeyValueSet* decoys, ObjectKeyValueSet* captured,
                                        MissionMemberSummary* summary, StatusCode* status);  // 0x5830a0
// Adds one person to `summary` (0x582fb0).
int AddPersonToMemberSummary(GameObj* person, int a, int b, MissionMemberSummary* summary, StatusCode* status);  // 0x582fb0
// Checks the summary against the mission record (0x583320).
int CheckSummaryAgainstRecord(DatRecord* record, MissionMemberSummary* summary, StatusCode* status);  // 0x583320

// Person lookups used by the mission checks (0x582c30-0x582e90). Resolve a key to a person object (families 0x30-0x3f)
// or to its data-table record; the Split variants also say whether it is a character or a special force.
int ResolveRoleFromKey(ObjectKey* key, GameObj** out);                                                   // 0x582c30
int SplitRoleFromKey(ObjectKey* key, GameObj** person, GameObj** specialForces, GameObj** character);  // 0x582cd0
int ResolvePersonRecord(ObjectKey* key, DatRecord** out);                                                // 0x582dd0
int SplitPersonRecord(ObjectKey* key, DatRecord** person, DatRecord** specialForces, DatRecord** character);  // 0x582e90

// Picks the best leader of a team into *leader and *seed (0x521300, stdcall).
int ChooseMissionLeader(ObjectKeyValueSet* team, ObjectKey* leader, NullableKey* seed);
