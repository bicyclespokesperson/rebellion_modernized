#include "orders/mission_orders.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_queries.h"
#include "orders/route_context.h"

// The 0x50-byte command of order 0x242 (ctor 0x540c40, another module).
Command* __thiscall FUN_00540c40(void* memory);

// The mission manager functions the checks go through (0x541fc0-0x54cd80, another module); `self` is the mission
// manager, the master of family 0xf8.
class MissionMgr;
int __thiscall FUN_005420d0(MissionMgr* self, const ActivityRequest* request, ObjectKeyValueSet* team,
                            ObjectKeyValueSet* characters, ObjectKeyValueSet* out, StatusCode* status);
int __thiscall FUN_005422f0(MissionMgr* self, const ActivityRequest* request, ObjectKeyValueSet* team,
                            ObjectKeyValueSet* characters, ObjectKeyValueSet* out, StatusCode* status);
int __thiscall FUN_005425e0(MissionMgr* self, const ActivityRequest* request, ObjectKey* resultKey,
                            ObjectKeyValueSet* team, ObjectKeyValueSet* characters, ObjectKeyValueSet* out,
                            StatusCode* status);
int __thiscall FUN_00543410(MissionMgr* self, uint32_t side, NullableKey* missionKey, GameObjNotifier* target,
                            GameObjNotifier** mission, StatusCode* status);
// Whether the mission-type key is acceptable (cdecl).
int FUN_0051cab0(NullableKey* key);

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusRefused = 1;
constexpr int kStatusNoMembers = 0x16;
constexpr int kStatusObjectNotFound = 0x12;
constexpr int kStatusUnknownSide = 0x14;
constexpr int kStatusWrongSide = 0x28;
constexpr int kMissionCategory = 0x40;
constexpr int kStatusNoMissionKey = 0x92;
constexpr int kStatusBadMissionKey = 0x93;
constexpr int kStatusTargetNotFound = 0x20;
constexpr uint32_t kMissionTypeFamilyFirst = 0x40;
constexpr uint32_t kMissionTypeFamilyEnd = 0x50;
constexpr uint32_t kMissionManagerKey = 0xf8000006;
constexpr int kRouteMode = 3;

bool isOk(const StatusCode* status) { return status->m_second == static_cast<uint32_t>(kStatusOk); }

void setStatus(StatusCode* status, int category, int code) { *status = StatusCode(category, code); }

void setStatus(StatusCode* status, int code) { setStatus(status, kStatusCategory, code); }

MissionMgr* getMissionMgr() { return reinterpret_cast<MissionMgr*>(GetGameRootMember_b8()); }

void initRequest(ActivityRequest* request) {
    request->m_side = 0;
    request->m_unk04 = 0;
    request->m_unk08 = ObjectKey::kUnsetValue;
    request->m_kind = 0;
}

bool isMissionTypeKey(const NullableKey& key) {
    const uint32_t family = key.m_value >> 24;
    return family >= kMissionTypeFamilyFirst && family < kMissionTypeFamilyEnd;
}

uint32_t controlSideOf(const GameObjNotifier* object) { return object->ControlSide(); }

}  // namespace

// ---- Order242 --------------------------------------------------------------------------------------------------------

Order242::Order242() : OrderBase() {}

Order242::~Order242() { REB_KEEP_VPTR_STORE(); }

Command* Order242::CreateCommand() {
    void* memory = ::operator new(0x50);
    return memory ? FUN_00540c40(memory) : nullptr;
}

Order242* CreateOrder242() { return NewOrNull<Order242>(); }

// ---- MissionOrder ----------------------------------------------------------------------------------------------------

MissionOrder::MissionOrder() : OrderBase(), m_unk44(0), m_unk48(0) { m_missionKey.m_value = 0; }

MissionOrder::~MissionOrder() { REB_KEEP_VPTR_STORE(); }

int MissionOrder::Read(istream* in) {
    int ok = OrderBase::Read(in);
    if (ok) {
        StreamRead32(in, &m_unk44);
        StreamRead32(in, &m_unk48);
        StreamRead32(in, &m_missionKey);
        ok = 1;
        m_characters.Read(in);
    }
    return ok;
}

int MissionOrder::Write(ostream* out) {
    int ok = OrderBase::Write(out);
    if (ok) {
        StreamWrite32(out, &m_unk44);
        StreamWrite32(out, &m_unk48);
        StreamWrite32(out, &m_missionKey);
        ok = 1;
        m_characters.Write(out);
    }
    return ok;
}

int MissionOrder::CollectCandidates(StatusCode* status, ObjectKeyValueSet* out) {
    setStatus(status, kStatusOk);
    out->Clear();
    int ok = CheckValid(status);
    Command* command = nullptr;
    if (ok && isOk(status)) {
        command = m_commands->FirstCommand();
        ok = command != nullptr;
    }
    Command* usable = (ok && isOk(status)) ? command : nullptr;
    MissionMgr* manager = getMissionMgr();
    ok = manager && ok;
    ActivityRequest request;
    initRequest(&request);
    BuildRequest(&request);
    if (ok && manager && usable && isOk(status)) {
        ObjectKey* resultKey = &static_cast<MissionOrderCommand*>(usable)->m_resultKey;
        ok = FUN_005425e0(manager, &request, resultKey, GetTeam(), &m_characters, out, status);
    }
    return ok;
}

Command* MissionOrder::CreateCommand() { return NewOrNull<MissionOrderCommand>(); }

int MissionOrder::CheckCommand(StatusCode* status) {
    setStatus(status, kStatusOk);
    Command* created = CreateCommand();
    const bool made = created != nullptr;
    if (made) {
        if (isOk(status)) {
            MissionOrderCommand* command = static_cast<MissionOrderCommand*>(created);
            m_commands->Clear();
            ActivityRequest request;
            initRequest(&request);
            BuildRequest(&request);
            command->SetRequest(&request);
            command->m_unk50 = m_unk44;
            command->m_unk54 = m_unk48;
            command->m_team = *GetTeam();
            command->m_characters = m_characters;
            command->m_key.m_value = kMissionManagerKey;
            m_commands->Insert(command);
        }
    } else {
        setStatus(status, kStatusRefused);
    }
    return made;
}

int MissionOrder::CheckDestination(StatusCode* status) {
    int ok = static_cast<int>(m_unk1c);
    setStatus(status, kStatusOk);
    ActivityRequest request;
    initRequest(&request);
    BuildRequest(&request);

    uint32_t side = 0;
    if (ok) {
        ObjectKeyNode* first = GetTeam()->First();
        if (!first) first = m_characters.First();
        if (!first) setStatus(status, kStatusNoMembers);
        if (isOk(status)) {
            GameObjNotifier* firstObject = nullptr;
            ObjectKey firstKey;
            const StatusCode missing(kStatusCategory, kStatusObjectNotFound);
            ok = ResolveMasterInFamilies_01_ff(first->GetKey(&firstKey), &firstObject, &missing, status);
            if (ok && isOk(status)) side = controlSideOf(firstObject);
        }
        if (ok && isOk(status)) {
            if (m_missionKey.m_value == 0) {
                setStatus(status, kMissionCategory, kStatusNoMissionKey);
            } else if (FUN_0051cab0(&m_missionKey) == 0) {
                setStatus(status, kMissionCategory, kStatusBadMissionKey);
                ok = 0;
            }
        }
    }

    int view = 0;
    if (ok && isOk(status)) {
        if (side == 1) {
            view = 1;
        } else if (side == 2) {
            view = 2;
        } else {
            setStatus(status, kStatusUnknownSide);
        }
    }

    GameObjNotifier* target = nullptr;
    if (ok && isOk(status)) {
        ObjectKey targetKey;
        target = GetObjectInFamilies_01_ff(view, GetTarget(&targetKey));
        if (!target) setStatus(status, kMissionCategory, kStatusTargetNotFound);
    }

    GameObjNotifier* mission = nullptr;
    if (ok) {
        if (isOk(status)) {
            MissionMgr* manager = getMissionMgr();
            ok = manager != nullptr;
            if (ok) ok = FUN_00543410(manager, side, &m_missionKey, target, &mission, status);
        }
        if (ok && isOk(status) && isMissionTypeKey(m_missionKey) && controlSideOf(mission) != side) {
            setStatus(status, kStatusWrongSide);
        }
    }

    ObjectKey capacityKey;
    RouteContext route;
    if (ok) {
        if (isOk(status)) {
            const int filter = kRouteMode;
            ok = route.InitFromRequest(&request, static_cast<int>(side), &filter);
            if (ok) {
                ObjectKey missionKeyOfObject;
                ok = route.AddContainerLegs(mission->GetKey(&missionKeyOfObject));
            }
        }
        if (ok && isOk(status)) {
            CapacityScratch scratch;
            ObjectKey missionKeyOfObject;
            ok = route.CheckMoveSets(GetTeam(), &m_characters, mission->GetKey(&missionKeyOfObject), status, &scratch,
                                     &capacityKey);
        }
    }

    ObjectKey resultKey;
    if (ok && isOk(status)) {
        ObjectKey missionKeyOfObject;
        if (mission->GetKey(&missionKeyOfObject)->m_value == capacityKey.m_value) {
            ObjectKey targetKey;
            resultKey = *GetTarget(&targetKey);
        } else {
            resultKey = capacityKey;
        }
    }

    MissionOrderCommand* command = nullptr;
    if (ok) {
        if (isOk(status)) {
            command = static_cast<MissionOrderCommand*>(m_commands->FirstCommand());
            ok = command != nullptr;
        }
        if (ok && isOk(status)) {
            command->m_side = static_cast<int>(side);
            command->m_missionKey.m_value = m_missionKey.m_value;
            command->m_request.m_unk08 = static_cast<int>(resultKey.m_value);
            command->m_resultKey = capacityKey;
        }
    }
    if (!ok) setStatus(status, kStatusRefused);
    return ok;
}

MissionOrder240::MissionOrder240() : MissionOrder() {}

MissionOrder240::~MissionOrder240() { REB_KEEP_VPTR_STORE(); }

int MissionOrder240::CollectCandidatesA(ObjectKeyValueSet* out) {
    out->Clear();
    MissionMgr* manager = getMissionMgr();
    int ok = manager != nullptr;
    StatusCode status;
    ActivityRequest request;
    initRequest(&request);
    BuildRequest(&request);
    if (ok) ok = FUN_005420d0(manager, &request, GetTeam(), &m_characters, out, &status);
    return ok;
}

int MissionOrder240::CollectCandidatesB(ObjectKeyValueSet* out) {
    out->Clear();
    MissionMgr* manager = getMissionMgr();
    int ok = manager != nullptr;
    StatusCode status;
    ActivityRequest request;
    initRequest(&request);
    BuildRequest(&request);
    if (ok) ok = FUN_005422f0(manager, &request, GetTeam(), &m_characters, out, &status);
    return ok;
}

MissionOrder241::MissionOrder241() : MissionOrder() {}

MissionOrder241::~MissionOrder241() { REB_KEEP_VPTR_STORE(); }

MissionOrder240* CreateMissionOrder240() { return NewOrNull<MissionOrder240>(); }

MissionOrder241* CreateMissionOrder241() { return NewOrNull<MissionOrder241>(); }
