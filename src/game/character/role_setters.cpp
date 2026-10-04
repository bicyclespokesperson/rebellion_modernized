#include "character/role.h"

#include "gameobj/game_obj_root.h"

// Range check helper of the notification hub (0x53fc90); the message is ignored by the release build.
int FUN_0053fc90(int value, int low, int high, const char* message);
// Pure clamp (0x53e0f0); the setters call it and ignore the result.
int Clamp(int value, int low, int high);

namespace {

constexpr int kAllianceView = 1;
constexpr int kEmpireView = 2;
constexpr int kMaxSkillValue = 0x7fff;
constexpr int kMaxLoyalty = 100;

}  // namespace

int Role::SetBaseDiplomacy(int value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result) {
        Clamp(value, 0, kMaxSkillValue);
        if (m_baseDiplomacy != value) {
            const int oldValue = m_baseDiplomacy;
            m_baseDiplomacy = static_cast<int16_t>(value);
            ViewAsRole(kAllianceView)->ApplyBaseDiplomacy(ctx);
            ViewAsRole(kEmpireView)->ApplyBaseDiplomacy(ctx);
            OnBaseDiplomacyChanged(oldValue, m_baseDiplomacy, ctx);
        }
    }
    return result;
}

int Role::SetBaseEspionage(int value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result) {
        Clamp(value, 0, kMaxSkillValue);
        if (m_baseEspionage != value) {
            const int oldValue = m_baseEspionage;
            m_baseEspionage = static_cast<int16_t>(value);
            ViewAsRole(kAllianceView)->ApplyBaseEspionage(ctx);
            ViewAsRole(kEmpireView)->ApplyBaseEspionage(ctx);
            OnBaseEspionageChanged(oldValue, m_baseEspionage, ctx);
        }
    }
    return result;
}

int Role::SetBaseShipyardRD(int value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result) {
        Clamp(value, 0, kMaxSkillValue);
        if (m_baseShipyardRD != value) {
            const int oldValue = m_baseShipyardRD;
            m_baseShipyardRD = static_cast<int16_t>(value);
            ViewAsRole(kAllianceView)->ApplyBaseShipyardRD(ctx);
            ViewAsRole(kEmpireView)->ApplyBaseShipyardRD(ctx);
            OnBaseShipyardRDChanged(oldValue, m_baseShipyardRD, ctx);
        }
    }
    return result;
}

int Role::SetBaseTrainingFacilRD(int value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result) {
        Clamp(value, 0, kMaxSkillValue);
        if (m_baseTrainingFacilRD != value) {
            const int oldValue = m_baseTrainingFacilRD;
            m_baseTrainingFacilRD = static_cast<int16_t>(value);
            ViewAsRole(kAllianceView)->ApplyBaseTrainingFacilRD(ctx);
            ViewAsRole(kEmpireView)->ApplyBaseTrainingFacilRD(ctx);
            OnBaseTrainingFacilRDChanged(oldValue, m_baseTrainingFacilRD, ctx);
        }
    }
    return result;
}

int Role::SetBaseConstructionYardRD(int value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result) {
        Clamp(value, 0, kMaxSkillValue);
        if (m_baseConstructionYardRD != value) {
            const int oldValue = m_baseConstructionYardRD;
            m_baseConstructionYardRD = static_cast<int16_t>(value);
            ViewAsRole(kAllianceView)->ApplyBaseConstructionYardRD(ctx);
            ViewAsRole(kEmpireView)->ApplyBaseConstructionYardRD(ctx);
            OnBaseConstructionYardRDChanged(oldValue, m_baseConstructionYardRD, ctx);
        }
    }
    return result;
}

int Role::SetBaseCombat(int value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result) {
        Clamp(value, 0, kMaxSkillValue);
        if (m_baseCombat != value) {
            const int oldValue = m_baseCombat;
            m_baseCombat = static_cast<int16_t>(value);
            ViewAsRole(kAllianceView)->ApplyBaseCombat(ctx);
            ViewAsRole(kEmpireView)->ApplyBaseCombat(ctx);
            OnBaseCombatChanged(oldValue, m_baseCombat, ctx);
        }
    }
    return result;
}

int Role::SetBaseLeadership(int value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result) {
        Clamp(value, 0, kMaxSkillValue);
        if (m_baseLeadership != value) {
            const int oldValue = m_baseLeadership;
            m_baseLeadership = static_cast<int16_t>(value);
            ViewAsRole(kAllianceView)->ApplyBaseLeadership(ctx);
            ViewAsRole(kEmpireView)->ApplyBaseLeadership(ctx);
            OnBaseLeadershipChanged(oldValue, m_baseLeadership, ctx);
        }
    }
    return result;
}

int Role::SetBaseLoyalty(int value, NotifyContext* ctx) {
    int result = 0;
    if (IsMasterView()) {
        result = FUN_0053fc90(value, 0, kMaxLoyalty, "Invalid BaseLoyalty value!");
        Clamp(value, 0, kMaxSkillValue);
    }
    if (result && m_baseLoyalty != value) {
        const int oldValue = m_baseLoyalty;
        m_baseLoyalty = static_cast<int16_t>(value);
        ViewAsRole(kAllianceView)->ApplyBaseLoyalty(ctx);
        ViewAsRole(kEmpireView)->ApplyBaseLoyalty(ctx);
        OnBaseLoyaltyChanged(oldValue, m_baseLoyalty, ctx);
    }
    return result;
}

int Role::SetMissionKey(ObjectKey* value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && m_missionKey.m_value != value->m_value) {
        ObjectKey oldKey;
        oldKey.m_value = m_missionKey.m_value;
        m_missionKey.m_value = value->m_value;
        ViewAsRole(kAllianceView)->ApplyMissionKey(ctx);
        ViewAsRole(kEmpireView)->ApplyMissionKey(ctx);
        OnMissionChanged(&oldKey, &m_missionKey, ctx);
    }
    return result;
}

int Role::SetMissionSeedKey(ObjectKey* value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && m_missionSeedKey.m_value != value->m_value) {
        ObjectKey oldKey;
        oldKey.m_value = m_missionSeedKey.m_value;
        m_missionSeedKey.m_value = value->m_value;
        ViewAsRole(kAllianceView)->ApplyMissionSeedKey(ctx);
        ViewAsRole(kEmpireView)->ApplyMissionSeedKey(ctx);
        OnMissionSeedChanged(&oldKey, MissionSeedKeyAsObjectKey(), ctx);
    }
    return result;
}

int Role::SetParentAtMissionCompletionKey(ObjectKey* value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && m_parentAtMissionCompletionKey.m_value != value->m_value) {
        ObjectKey oldKey;
        oldKey.m_value = m_parentAtMissionCompletionKey.m_value;
        m_parentAtMissionCompletionKey.m_value = value->m_value;
        ViewAsRole(kAllianceView)->ApplyParentAtMissionCompletionKey(ctx);
        ViewAsRole(kEmpireView)->ApplyParentAtMissionCompletionKey(ctx);
        OnParentAtMissionCompletionChanged(&oldKey, &m_parentAtMissionCompletionKey, ctx);
    }
    return result;
}

int Role::SetLocationAtMissionCompletionKey(ObjectKey* value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && m_locationAtMissionCompletionKey.m_value != value->m_value) {
        ObjectKey oldKey;
        oldKey.m_value = m_locationAtMissionCompletionKey.m_value;
        m_locationAtMissionCompletionKey.m_value = value->m_value;
        ViewAsRole(kAllianceView)->ApplyLocationAtMissionCompletionKey(ctx);
        ViewAsRole(kEmpireView)->ApplyLocationAtMissionCompletionKey(ctx);
        OnLocationAtMissionCompletionChanged(&oldKey, &m_locationAtMissionCompletionKey, ctx);
    }
    return result;
}

int Role::SetDecoy(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(role_flag::kDecoy, on, &m_flags78)) {
        ViewAsRole(kAllianceView)->ApplyDecoy(ctx);
        ViewAsRole(kEmpireView)->ApplyDecoy(ctx);
        OnDecoyChanged(on, ctx);
    }
    return result;
}

int Role::SetMovingBetweenMissions(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(role_flag::kMovingBetweenMissions, on, &m_flags78)) {
        ViewAsRole(kAllianceView)->ApplyMovingBetweenMissions(ctx);
        ViewAsRole(kEmpireView)->ApplyMovingBetweenMissions(ctx);
        OnMovingBetweenMissionsChanged(on, ctx);
    }
    return result;
}

int Role::SetMissionRemoveRequest(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(role_flag::kMissionRemoveRequest, on, &m_flags78)) {
        ViewAsRole(kAllianceView)->ApplyMissionRemoveRequest(ctx);
        ViewAsRole(kEmpireView)->ApplyMissionRemoveRequest(ctx);
        OnMissionRemoveRequestChanged(on, ctx);
    }
    return result;
}

int Role::SetMissionResignRequest(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(role_flag::kMissionResignRequest, on, &m_flags78)) {
        ViewAsRole(kAllianceView)->ApplyMissionResignRequest(ctx);
        ViewAsRole(kEmpireView)->ApplyMissionResignRequest(ctx);
        OnMissionResignRequestChanged(on, ctx);
    }
    return result;
}

int Role::SetCanResignFromMission(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(role_flag::kCanResignFromMission, on, &m_flags78)) {
        ViewAsRole(kAllianceView)->ApplyCanResignFromMission(ctx);
        ViewAsRole(kEmpireView)->ApplyCanResignFromMission(ctx);
        OnCanResignFromMissionChanged(on, ctx);
    }
    return result;
}

int Role::SetIsDecoying(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(role_flag::kIsDecoying, on, &m_flags78)) {
        ViewAsRole(kAllianceView)->ApplyIsDecoying(ctx);
        ViewAsRole(kEmpireView)->ApplyIsDecoying(ctx);
        OnIsDecoyingChanged(on, ctx);
    }
    return result;
}

int Role::SetAdrift(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(role_flag::kAdrift, on, &m_flags78)) {
        ViewAsRole(kAllianceView)->ApplyAdrift(ctx);
        ViewAsRole(kEmpireView)->ApplyAdrift(ctx);
        OnAdriftChanged(on, ctx);
    }
    return result;
}

int Role::SetOnMission(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(role_flag::kOnMission, on, &m_flags78)) {
        ViewAsRole(kAllianceView)->ApplyOnMission(ctx);
        ViewAsRole(kEmpireView)->ApplyOnMission(ctx);
        OnOnMissionChanged(on, ctx);
    }
    return result;
}

int Role::SetOnHiddenMission(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(role_flag::kOnHiddenMission, on, &m_flags78)) {
        ViewAsRole(kAllianceView)->ApplyOnHiddenMission(ctx);
        ViewAsRole(kEmpireView)->ApplyOnHiddenMission(ctx);
        OnOnHiddenMissionChanged(on, ctx);
    }
    return result;
}

int Role::SetOnMandatoryMission(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(role_flag::kOnMandatoryMission, on, &m_flags78)) {
        ViewAsRole(kAllianceView)->ApplyOnMandatoryMission(ctx);
        ViewAsRole(kEmpireView)->ApplyOnMandatoryMission(ctx);
        OnOnMandatoryMissionChanged(on, ctx);
    }
    return result;
}
