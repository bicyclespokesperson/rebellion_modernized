#include "core/checked_new.h"
#include "events/event_helpers.h"
#include "gameobj/object_queries.h"
#include "mission/mission.h"
#include "mission/mission_events.h"
#include "story/mission_mgr.h"

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusWrongObject = 0x12;

// Sets *status to success, then looks the object up with the lookup of its family range. Returns the lookup's result
// (0 if the object exists but lies outside the range); *object is null if the key names nothing.
template <typename Resolve>
int resolveWithStatus(Resolve resolve, ObjectKey* key, GameObj** object, StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    const StatusCode missing(kStatusCategory, kStatusWrongObject);
    return resolve(key, reinterpret_cast<GameObjNotifier**>(object), &missing, status);
}

int resolveMissionMgr(ObjectKey* key, MissionMgr** manager, StatusCode* status) {
    return resolveWithStatus(ResolveMasterInFamilies_f8_f9, key, reinterpret_cast<GameObj**>(manager), status);
}

int resolveMission(ObjectKey* key, Mission** mission, StatusCode* status) {
    return resolveWithStatus(ResolveMasterInFamilies_40_80, key, reinterpret_cast<GameObj**>(mission), status);
}

bool succeeded(const StatusCode& status) { return status.m_second == static_cast<uint32_t>(kStatusOk); }

}  // namespace

// ---- MissionOrderCommand (kind 0x250) --------------------------------------------------------------------------------

MissionOrderCommand::MissionOrderCommand() : Command(), m_unk50(0), m_unk54(0), m_side(0) {
    m_missionKey.m_value = 0;
}

MissionOrderCommand::MissionOrderCommand(const MissionOrderCommand& other) = default;

MissionOrderCommand::~MissionOrderCommand() = default;

Command* MissionOrderCommand::Clone() { return NewOrNull<MissionOrderCommand>(*this); }

void MissionOrderCommand::Read(istream* in) {
    ReadCommand(in);
    ReadValue32(in, &m_unk50);
    ReadValue32(in, &m_unk54);
    ReadValue32(in, &m_side);
    m_missionKey.Read(in);
    m_resultKey.Read(in);
    m_team.Load(in);
    m_characters.Load(in);
}

void MissionOrderCommand::Write(ostream* out) {
    WriteCommand(out);
    WriteValue32(out, &m_unk50);
    WriteValue32(out, &m_unk54);
    WriteValue32(out, &m_side);
    m_missionKey.Write(out);
    m_resultKey.Write(out);
    m_team.Save(out);
    m_characters.Save(out);
}

void MissionOrderCommand::Checksum(Crc32* crc) {
    int ok = ChecksumCommand(crc);
    crc->AddInt(m_unk50);
    crc->AddInt(m_unk54);
    crc->AddInt(m_side);
    ok = m_missionKey.Checksum(crc) && ok;
    ok = m_resultKey.Checksum(crc) && ok;
    ok = m_team.Checksum(crc) && ok;
    ok = m_characters.Checksum(crc) && ok;
    (void)ok;  // the slot is void in Command; the original's EAX result is not part of the interface
}

int MissionOrderCommand::CheckEnabled(StatusCode* status) {
    MissionMgr* manager = nullptr;
    int ok = resolveMissionMgr(&m_key, &manager, status);
    if (ok && succeeded(*status)) ok = manager->MissionMgr::CheckMissionOrderMembers(&m_request, m_unk50, m_unk54, &m_team, &m_characters, status);
    return ok;
}

int MissionOrderCommand::CheckValid(StatusCode* status) {
    MissionMgr* manager = nullptr;
    int ok = resolveMissionMgr(&m_key, &manager, status);
    if (ok && succeeded(*status)) {
        ok = manager->MissionMgr::CheckMissionOrder(&m_request, m_unk50, m_unk54, &m_missionKey, &m_resultKey, &m_team, &m_characters, status);
    }
    return ok;
}

int MissionOrderCommand::Execute() {
    NotifyContext ctx(m_context);
    MissionMgr* manager = nullptr;
    StatusCode status;
    int ok = resolveMissionMgr(&m_key, &manager, &status);
    if (ok && succeeded(status)) {
        ok = manager->MissionMgr::CheckMissionOrder(&m_request, m_unk50, m_unk54, &m_missionKey, &m_resultKey, &m_team, &m_characters, &status);
    }
    if (ok && succeeded(status)) {
        manager->CreateMissionForRequest(&m_request, m_unk50, m_unk54, m_side, &m_missionKey,
                                   &m_resultKey, &m_team, &m_characters, &ctx);
    }
    return 0;
}

MissionOrderCommand* CreateMissionOrderCommand() { return NewOrNull<MissionOrderCommand>(); }

// ---- MissionActivityCommand (kind 0x261) -----------------------------------------------------------------------------

MissionActivityCommand::MissionActivityCommand() : Command() {}

MissionActivityCommand::MissionActivityCommand(const MissionActivityCommand& other)  = default;

MissionActivityCommand::~MissionActivityCommand() = default;

Command* MissionActivityCommand::Clone() { return NewOrNull<MissionActivityCommand>(*this); }

int MissionActivityCommand::CheckEnabled(StatusCode* status) {
    Mission* mission = nullptr;
    int ok = resolveMission(&m_key, &mission, status);
    if (ok && succeeded(*status)) ok = mission->CheckActivity(&m_request, status);
    return ok;
}

int MissionActivityCommand::CheckValid(StatusCode* status) {
    Mission* mission = nullptr;
    int ok = resolveMission(&m_key, &mission, status);
    if (ok && succeeded(*status)) ok = mission->CheckActivityStatus(&m_request, status);
    return ok;
}

int MissionActivityCommand::Execute() {
    NotifyContext ctx(m_context);
    Mission* mission = nullptr;
    StatusCode status;
    if (resolveMission(&m_key, &mission, &status) && succeeded(status)) {
        mission->TryComplete(&m_request, &ctx);
    }
    return 0;
}
