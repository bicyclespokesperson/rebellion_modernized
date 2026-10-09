#include "mission/mission_bases.h"

#include <new>

#include "character/character.h"
#include "character/role.h"
#include "core/vptr_store.h"
#include "galaxy/mission_decl.h"
#include "galaxy/system.h"
#include "gameobj/object_database.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_queries.h"
#include "mission/mission_external.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));
void DestroyRuleBaseEventKinds();
void DestroyObjectTargetBaseEventKinds();
void DestroyTypeBaseEventKinds();
void DestroySystemTargetBaseEventKinds();
void DestroyCharacterTargetBaseEventKinds();
void DestroyStoryBaseEventKinds();

extern int g_ruleBaseEventKindsBuilt;               // 0x6bc018
extern EventKindSet g_ruleBaseEventKinds;           // 0x6bc020
extern int g_systemTargetBaseEventKindsBuilt;       // 0x6bbe38
extern EventKindSet g_systemTargetBaseEventKinds;   // 0x6bbe30
extern int g_characterTargetBaseEventKindsBuilt;    // 0x6bbe48
extern EventKindSet g_characterTargetBaseEventKinds;  // 0x6bbe40
extern int g_objectTargetBaseEventKindsBuilt;       // 0x6bc038
extern EventKindSet g_objectTargetBaseEventKinds;   // 0x6bc030
extern int g_typeBaseEventKindsBuilt;               // 0x6bc040
extern EventKindSet g_typeBaseEventKinds;           // 0x6bc048
extern int g_storyBaseEventKindsBuilt;              // 0x6bc060
extern EventKindSet g_storyBaseEventKinds;          // 0x6bc058

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusMissionCategory = 0x40;
constexpr uint32_t kCompleted = 0x4;
constexpr uint32_t kDestroyed = 0x8;
constexpr uint32_t kEnroute = 0x10;
constexpr int kPhaseScoring = 10;
constexpr int kPhaseFinished = 0xb;
constexpr FamilyRange kTargetKindFamilies(0x50, 0x60);

bool inFamilies(GameObjNotifier* object, uint32_t first, uint32_t last) {
    const uint32_t family = object->GetFamily();
    return family >= first && family < last;
}

uint32_t systemFlags(GameObj* system) { return static_cast<System*>(system)->m_flags; }

int checkOrderWith(int orderKind, StatusCode* out, int (*build)(), EventKindSet* kinds) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int result = build();
    if (result && kinds->Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return result;
}

int buildFromParent(int* built, EventKindSet* kinds, int (*initParent)(int, EventKindSet*)) {
    int result = 1;
    if (!*built) {
        *built = 1;
        result = initParent(0, kinds);
    }
    return result;
}

int initFrom(int (*build)(), EventKindSet* kinds, EventKindSet* out) {
    const int result = build();
    *out = *kinds;
    return result;
}

}  // namespace

// ==== MissionRuleBase ====================================================================================

MissionRuleBase::MissionRuleBase() {}

MissionRuleBase::~MissionRuleBase() { REB_KEEP_VPTR_STORE(); }

int MissionRuleBase::BuildEventKinds() {
    return buildFromParent(&g_ruleBaseEventKindsBuilt, &g_ruleBaseEventKinds, Mission::GetBaseEventKinds);
}

int MissionRuleBase::InitEventKinds(int unused, EventKindSet* out) {
    return initFrom(BuildEventKinds, &g_ruleBaseEventKinds, out);
}

int MissionRuleBase::IsFinalPhase() { return m_phase == 9; }

void ConstructMissionRuleBaseEventKinds() { new (&g_ruleBaseEventKinds) EventKindSet(); }  // 0x593360

void RegisterMissionRuleBaseEventKindsAtExit() { atexit(DestroyRuleBaseEventKinds); }  // 0x593370

void MissionRuleBase::InitStatics() {
    ConstructMissionRuleBaseEventKinds();
    RegisterMissionRuleBaseEventKindsAtExit();
}

int MissionRuleBase::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_ruleBaseEventKinds;
    return result;
}

int MissionRuleBase::CheckOrder(int a, int orderKind, StatusCode* out) {
    return checkOrderWith(orderKind, out, BuildEventKinds, &g_ruleBaseEventKinds);
}

int MissionRuleBase::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(&MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(&MissionRuleBase::CheckTarget);
    return 1;
}

int MissionRuleBase::AddInitialMembers(ObjectKeyValueSet* team, ObjectKeyValueSet* decoys, NotifyContext* ctx) {
    int ok = 1;
    for (AvlNode* node = team->First(); node; node = node->m_successor) {
        ObjectKey key;
        static_cast<ObjectKeyNode*>(node)->GetKey(&key);
        ok = (AddMember(&key, 0, ctx) && ok) ? 1 : 0;
    }
    for (AvlNode* node = decoys->First(); node; node = node->m_successor) {
        ObjectKey key;
        static_cast<ObjectKeyNode*>(node)->GetKey(&key);
        ok = (AddMember(&key, 1, ctx) && ok) ? 1 : 0;
    }
    return ok;
}

int MissionRuleBase::QueryPersonResult(GameObj* person, int* out) {
    *out = 0;
    return Virtual157(person, out) != 0;
}

int MissionRuleBase::ReadBody(istream* in) { return ReadImpl(in); }

int MissionRuleBase::WriteBody(ostream* out) { return WriteImpl(out); }

int MissionRuleBase::ApplyAllBody(NotifyContext* ctx) { return ApplyAllImpl(ctx); }

int __cdecl MissionRuleBase::CheckMember(int side, GameObj* person, GameObj** out, StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    *out = nullptr;
    int result = 0;
    if (side != 3) {
        const int view = side == 1 ? 1 : side == 2 ? 2 : 0;
        ObjectKey location;
        if ((person->m_stateFlags & kCompleted) == 0) {
            GameObjNotifier* builder = GetMasterInFamilies_01_ff(&person->m_builderKey);
            result = builder ? builder->GetOwnerKey(&location) : 0;
        } else {
            result = person->GetOwnerKey(&location);
        }
        if (location.OrNull() == nullptr) {
            const int reason = (person->m_stateFlags & kDestroyed) == 0 ? 0x20 : 0x21;
            *status = StatusCode(kStatusMissionCategory, reason);
        }
        if (status->m_second == static_cast<uint32_t>(kStatusOk)) {
            GameObjNotifier* object = (person->m_flags & 0x30) == 0 ? GetMasterInFamilies_01_ff(&location)
                                                                    : GetObjectInFamilies_01_ff(view, &location);
            *out = static_cast<GameObj*>(object);
            result = *out != nullptr;
        }
    }
    return result;
}

int __cdecl MissionRuleBase::CheckTarget(MissionCheckArgs* args, StatusCode* status, int* code) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    *code = 0;
    int ok = Mission::CheckTargetDefault(args, status, code);
    auto* record = static_cast<MissionSD*>(args->m_record);
    auto clear = [&] { return status->m_second == static_cast<uint32_t>(kStatusOk); };
    if (args->m_flag04 != 0 && args->m_flag0c != 0) {
        if (ok && clear() && !inFamilies(args->m_target, 0x90, 0xa0)) {
            *status = StatusCode(kStatusMissionCategory, 0x10);
            *code = -1;
        }
        if (ok && clear() && record->m_field70 != 0) {
            uint32_t populatedOrExplored = 1;
            if (inFamilies(args->m_target, 0x90, 0x98)) populatedOrExplored = (systemFlags(args->m_target) >> 1) & 1;
            if (populatedOrExplored == 0) {
                *status = StatusCode(kStatusMissionCategory, 0x11);
                *code = -1;
            }
        }
    }
    if (args->m_flag0c != 0) {
        if (ok) {
            if (clear() && record->m_targetLossEnds != 0 && (args->m_objective->m_stateFlags & kDestroyed) != 0) {
                *status = StatusCode(kStatusMissionCategory, 0x21);
                *code = 6;
            }
            if (ok) {
                if (clear() && (args->m_objective->m_stateFlags & kEnroute) != 0) {
                    *status = StatusCode(kStatusMissionCategory, 0x22);
                    *code = 6;
                }
                if (ok && clear()) {
                    ObjectKey objectiveLocation;
                    ObjectKey targetLocation;
                    ok = args->m_objective->GetOwnerKey(&objectiveLocation);
                    if (ok) ok = args->m_target->GetOwnerKey(&targetLocation);
                    if (!objectiveLocation.IsSet() || objectiveLocation.m_value != targetLocation.m_value) {
                        *status = StatusCode(kStatusMissionCategory, 0x20);
                        *code = 6;
                    }
                }
            }
        }
        if (args->m_flag0c != 0 && ok && clear() && record->m_needsPopulatedContainer != 0) {
            uint32_t populated = 0;
            if (inFamilies(args->m_target, 0x90, 0x98)) populated = systemFlags(args->m_target) & 1;
            if (populated == 0) {
                *status = StatusCode(kStatusMissionCategory, 0x16);
                *code = 0xd;
            }
        }
    }
    return ok;
}

int MissionRuleBase::Virtual161(NotifyContext* ctx) {
    int ok = 1;
    if (!CanAllTeamAct()) return ok;
    GameObj* targetSystem = nullptr;
    ok = GetTargetLocationSystem(&targetSystem) != 0;
    if (targetSystem && inFamilies(targetSystem, 0x90, 0x98)) {
        const int synced = static_cast<System*>(targetSystem)->SyncViewForEvent(6, (m_flags >> 6) & 3, ctx);
        ok = (synced && ok) ? 1 : 0;
    }
    GameObj* objectiveView = nullptr;
    GameObj* objective = nullptr;
    ok = (GetObjectiveInOwnerView(&objectiveView) && ok) ? 1 : 0;
    ok = (GetObjectiveObject(&objective) && ok) ? 1 : 0;
    if (objectiveView && objective && !inFamilies(objectiveView, 0x90, 0x98) && !inFamilies(objectiveView, 0x98, 0xa0)) {
        NotifyContext copy(*ctx);
        copy.m_flag04 = 1;
        copy.m_flag08 = 3;
        ObjectKey viewOwner;
        ObjectKey objectiveOwner;
        ok = (objectiveView->GetOwnerKey(&viewOwner) && ok) ? 1 : 0;
        ok = (objective->GetOwnerKey(&objectiveOwner) && ok) ? 1 : 0;
        if (m_targetLocationKey.m_value == objectiveOwner.m_value) {
            ok = (objectiveView->ApplyAll(&copy) && ok) ? 1 : 0;
        } else if (m_targetLocationKey.m_value == viewOwner.m_value) {
            ok = (objectiveView->UpdateSelfAndDescendants(&copy) && ok) ? 1 : 0;
        }
    }
    return ok;
}

int MissionRuleBase::OnPhaseChanged(int oldValue, int newValue, NotifyContext* ctx) {
    int ok = Mission::OnPhaseChanged(oldValue, newValue, ctx) != 0;
    if (newValue == kPhaseScoring) {
        ok = (Virtual158(ctx) && ok) ? 1 : 0;
        ObjectKeyValueSet scores;
        MissionCharacterTeamIterator characters(this);
        characters.Begin();
        while (characters.Current()) {
            int score = 0;
            GameObj* person = characters.Current();
            const int queried = QueryPersonResult(person, &score);
            const bool step = queried && ok;
            ObjectKey personKey;
            person->GetKey(&personKey);
            const int added = scores.AddKeyValue(&personKey, score);
            ok = (added && step) ? 1 : 0;
            characters.Advance();
        }
        while (AvlNode* first = scores.First()) {
            auto* best = static_cast<ObjectKeyValueNode*>(first);
            for (AvlNode* node = first; node; node = node->m_successor) {
                auto* candidate = static_cast<ObjectKeyValueNode*>(node);
                if (candidate->m_value < best->m_value) best = candidate;
            }
            ObjectKey bestKey;
            best->GetKey(&bestKey);
            GameObjNotifier* person = GetMasterInFamilies_30_3c(&bestKey);
            ok = (person && ok) ? 1 : 0;
            if (person) {
                int unused;
                const int applied = Virtual159(static_cast<GameObj*>(person), best->m_value, &unused, ctx);
                ok = (applied && ok) ? 1 : 0;
            }
            scores.RemoveKey(&bestKey);
        }
        MissionSpecialForcesTeamIterator specialForces(this);
        specialForces.Begin();
        while (specialForces.Current()) {
            int score = 0;
            const int queried = QueryPersonResult(specialForces.Current(), &score);
            const bool step = queried && ok;
            int unused;
            const int applied = Virtual159(specialForces.Current(), score, &unused, ctx);
            ok = (applied && step) ? 1 : 0;
            specialForces.Advance();
        }
        ok = (Virtual160(ctx) && ok) ? 1 : 0;
        ok = (Virtual161(ctx) && ok) ? 1 : 0;
    }
    if (newValue == kPhaseFinished) ok = (Virtual161(ctx) && ok) ? 1 : 0;
    return ok;
}

// ==== MissionSystemTargetBase ============================================================================

MissionSystemTargetBase::MissionSystemTargetBase() {}

MissionSystemTargetBase::~MissionSystemTargetBase() { REB_KEEP_VPTR_STORE(); }

int MissionSystemTargetBase::BuildEventKinds() {
    return buildFromParent(&g_systemTargetBaseEventKindsBuilt, &g_systemTargetBaseEventKinds,
                           MissionRuleBase::InitEventKinds);
}

int MissionSystemTargetBase::InitEventKinds(int unused, EventKindSet* out) {
    return initFrom(BuildEventKinds, &g_systemTargetBaseEventKinds, out);
}

void ConstructMissionSystemTargetBaseEventKinds() { new (&g_systemTargetBaseEventKinds) EventKindSet(); }  // 0x586b30

void RegisterMissionSystemTargetBaseEventKindsAtExit() { atexit(DestroySystemTargetBaseEventKinds); }  // 0x586b40

void MissionSystemTargetBase::InitStatics() {
    ConstructMissionSystemTargetBaseEventKinds();
    RegisterMissionSystemTargetBaseEventKindsAtExit();
}

int MissionSystemTargetBase::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_systemTargetBaseEventKinds;
    return result;
}

int MissionSystemTargetBase::CheckOrder(int a, int orderKind, StatusCode* out) {
    return checkOrderWith(orderKind, out, BuildEventKinds, &g_systemTargetBaseEventKinds);
}

int MissionSystemTargetBase::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(&MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(&MissionSystemTargetBase::CheckTarget);
    return 1;
}

int MissionSystemTargetBase::GetObjectiveSystem(GameObj** out) {
    *out = nullptr;
    GameObj* objective = nullptr;
    int found = 0;
    if (GetObjectiveObject(&objective)) {
        found = inFamilies(objective, 0x90, 0x98);
        if (found) *out = objective;
    }
    return found;
}

int __cdecl MissionSystemTargetBase::CheckTarget(MissionCheckArgs* args, StatusCode* status, int* code) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    *code = 0;
    const int ok = MissionRuleBase::CheckTarget(args, status, code);
    auto* record = static_cast<MissionSD*>(args->m_record);
    if (args->m_flag04 != 0 && args->m_flag0c != 0 && ok && status->m_second == static_cast<uint32_t>(kStatusOk)) {
        if (!inFamilies(args->m_objective, 0x90, 0x98)) {
            *status = StatusCode(kStatusMissionCategory, 0x24);
            *code = -1;
        }
    }
    if (args->m_flag0c != 0 && ok && status->m_second == static_cast<uint32_t>(kStatusOk)) {
        const uint32_t uprising = (systemFlags(args->m_objective) >> 2) & 1;
        if (uprising == 0 || record->m_revoltingTarget != 0) {
            if (uprising != 0) return ok;
            if (record->m_calmTarget != 0) return ok;
        }
        *status = StatusCode(kStatusMissionCategory, 0x20);
        *code = 6;
    }
    return ok;
}

MissionTargetKindIterator::MissionTargetKindIterator(GameObjNotifier* parent)
    : ChildIterator(parent, &kTargetKindFamilies, 1) {}

MissionTargetKindIterator::~MissionTargetKindIterator() { REB_KEEP_VPTR_STORE(); }

// ==== MissionCharacterTargetBase =========================================================================

MissionCharacterTargetBase::MissionCharacterTargetBase() {}

MissionCharacterTargetBase::~MissionCharacterTargetBase() { REB_KEEP_VPTR_STORE(); }

int MissionCharacterTargetBase::BuildEventKinds() {
    return buildFromParent(&g_characterTargetBaseEventKindsBuilt, &g_characterTargetBaseEventKinds,
                           MissionRuleBase::InitEventKinds);
}

int MissionCharacterTargetBase::InitEventKinds(int unused, EventKindSet* out) {
    return initFrom(BuildEventKinds, &g_characterTargetBaseEventKinds, out);
}

void ConstructMissionCharacterTargetBaseEventKinds() { new (&g_characterTargetBaseEventKinds) EventKindSet(); }  // 0x5870c0

void RegisterMissionCharacterTargetBaseEventKindsAtExit() { atexit(DestroyCharacterTargetBaseEventKinds); }  // 0x5870d0

void MissionCharacterTargetBase::InitStatics() {
    ConstructMissionCharacterTargetBaseEventKinds();
    RegisterMissionCharacterTargetBaseEventKindsAtExit();
}

int MissionCharacterTargetBase::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_characterTargetBaseEventKinds;
    return result;
}

int MissionCharacterTargetBase::CheckOrder(int a, int orderKind, StatusCode* out) {
    return checkOrderWith(orderKind, out, BuildEventKinds, &g_characterTargetBaseEventKinds);
}

int MissionCharacterTargetBase::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(&MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(&MissionCharacterTargetBase::CheckTarget);
    return 1;
}

int MissionCharacterTargetBase::GetObjectiveCharacter(GameObj** out) {
    *out = nullptr;
    GameObj* objective = nullptr;
    int found = 0;
    if (GetObjectiveObject(&objective)) {
        found = inFamilies(objective, 0x30, 0x3c);
        if (found) *out = objective;
    }
    return found;
}

int __cdecl MissionCharacterTargetBase::CheckTarget(MissionCheckArgs* args, StatusCode* status, int* code) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    *code = 0;
    const int ok = MissionRuleBase::CheckTarget(args, status, code);
    auto* record = static_cast<MissionSD*>(args->m_record);
    if (args->m_flag04 != 0 && args->m_flag0c != 0 && ok && status->m_second == static_cast<uint32_t>(kStatusOk)) {
        if (!inFamilies(args->m_objective, 0x30, 0x3c)) {
            *status = StatusCode(kStatusMissionCategory, 0x26);
            *code = -1;
        }
    }
    if (args->m_flag0c != 0 && ok && inFamilies(args->m_objective, 0x30, 0x3c)) {
        const uint32_t captured = static_cast<Character*>(args->m_objective)->m_flagsAc & 1;
        if (captured == 0 || record->m_prisonerTarget != 0) {
            if (captured != 0) return ok;
            if (record->m_freeTarget != 0) return ok;
            *status = StatusCode(kStatusMissionCategory, 0x2b);
        } else {
            *status = StatusCode(kStatusMissionCategory, 0x2a);
        }
        *code = 6;
    }
    return ok;
}

// ==== MissionObjectTargetBase ============================================================================

MissionObjectTargetBase::MissionObjectTargetBase() {}

MissionObjectTargetBase::~MissionObjectTargetBase() { REB_KEEP_VPTR_STORE(); }

int MissionObjectTargetBase::BuildEventKinds() {
    return buildFromParent(&g_objectTargetBaseEventKindsBuilt, &g_objectTargetBaseEventKinds,
                           MissionRuleBase::InitEventKinds);
}

int MissionObjectTargetBase::InitEventKinds(int unused, EventKindSet* out) {
    return initFrom(BuildEventKinds, &g_objectTargetBaseEventKinds, out);
}

void ConstructMissionObjectTargetBaseEventKinds() { new (&g_objectTargetBaseEventKinds) EventKindSet(); }  // 0x5936f0

void RegisterMissionObjectTargetBaseEventKindsAtExit() { atexit(DestroyObjectTargetBaseEventKinds); }  // 0x593700

void MissionObjectTargetBase::InitStatics() {
    ConstructMissionObjectTargetBaseEventKinds();
    RegisterMissionObjectTargetBaseEventKindsAtExit();
}

int MissionObjectTargetBase::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_objectTargetBaseEventKinds;
    return result;
}

int MissionObjectTargetBase::CheckOrder(int a, int orderKind, StatusCode* out) {
    return checkOrderWith(orderKind, out, BuildEventKinds, &g_objectTargetBaseEventKinds);
}

int MissionObjectTargetBase::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(&MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(&MissionObjectTargetBase::CheckTarget);
    return 1;
}

int __cdecl MissionObjectTargetBase::CheckTarget(MissionCheckArgs* args, StatusCode* status, int* code) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    *code = 0;
    const int ok = MissionRuleBase::CheckTarget(args, status, code);
    if (args->m_flag04 != 0 && args->m_flag0c != 0) {
        if (ok && status->m_second == static_cast<uint32_t>(kStatusOk) && !inFamilies(args->m_objective, 0x10, 0x40)) {
            *status = StatusCode(kStatusMissionCategory, 0x27);
            *code = -1;
        }
        if (ok && status->m_second == static_cast<uint32_t>(kStatusOk) &&
            (args->m_objective->m_stateFlags & kCompleted) == 0) {
            *status = StatusCode(kStatusMissionCategory, 0x34);
            *code = -1;
        }
    }
    return ok;
}

// ==== MissionStoryBase ===================================================================================

MissionStoryBase::MissionStoryBase() {}

MissionStoryBase::~MissionStoryBase() { REB_KEEP_VPTR_STORE(); }

int MissionStoryBase::BuildEventKinds() {
    return buildFromParent(&g_storyBaseEventKindsBuilt, &g_storyBaseEventKinds, MissionRuleBase::InitEventKinds);
}

int MissionStoryBase::InitEventKinds(int unused, EventKindSet* out) {
    return initFrom(BuildEventKinds, &g_storyBaseEventKinds, out);
}

void ConstructMissionStoryBaseEventKinds() { new (&g_storyBaseEventKinds) EventKindSet(); }  // 0x5944e0

void RegisterMissionStoryBaseEventKindsAtExit() { atexit(DestroyStoryBaseEventKinds); }  // 0x5944f0

void MissionStoryBase::InitStatics() {
    ConstructMissionStoryBaseEventKinds();
    RegisterMissionStoryBaseEventKindsAtExit();
}

int MissionStoryBase::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_storyBaseEventKinds;
    return result;
}

int MissionStoryBase::CheckOrder(int a, int orderKind, StatusCode* out) {
    return checkOrderWith(orderKind, out, BuildEventKinds, &g_storyBaseEventKinds);
}

int MissionStoryBase::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(&MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(&MissionStoryBase::CheckTarget);
    return 1;
}

int MissionStoryBase::OnPhaseChanged(int oldValue, int newValue, NotifyContext* ctx) {
    return MissionRuleBase::OnPhaseChanged(oldValue, newValue, ctx);
}

int __cdecl MissionStoryBase::CheckTarget(MissionCheckArgs* args, StatusCode* status, int* code) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    *code = 0;
    const int ok = MissionRuleBase::CheckTarget(args, status, code);
    if (args->m_flag04 != 0 && args->m_flag0c != 0 && ok && status->m_second == static_cast<uint32_t>(kStatusOk)) {
        if (!inFamilies(args->m_objective, 0x98, 0xa0)) {
            *status = StatusCode(kStatusMissionCategory, 0x2d);
            *code = -1;
        }
    }
    return ok;
}

// ==== MissionTypeBase (declared in galaxy/mission_decl.h) ================================================

int MissionTypeBase::BuildEventKinds() {
    return buildFromParent(&g_typeBaseEventKindsBuilt, &g_typeBaseEventKinds, Mission::GetBaseEventKinds);
}

int MissionTypeBase::InitEventKinds(int unused, EventKindSet* out) {
    return initFrom(BuildEventKinds, &g_typeBaseEventKinds, out);
}

void ConstructMissionTypeBaseEventKinds() { new (&g_typeBaseEventKinds) EventKindSet(); }  // 0x593b50

void RegisterMissionTypeBaseEventKindsAtExit() { atexit(DestroyTypeBaseEventKinds); }  // 0x593b60

void MissionTypeBase::InitStatics() {
    ConstructMissionTypeBaseEventKinds();
    RegisterMissionTypeBaseEventKindsAtExit();
}

int MissionTypeBase::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_typeBaseEventKinds;
    return result;
}

int MissionTypeBase::CheckOrder(int a, int orderKind, StatusCode* out) {
    return checkOrderWith(orderKind, out, BuildEventKinds, &g_typeBaseEventKinds);
}

int MissionTypeBase::AddInitialMembers(ObjectKeyValueSet* team, ObjectKeyValueSet* decoys, NotifyContext* ctx) {
    ObjectKey leader;
    NullableKey seed;
    seed.m_value = 0;
    bool ok;
    if (ChooseMissionLeader(team, &leader, &seed) == 0) {
        ok = false;
    } else {
        ok = true;
        if ((m_flagsA4 & mission_flag::kImpliedTeam) == 0) ok = leader.IsSet() != 0;
    }
    for (AvlNode* node = team->First(); node; node = node->m_successor) {
        ObjectKey key;
        static_cast<ObjectKeyNode*>(node)->GetKey(&key);
        Role* person = static_cast<Role*>(GetMasterInFamilies_30_40(&key));
        int decoy = 0;
        if (leader.IsSet()) {
            if (key.m_value != leader.m_value && person && person->Virtual117() == 0) decoy = 1;
        }
        ok = (AddMember(&key, decoy, ctx) && ok);
    }
    for (AvlNode* node = decoys->First(); node; node = node->m_successor) {
        ObjectKey key;
        static_cast<ObjectKeyNode*>(node)->GetKey(&key);
        ok = (AddMember(&key, 1, ctx) && ok);
    }
    return ok;
}

MissionTypeBase::MissionTypeBase() {}

MissionTypeBase::~MissionTypeBase() { REB_KEEP_VPTR_STORE(); }

int MissionTypeBase::CreateViewsBody(int controlKind, NotifyContext* ctx) { return CreateViewsImpl(controlKind, ctx); }

int MissionTypeBase::ApplyAllBody(NotifyContext* ctx) {
    const int result = ApplyAllImpl(ctx);
    // BUG(original): the body is applied a second time when the first succeeded.
    if (result) ApplyAllImpl(ctx);
    return result;
}

int MissionTypeBase::ChecksumBody(Crc32* crc) { return ChecksumImpl(crc); }

// ---- destructors registered with atexit ----------------------------------------------------

extern uint8_t g_systemTargetBaseEventKindsDestroyedGuard;  // 0x6bbe3c

void DestroySystemTargetBaseEventKinds() {
    if (!(g_systemTargetBaseEventKindsDestroyedGuard & 0x1)) {
        g_systemTargetBaseEventKindsDestroyedGuard |= 0x1;
        g_systemTargetBaseEventKinds.~EventKindSet();
    }
}

extern uint8_t g_characterTargetBaseEventKindsDestroyedGuard;  // 0x6bbe4c

void DestroyCharacterTargetBaseEventKinds() {
    if (!(g_characterTargetBaseEventKindsDestroyedGuard & 0x1)) {
        g_characterTargetBaseEventKindsDestroyedGuard |= 0x1;
        g_characterTargetBaseEventKinds.~EventKindSet();
    }
}

extern uint8_t g_ruleBaseEventKindsDestroyedGuard;  // 0x6bc028

void DestroyRuleBaseEventKinds() {
    if (!(g_ruleBaseEventKindsDestroyedGuard & 0x1)) {
        g_ruleBaseEventKindsDestroyedGuard |= 0x1;
        g_ruleBaseEventKinds.~EventKindSet();
    }
}

extern uint8_t g_objectTargetBaseEventKindsDestroyedGuard;  // 0x6bc03c

void DestroyObjectTargetBaseEventKinds() {
    if (!(g_objectTargetBaseEventKindsDestroyedGuard & 0x1)) {
        g_objectTargetBaseEventKindsDestroyedGuard |= 0x1;
        g_objectTargetBaseEventKinds.~EventKindSet();
    }
}

extern uint8_t g_typeBaseEventKindsDestroyedGuard;  // 0x6bc050

void DestroyTypeBaseEventKinds() {
    if (!(g_typeBaseEventKindsDestroyedGuard & 0x1)) {
        g_typeBaseEventKindsDestroyedGuard |= 0x1;
        g_typeBaseEventKinds.~EventKindSet();
    }
}

extern uint8_t g_storyBaseEventKindsDestroyedGuard;  // 0x6bc064

void DestroyStoryBaseEventKinds() {
    if (!(g_storyBaseEventKindsDestroyedGuard & 0x1)) {
        g_storyBaseEventKindsDestroyedGuard |= 0x1;
        g_storyBaseEventKinds.~EventKindSet();
    }
}
