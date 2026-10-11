#pragma once
#include <cstdint>

#include "character/role.h"
#include "core/checked_new.h"
#include "galaxy/mission_decl.h"
#include "galaxy/system.h"
#include "gameobj/object_family.h"
#include "mission/mission_bases.h"
#include "mission/mission_external.h"

// Declarations shared by the concrete mission types (MissionSubdue, MissionSabotage, ...): the functions of the mission
// bases and of the rules code that every type calls, known by address only (the bases' owner may give them names).


namespace mission_status {
constexpr int kCategory = 1;
constexpr int kOk = -1;
constexpr int kNotSupported = 0x17;
}  // namespace mission_status

// MissionCharacterTargetBase: the target as a character (0 if it is not one), its order check and event kind builder.
class Character;

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
    int CLASS::Read(istream* in) { return this->Mission::ReadImpl(in); }                                               \
    int CLASS::Write(ostream* out) { return WriteImpl(out); }                                            \
    int CLASS::Checksum(Crc32* crc) { return this->Mission::ChecksumImpl(crc); }                                           \
    int CLASS::Validate() { return Mission::ValidateImpl(); }                                                          \
    int CLASS::ApplyAll(NotifyContext* ctx) { return IsSideView() ? ApplyAllImpl(ctx) : 0; }                \
    int CLASS::CreateViews(int controlKind, NotifyContext* ctx) { return Mission::CreateViewsImpl(controlKind, ctx); }

#define REB_MISSION_SHARED_PERSISTENCE_DECLS                                                                       \
    virtual int Read(istream* in) override;                                                                       \
    virtual int Write(ostream* out) override;                                                                     \
    virtual int Checksum(Crc32* crc) override;                                                                    \
    virtual int Validate() override;                                                                              \
    virtual int ApplyAll(NotifyContext* ctx) override;                                                            \
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;

// Slot 160 body shared by Subdue, Rescue, Recruit, Pickup, Jedi Training and Incite (0x576700): a mission that is
// still in TaskStatus 0 starts (TaskStatus 2).
int MissionStartIfIdle(Mission* mission, NotifyContext* ctx);

// Slot 160 body shared by Sabotage and DS Sabotage (0x5746e0): start when idle; once the task has succeeded the object the
// mission started from is destroyed (reason 6).
int MissionDestroyOriginOnSuccess(Mission* mission, NotifyContext* ctx);
