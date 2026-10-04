#include "character/character.h"
#include "data/dat_record.h"
#include "data/gnprtb.h"
#include "data/random.h"
#include "data/sd_records.h"

// Event hubs of the strategic notification layer (all cdecl).
int FUN_0053f950(int eventId, GameObj* object, int a, int b, NotifyContext* ctx);
int FUN_0053f9c0(int eventId, GameObj* object, int value, NotifyContext* ctx);
int FUN_0053fa60(int eventId, int a, GameObj* object, void* data, NotifyContext* ctx);
int FUN_0053fab0(int eventId, int a, GameObj* object, void* data, NotifyContext* ctx);

// OnXChanged: events on the master view. Most of them recompute the derived values that depend on the property.

namespace {

constexpr uint32_t kCapturedFlag = 0x1;
constexpr uint32_t kFastHealFlag = 0x4;
constexpr uint32_t kCanResignFlag = 0x10;
constexpr int kCommandNameOffsets[] = {0, 0x6800, 0x6c00, 0x7000};
constexpr int kAllSides = 3;

CharacterData* dataOf(Character* character) { return static_cast<CharacterData*>(character->m_data); }

}  // namespace

// ---- overrides of the Role/GameObj events ---------------------------------------------------------------------

int Character::OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx) {
    int result = Role::OnParentChanged(oldParent, newParent, ctx);
    if (result) result = UpdateCanHeal(ctx);
    return result;
}

int Character::OnCreatedChanged(int newValue, NotifyContext* ctx) {
    int result = Role::OnCreatedChanged(newValue, ctx);
    MajorCharacterSD* record = nullptr;
    if (result) {
        record = GetCharacterRecord();
        result = record != nullptr;
    }
    if (result) {
        const int jediProbability = record->m_moreFields[0];
        const int knownJedi = record->m_moreFields[1];
        result = SetForceAware(knownJedi, ctx);
        if (result) result = SetForcePotential(PercentChance(jediProbability), ctx);
        if (result) result = UpdateForceExperience(ctx);
        if (result) result = UpdateForce(ctx);
        if (result) result = UpdateEnhancedDiplomacy(ctx);
        if (result) result = UpdateEnhancedEspionage(ctx);
        if (result) result = UpdateEnhancedShipyardRD(ctx);
        if (result) result = UpdateEnhancedTrainingFacilRD(ctx);
        if (result) result = UpdateEnhancedConstructionYardRD(ctx);
        if (result) result = UpdateEnhancedCombat(ctx);
        if (result) result = UpdateEnhancedLeadership(ctx);
        if (result) result = UpdateEnhancedLoyalty(ctx);
    }
    return result;
}

int Character::OnEnrouteChanged(int newValue, NotifyContext* ctx) {
    int result = Role::OnEnrouteChanged(newValue, ctx);
    if (result) result = UpdateHealing(ctx);
    return result;
}

int Character::OnUsableChanged(int newValue, NotifyContext* ctx) {
    int result = Role::OnUsableChanged(newValue, ctx);
    if (result) {
        result = UpdateDiscoveringForceUser(ctx);
        if (result) result = RaiseEscapeRequest(ctx);
    }
    return result;
}

int Character::OnDestroyedChanged(int newValue, NotifyContext* ctx) {
    int result = Role::OnDestroyedChanged(newValue, ctx);
    if (newValue != 0) {
        result = UpdateCommandKind(ctx) && result;
        result = UpdateCanHeal(ctx) && result;
        result = SetCanEscape(0, ctx) && result;
        const ObjectKey unset;
        result = SetHolderKey(&unset) && result;
    }
    return result;
}

int Character::OnAutoroutingChanged(int newValue, NotifyContext* ctx) {
    return Role::OnAutoroutingChanged(newValue, ctx);
}

int Character::OnAdriftChanged(int newValue, NotifyContext* ctx) {
    int result = Role::OnAdriftChanged(newValue, ctx);
    if (m_flagsAc & kCapturedFlag) result = Virtual134(reinterpret_cast<int>(ctx));
    return result;
}

int Character::OnMovingBetweenMissionsChanged(int newValue, NotifyContext* ctx) {
    return Role::OnMovingBetweenMissionsChanged(newValue, ctx);
}

int Character::OnOnMissionChanged(int newValue, NotifyContext* ctx) {
    int result = Role::OnOnMissionChanged(newValue, ctx);
    if (result) {
        result = UpdateDiscoveringForceUser(ctx);
        if (result) {
            result = UpdateCommandKind(ctx);
            if (result) result = RaiseEscapeRequest(ctx);
        }
    }
    return result;
}

int Character::OnMissionChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) {
    int result = Role::OnMissionChanged(oldKey, newKey, ctx);
    if (result && !newKey->IsSet()) result = SetMissionHyperdriveModifier(g_gnprtbDefault, ctx);
    return result;
}

int Character::OnBaseDiplomacyChanged(int oldValue, int newValue, NotifyContext* ctx) {
    int result = Role::OnBaseDiplomacyChanged(oldValue, newValue, ctx);
    if (result) result = UpdateEnhancedDiplomacy(ctx);
    return result;
}

int Character::OnBaseEspionageChanged(int oldValue, int newValue, NotifyContext* ctx) {
    int result = Role::OnBaseEspionageChanged(oldValue, newValue, ctx);
    if (result) result = UpdateEnhancedEspionage(ctx);
    return result;
}

int Character::OnBaseShipyardRDChanged(int oldValue, int newValue, NotifyContext* ctx) {
    int result = Role::OnBaseShipyardRDChanged(oldValue, newValue, ctx);
    if (result) result = UpdateEnhancedShipyardRD(ctx);
    return result;
}

int Character::OnBaseTrainingFacilRDChanged(int oldValue, int newValue, NotifyContext* ctx) {
    int result = Role::OnBaseTrainingFacilRDChanged(oldValue, newValue, ctx);
    if (result) result = UpdateEnhancedTrainingFacilRD(ctx);
    return result;
}

int Character::OnBaseConstructionYardRDChanged(int oldValue, int newValue, NotifyContext* ctx) {
    int result = Role::OnBaseConstructionYardRDChanged(oldValue, newValue, ctx);
    if (result) result = UpdateEnhancedConstructionYardRD(ctx);
    return result;
}

int Character::OnBaseCombatChanged(int oldValue, int newValue, NotifyContext* ctx) {
    int result = Role::OnBaseCombatChanged(oldValue, newValue, ctx);
    if (result) result = UpdateEnhancedCombat(ctx);
    return result;
}

int Character::OnBaseLeadershipChanged(int oldValue, int newValue, NotifyContext* ctx) {
    int result = Role::OnBaseLeadershipChanged(oldValue, newValue, ctx);
    if (result) result = UpdateEnhancedLeadership(ctx);
    return result;
}

int Character::OnBaseLoyaltyChanged(int oldValue, int newValue, NotifyContext* ctx) {
    int result = Role::OnBaseLoyaltyChanged(oldValue, newValue, ctx);
    if (result) result = UpdateEnhancedLoyalty(ctx);
    return result;
}

// ---- events of the Character properties ---------------------------------------------------------------------

int Character::OnForceChanged(int oldValue, int newValue, NotifyContext* ctx) {
    int result = newValue != 0;
    if (result) result = UpdateEnhancedDiplomacy(ctx);
    if (result) {
        result = UpdateEnhancedEspionage(ctx);
        if (result) {
            result = UpdateEnhancedCombat(ctx);
            if (result) {
                result = UpdateFastHeal(ctx);
                if (result) result = UpdateDiscoveringForceUser(ctx);
            }
        }
    }
    return result;
}

int Character::OnForceExperienceChanged(int oldValue, int newValue, NotifyContext* ctx) { return UpdateForce(ctx); }

int Character::OnForceTrainingChanged(int oldValue, int newValue, NotifyContext* ctx) { return UpdateForce(ctx); }

int Character::OnLeadershipAdjustmentChanged(int oldValue, int newValue, NotifyContext* ctx) {
    return UpdateEnhancedLeadership(ctx);
}

int Character::OnInjuryChanged(int oldValue, int newValue, NotifyContext* ctx) {
    int result = UpdateEnhancedCombat(ctx);
    if (result) {
        result = UpdateHealing(ctx);
        if (result) {
            result = UpdateCommandKind(ctx);
            if (result) {
                if (newValue != 0 && (m_flags78 & kCanResignFlag)) {
                    const ObjectKey missionKey(m_missionKey);
                    if (missionKey.IsSet()) result = SetMissionResignRequest(1, ctx);
                }
                if (result) result = SetDamaged(newValue != 0, ctx);
            }
        }
    }
    return result;
}

int Character::OnCommandKindChanged(int oldValue, int newValue, NotifyContext* ctx) {
    const DatRecord* record = m_record;
    bool named = true;
    ResourceStringId nameId = {static_cast<int16_t>(record->m_small), static_cast<int16_t>(record->m_small2)};
    const unsigned kind = static_cast<unsigned>(m_commandKind);
    if (kind < sizeof(kCommandNameOffsets) / sizeof(kCommandNameOffsets[0])) {
        nameId.m_id = static_cast<int16_t>(nameId.m_id + kCommandNameOffsets[kind]);
    } else {
        named = false;
    }
    int result = 0;
    if (named) {
        String name(&nameId);
        result = SetName(&name, ctx);
    }
    if (result) {
        ObjectKey unset;
        result = SetCommandingKey(&unset, ctx);
    }
    return result;
}

int Character::OnStateChanged(int oldValue, int newValue, NotifyContext* ctx) {
    const int captured = UpdateCaptured(ctx);
    int ok;
    {
        const ObjectKey unset;
        ok = SetHolderKey(&unset) && captured;
    }
    ok = SetAutorouting(1, ctx) && ok;
    if (m_missionKey.IsSet()) ok = SetMissionRemoveRequest(1, ctx) && ok;
    const int hub = FUN_0053f950(0x30a, this, oldValue, newValue, ctx);
    return hub && ok ? 1 : 0;
}

int Character::OnEncounterChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) {
    NotifyContext allViews(*ctx);
    allViews.m_flag04 = 1;
    allViews.m_flag08 = kAllSides;
    ViewAsCharacter(1)->ApplyEncounterKey(&allViews);
    ViewAsCharacter(2)->ApplyEncounterKey(&allViews);
    return 1;
}

int Character::OnCapturedChanged(int newValue, NotifyContext* ctx) {
    int result = SetCanEscape(newValue, ctx);
    if (result) {
        result = SetEscapeRequest(0, ctx);
        if (result) {
            result = UpdateControlKind(ctx);
            if (result) {
                result = UpdateCanHeal(ctx);
                if (result) {
                    result = UpdateDiscoveringForceUser(ctx);
                    if (result) result = UpdateCommandKind(ctx);
                }
            }
        }
    }
    return result;
}

int Character::OnCanHealChanged(int newValue, NotifyContext* ctx) { return UpdateHealing(ctx); }

int Character::OnFastHealChanged(int newValue, NotifyContext* ctx) {
    const int level = (m_flagsAc & kFastHealFlag) ? g_gnprtbA01 : g_gnprtbA02;
    return FUN_0053fab0(0x388, level, this, &dataOf(this)->m_healingStat, ctx);
}

int Character::OnForceAwareChanged(int newValue, NotifyContext* ctx) { return UpdateForceExperience(ctx); }

int Character::OnForcePotentialChanged(int newValue, NotifyContext* ctx) { return UpdateForceExperience(ctx); }

int Character::OnHealingChanged(int newValue, NotifyContext* ctx) {
    return FUN_0053fa60(0x388, newValue, this, &dataOf(this)->m_healingStat, ctx);
}

int Character::OnDiscoveringForceUserChanged(int newValue, NotifyContext* ctx) {
    return FUN_0053fa60(0x389, newValue, this, &dataOf(this)->m_forceDiscoveryStat, ctx);
}

int Character::OnCanEscapeChanged(int newValue, NotifyContext* ctx) {
    return FUN_0053fa60(0x38a, newValue, this, &dataOf(this)->m_escapeStat, ctx);
}

int Character::OnEscapeRequestChanged(int newValue, NotifyContext* ctx) { return RaiseEscapeRequest(ctx); }

int Character::OnEscapeAttemptChanged(int newValue, NotifyContext* ctx) {
    return FUN_0053f9c0(0x317, this, newValue, ctx);
}
