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

#include "story/mission_mgr.h"

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

// ---- the rules object (game root member +0xbc, type 0xf9, 0x554020-0x557410) ----
int __thiscall FUN_00556710(GameObjNotifier* rules, GameObj* person, NotifyContext* ctx);
int __thiscall FUN_00556390(GameObjNotifier* rules, ObjectKey* personKey, ObjectKey* locationKey, NotifyContext* ctx);
int __thiscall FUN_00556c70(GameObjNotifier* rules, ObjectKey* key, int a, NotifyContext* ctx);

// ---- per-class checks of the 0x58 mission family ----
int __thiscall FUN_00570470(Mission* mission, NotifyContext* ctx);
int __thiscall FUN_005704b0(Mission* mission, NotifyContext* ctx);

extern int DAT_006bb5b8;  // GNPRTB 0x1806, a percentage
