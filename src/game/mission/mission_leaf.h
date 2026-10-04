#pragma once
#include <cstdint>

#include "character/role.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "galaxy/system.h"
#include "gameobj/object_family.h"
#include "mission/mission_bases.h"

// Declarations shared by the concrete mission types (MissionSubdue, MissionSabotage, ...): the functions of the mission
// bases and of the rules code that every type calls, known by address only (the bases' owner may give them names).

extern "C" int __cdecl atexit(void(__cdecl* function)(void));

namespace mission_status {
constexpr int kCategory = 1;
constexpr int kOk = -1;
constexpr int kNotSupported = 0x17;
}  // namespace mission_status

// The record the order-check callbacks of slot 111 receive (fields +0x04, +0x0c, +0x14, +0x38 are read by the bases).
struct MissionOrderCheck {
    uint8_t m_unk00[4];
    void* m_unk04;           // +0x04
    uint8_t m_unk08[4];
    void* m_unk0c;           // +0x0c
    uint8_t m_unk10[0x28];
    GameObjRoot* m_target;   // +0x38, tested for its family
};

// Slot-111 callbacks of the bases: (kind, person-ish object, out, status) for the member check, (check, status, out) for
// the target checks of each base.
int __cdecl FUN_00592aa0(int kind, void* object, int* out, StatusCode* status);
int __cdecl FUN_005868c0(MissionOrderCheck* check, StatusCode* status, int* out);

// MissionSystemTargetBase: the target location as a system (0 if it is not one); static event kind set builder.
int __thiscall FUN_00586720(Mission* self, System** out);
int __cdecl FUN_00586b00(int unused, EventKindSet* out);

// MissionCharacterTargetBase: the target as a character (0 if it is not one), its order check and event kind builder.
class Character;
int __thiscall FUN_00586c80(Mission* self, Character** out);
int __cdecl FUN_00586e20(MissionOrderCheck* check, StatusCode* status, int* out);
int __cdecl FUN_00587090(int unused, EventKindSet* out);

// MissionCharacterTargetBase (the target is a character; Rescue) and MissionObjectTargetBase (Sabotage, DS Sabotage).
int __thiscall FUN_00586c80(Mission* self, Role** out);
int __cdecl FUN_00593500(MissionOrderCheck* check, StatusCode* status, int* out);
int __cdecl FUN_005936c0(int unused, EventKindSet* out);

// Bodies of the persistence slots of the rule bases (what the shared first-level forwarders end up calling).
int __thiscall FUN_00592c50(Mission* self, istream* in);
int __thiscall FUN_00593ad0(Mission* self, Crc32* crc);
int __thiscall FUN_00523c60(Mission* self);
int __thiscall FUN_005236e0(Mission* self, int controlKind, NotifyContext* ctx);

inline int FamilyInRange(uint32_t familyValue, const FamilyRange& range) {
    return familyValue >= range.m_first && familyValue < range.m_second;
}

inline StatusCode MakeStatus(int code) { return StatusCode(mission_status::kCategory, code); }

// Behaviour of the per-type event kind sets: build the static set once from the base's, then copy it / test it.
template <class BuildFn>
inline int CopyEventKinds(BuildFn build, EventKindSet* source, EventKindSet* out) {
    const int result = build();
    if (result) *out = *source;
    return result;
}

template <class BuildFn>
inline int CheckOrderAgainstKinds(BuildFn build, EventKindSet* kinds, int orderKind, StatusCode* out) {
    *out = MakeStatus(mission_status::kNotSupported);
    const int result = build();
    if (result && kinds->Contains(orderKind)) *out = MakeStatus(mission_status::kOk);
    return result;
}

// The slot 5-9 and 37 bodies shared (identical-code-folded) by almost all concrete types. A type that has its own
// second-level forwarders still ends in the same rule-base bodies.
#define REB_MISSION_SHARED_PERSISTENCE(CLASS)                                                                      \
    int CLASS::Read(istream* in) { return FUN_00592c50(this, in); }                                               \
    int CLASS::Write(ostream* out) { return WriteImpl(out); }                                            \
    int CLASS::Checksum(Crc32* crc) { return FUN_00593ad0(this, crc); }                                           \
    int CLASS::Validate() { return FUN_00523c60(this); }                                                          \
    int CLASS::ApplyAll(NotifyContext* ctx) { return IsSideView() ? ApplyAllImpl(ctx) : 0; }                \
    int CLASS::CreateViews(int controlKind, NotifyContext* ctx) { return FUN_005236e0(this, controlKind, ctx); }

#define REB_MISSION_SHARED_PERSISTENCE_DECLS                                                                       \
    virtual int Read(istream* in) override;                                                                       \
    virtual int Write(ostream* out) override;                                                                     \
    virtual int Checksum(Crc32* crc) override;                                                                    \
    virtual int Validate() override;                                                                              \
    virtual int ApplyAll(NotifyContext* ctx) override;                                                            \
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;

// Slot 160 body shared by Subdue, Rescue, Recruit, Pickup, Jedi Training and Incite (0x576700): a mission that is
// still in TaskStatus 0 starts (TaskStatus 2).
int __thiscall MissionStartIfIdle(Mission* mission, NotifyContext* ctx);

// Slot 160 body shared by Sabotage and DS Sabotage (0x5746e0): start when idle; once the task has succeeded the object the
// mission started from is destroyed (reason 6).
int __thiscall MissionDestroyOriginOnSuccess(Mission* mission, NotifyContext* ctx);
