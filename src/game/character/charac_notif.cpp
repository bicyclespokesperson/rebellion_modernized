#include "character/character.h"
#include "events/event_post.h"

// OnXNotified: events raised on a side view (the "Character*Notif" strings). The debug log calls are dead in the shipped
// game; the hub calls forward the change to the UI (ids 0x1e0-0x1e5 for the Force/command state, 0x360-0x362 for keys).

int Character::OnEnhancedDiplomacyNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(CharacterEnhancedDiplomacyNotif)", "EnhancedDiplomacy", newValue, oldValue, 0, ctx);
    return result;
}

int Character::OnEnhancedEspionageNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(CharacterEnhancedEspionageNotif)", "EnhancedEspionage", newValue, oldValue, 0, ctx);
    return result;
}

int Character::OnEnhancedShipyardRDNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(CharacterEnhancedShipyardRDNotif)", "EnhancedShipyardRD", newValue, oldValue, 0, ctx);
    return result;
}

int Character::OnEnhancedTrainingFacilRDNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(CharacterEnhancedTrainingFacilRDNotif)", "EnhancedTrainingFacilRD", newValue, oldValue, 0, ctx);
    return result;
}

int Character::OnEnhancedConstructionYardRDNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(CharacterEnhancedConstructionYardRDNotif)", "EnhancedConstructionYardRD", newValue, oldValue, 0, ctx);
    return result;
}

int Character::OnEnhancedCombatNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(CharacterEnhancedCombatNotif)", "EnhancedCombat", newValue, oldValue, 0, ctx);
    return result;
}

int Character::OnEnhancedLeadershipNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(CharacterEnhancedLeadershipNotif)", "EnhancedLeadership", newValue, oldValue, 0, ctx);
    return result;
}

int Character::OnEnhancedLoyaltyNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(CharacterEnhancedLoyaltyNotif)", "EnhancedLoyalty", newValue, oldValue, 0, ctx);
    return result;
}

int Character::OnForceNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("CharacterForceNotif", "Force", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x1e1, this, oldValue, newValue, ctx);
    }
    return result;
}

int Character::OnForceExperienceNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("CharacterForceExperienceNotif", "ForceExperience", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x1e4, this, oldValue, newValue, ctx);
    }
    return result;
}

int Character::OnForceTrainingNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("CharacterForceTrainingNotif", "ForceTraining", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x1e5, this, oldValue, newValue, ctx);
    }
    return result;
}

int Character::OnLeadershipAdjustmentNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(CharacterLeadershipAdjustmentNotif)", "LeadershipAdjustment", newValue, oldValue, 0, ctx);
    return result;
}

int Character::OnInjuryNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("CharacterInjuryNotif", "Injury", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x1e3, this, oldValue, newValue, ctx);
    }
    return result;
}

int Character::OnCommandKindNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("(CharacterCommandKindNotif)", "CommandKind", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x1e2, this, oldValue, newValue, ctx);
    }
    return result;
}

int Character::OnStateNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        result = PostPayloadInts(0x1e0, this, oldValue, newValue, ctx);
    }
    return result;
}

int Character::OnMissionHyperdriveModifierNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(MissionHyperdriveModifierNotif)", "MissionHyperdriveModifier", newValue, oldValue, 0, ctx);
    return result;
}

int Character::OnEncounterNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogKey("(CharacterEncounterKeyNotif)", "Encounter", newKey, oldKey, 0, ctx);
        result = PostPayloadKeysA(0x360, this, oldKey, newKey, ctx);
    }
    return result;
}

int Character::OnTraitorDiscoveredNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogKey("CharacterTraitorDiscoveredKeyNotif", "TraitorDiscovered", newKey, oldKey, 0, ctx);
        result = PostPayloadKeysA(0x361, this, oldKey, newKey, ctx);
    }
    return result;
}

int Character::OnForceUserDiscoveredNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogKey("CharacterForceUserDiscoveredKeyNotif", "ForceUserDiscovered", newKey, oldKey, 0, ctx);
        result = PostPayloadKeysA(0x362, this, oldKey, newKey, ctx);
    }
    return result;
}

int Character::OnCommandingNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogKey("CharacterCommandingKeyNotif", "Commanding", newKey, oldKey, 0, ctx);
        result = PostPayloadKeysA(0x362, this, oldKey, newKey, ctx);
    }
    return result;
}

int Character::OnCapturedNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(CharacterCapturedNotif)", "Captured", value, 0, ctx);
    return result;
}

int Character::OnCanHealNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(CharacterCanHealNotif)", "CanHeal", value, 0, ctx);
    return result;
}

int Character::OnFastHealNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(CharacterFastHealNotif)", "FastHeal", value, 0, ctx);
    return result;
}

int Character::OnForceAwareNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(CharacterForceAwareNotif)", "ForceAware", value, 0, ctx);
    return result;
}

int Character::OnForcePotentialNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(CharacterForcePotentialNotif)", "ForcePotential", value, 0, ctx);
    return result;
}

int Character::OnHealingNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(CharacterHealingNotif)", "Healing", value, 0, ctx);
    return result;
}

int Character::OnDiscoveringForceUserNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(CharacterDiscoveringForceUserNotif)", "DiscoveringForceUser", value, 0, ctx);
    return result;
}

int Character::OnCanEscapeNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(CharacterCanEscapeNotif)", "CanEscape", value, 0, ctx);
    return result;
}

int Character::OnEscapeRequestNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(CharacterEscapeRequestNotif)", "EscapeRequest", value, 0, ctx);
    return result;
}

int Character::OnEscapeAttemptNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(CharacterEscapeAttemptNotif)", "EscapeAttempt", value, 0, ctx);
    return result;
}
