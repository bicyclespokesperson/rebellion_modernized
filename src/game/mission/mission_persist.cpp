#include "character/role.h"
#include "core/utility_classes.h"
#include "gameobj/side_bits.h"
#include "mission/mission.h"

namespace {

constexpr uint32_t kControlSideBits = 0xc0;
constexpr uint32_t kExisting = 0x40;
constexpr uint32_t kEnrouteActive = 0x20;
constexpr uint32_t kHyperdriveActive = 0x800;
constexpr uint32_t kRemoveOrResignRequests = role_flag::kMissionRemoveRequest | role_flag::kMissionResignRequest;

ObjectKey* seedAsObjectKey(NullableKey* key) { return reinterpret_cast<ObjectKey*>(key); }

bool isActiveMember(Mission* mission, Role* person) {
    ObjectKey missionKey;
    mission->GetKey(&missionKey);
    uint32_t valid = person->m_missionKey.m_value == missionKey.m_value;
    if (valid) {
        valid = ((mission->m_flags ^ person->m_flags) & kControlSideBits) == 0;
        if (valid) {
            valid = (person->m_stateFlags & kExisting) != 0;
            if (valid) {
                valid = (person->m_flags78 & kRemoveOrResignRequests) == 0;
                if (valid && (person->m_stateFlags & kEnrouteActive) != 0) {
                    valid = (person->m_stateFlags & kHyperdriveActive) != 0;
                }
            }
        }
    }
    return valid != 0;
}

}  // namespace

int Mission::ReadImpl(istream* in) {
    int result = GameObj::Read(in);
    if (!result) return result;
    ReadValue32(in, &m_userId);
    ReadValue32(in, &m_userId2);
    ReadValue32(in, &m_taskStatus);
    ReadValue32(in, &m_completionStatus);
    ReadValue32(in, &m_phase);
    if (!m_originLocationKey.Read(in)) return 0;
    if (!m_objectiveKey.Read(in)) return 0;
    if (!m_targetKey.Read(in)) return 0;
    if (!m_targetLocationKey.Read(in)) return 0;
    if (!m_leaderKey.Read(in)) return 0;
    result = seedAsObjectKey(&m_leaderSeedKey)->Read(in);
    if (result) {
        m_team.Read(in);
        m_decoys.Read(in);
        m_captured.Read(in);
        m_finishedMembers.Read(in);
        ReadValue32(in, &m_flagsA4);
    }
    return result;
}

int Mission::WriteImpl(ostream* out) {
    int result = GameObj::Write(out);
    if (!result) return result;
    WriteValue32(out, &m_userId);
    WriteValue32(out, &m_userId2);
    WriteValue32(out, &m_taskStatus);
    WriteValue32(out, &m_completionStatus);
    WriteValue32(out, &m_phase);
    if (!m_originLocationKey.Write(out)) return 0;
    if (!m_objectiveKey.Write(out)) return 0;
    if (!m_targetKey.Write(out)) return 0;
    if (!m_targetLocationKey.Write(out)) return 0;
    if (!m_leaderKey.Write(out)) return 0;
    result = seedAsObjectKey(&m_leaderSeedKey)->Write(out);
    if (result) {
        m_team.Write(out);
        m_decoys.Write(out);
        m_captured.Write(out);
        m_finishedMembers.Write(out);
        WriteValue32(out, &m_flagsA4);
    }
    return result;
}

int Mission::ChecksumImpl(Crc32* crc) {
    int result = GameObj::Checksum(crc);
    if (!result) return result;
    crc->AddInt(m_userId);
    crc->AddInt(m_userId2);
    crc->AddInt(m_taskStatus);
    crc->AddInt(m_completionStatus);
    crc->AddInt(m_phase);
    auto checksumKey = [crc](void* key) { return reinterpret_cast<SideBits*>(key)->Checksum(crc); };
    if (!checksumKey(&m_originLocationKey)) return 0;
    if (!checksumKey(&m_objectiveKey)) return 0;
    if (!checksumKey(&m_targetKey)) return 0;
    if (!checksumKey(&m_targetLocationKey)) return 0;
    if (!checksumKey(&m_leaderKey)) return 0;
    if (!checksumKey(&m_leaderSeedKey)) return 0;
    if (!m_team.Checksum(crc)) return 0;
    if (!m_decoys.Checksum(crc)) return 0;
    if (!m_captured.Checksum(crc)) return 0;
    result = m_finishedMembers.Checksum(crc);
    if (result) crc->AddInt(m_flagsA4);
    return result;
}

int Mission::ValidateImpl() {
    int valid = GameObj::Validate();
    if (valid) {
        MissionPersonIterator members(this);
        members.Begin();
        do {
            if (!members.Current()) break;
            if (valid) valid = isActiveMember(this, static_cast<Role*>(members.Current()));
            members.Advance();
        } while (valid);
    }
    return valid;
}

int Mission::ApplyAllImpl(NotifyContext* ctx) {
    const int result = GameObj::ApplyAll(ctx);
    if (result && ctx->m_flag04 == 1) {
        ApplyUserId(ctx);
        ApplyUserId2(ctx);
        ApplyTaskStatus(ctx);
        ApplyCompletionStatus(ctx);
        ApplyPhase(ctx);
        ApplyOriginLocationKey(ctx);
        ApplyObjectiveKey(ctx);
        ApplyTargetKey(ctx);
        ApplyTargetLocationKey(ctx);
        ApplyLeaderKey(ctx);
        ApplyLeaderSeedKey(ctx);
        ApplyTeam(ctx);
        ApplyDecoys(ctx);
        ApplyCaptured(ctx);
        ApplyFinishedMembers(ctx);
        ApplyReadyForNextPhase(ctx);
        ApplyImpliedTeam(ctx);
        ApplyMandatory(ctx);
    }
    return result;
}
