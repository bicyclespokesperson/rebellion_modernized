#include "events/event_post.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"
#include "mission/mission_rules.h"
#include "rules/rules_mgr.h"
#include "story/mission_mgr.h"
#include "story/story_characters.h"
#include "story/story_common.h"

// The strategic rules object (game root member +0xbc, family 0xf9): 0x556c70 is the call the Han and Palace steps make.
ObjectKey* __thiscall GetOwnerKeyValue(GameObjNotifier* self, ObjectKey* out);


// The story of the game: Luke is trained on Dagobah, meets Han on the Palace, Darth Vader takes the heroes captive and the final
// battle is fought between Luke, the Emperor and Darth. MissionMgr derives each step from where the six named characters are
// (Luke 0x32000242, Leia 0x31000241, Han 0x33000243, Chewbacca 0x38000343, Darth 0x35000281, the Emperor 0x34000280) and pulses
// the matching story flag (set, then cleared): the OnXChanged handler of the flag creates the story's missions.

namespace {

constexpr uint32_t kKeyLuke = 0x32000242;
constexpr uint32_t kKeyLeia = 0x31000241;
constexpr uint32_t kKeyHan = 0x33000243;
constexpr uint32_t kKeyChewbacca = 0x38000343;
constexpr uint32_t kKeyDarth = 0x35000281;
constexpr uint32_t kKeyEmperor = 0x34000280;
constexpr uint32_t kKeyPalaceSystem = 0x98000481;
constexpr uint32_t kFamilyDagobahMission = 0x71;
constexpr uint32_t kFamilyPickupMission = 0x73;
constexpr uint32_t kSideEmpire = 2;
constexpr int kPhaseTrainingStarted = 8;
constexpr int kSideAlliance = 1;
constexpr int kSyncModeBattle = 6;
constexpr uint32_t kControlSideMask = 0xc0;
constexpr uint32_t kFamilySystemBegin = 0x90;
constexpr uint32_t kFamilySystemEnd = 0x98;
constexpr uint32_t kRoleOnMission = role_flag::kOnMission;
constexpr uint32_t kRoleBusyMask = role_flag::kOnMission | role_flag::kAdrift;

int contextAsArgument(NotifyContext* ctx) { return static_cast<int>(reinterpret_cast<intptr_t>(ctx)); }

int keyAsArgument(ObjectKey* key) { return static_cast<int>(reinterpret_cast<intptr_t>(key)); }

// One named story character as the finders report it: the master, the object that holds it and that object's key.
struct StoryActor {
    GameObjNotifier* m_character = nullptr;
    GameObjNotifier* m_location = nullptr;
    ObjectKey m_locationKey;

    bool placed() { return m_character && m_location && m_locationKey.OrNull(); }
    Character* character() { return static_cast<Character*>(m_character); }
};

bool isCapturedAndSettled(Character* character) {
    return (character->m_flagsAc & 1) && (character->m_stateFlags & 1) && !(character->m_flags78 & kRoleBusyMask) &&
           !(character->m_stateFlags & 0x1000);
}

// Runs a story step's flag pulse: set, then cleared. Returns the accumulated ok.
template <typename SetFlag>
int pulseFlag(SetFlag set, int ok, NotifyContext* ctx) {
    ok = set(1, ctx) && ok ? 1 : 0;
    return set(0, ctx) && ok ? 1 : 0;
}

}  // namespace

// ---- finders and predicates ----------------------------------------------------------------------------------------

namespace {
int locate(GameObjNotifier* found, GameObjNotifier** character, GameObjNotifier** location, ObjectKey* locationKey) {
    *character = found;
    int result = found != nullptr;
    if (result) {
        result = static_cast<GameObj*>(found)->GetParent(location);
        if (result) {
            ObjectKey scratch;
            *locationKey = *(*location)->GetKey(&scratch);
        }
    }
    return result;
}
}  // namespace

int MissionMgr::FindLuke(GameObjNotifier** character, GameObjNotifier** location, ObjectKey* locationKey) {
    *character = nullptr;
    *location = nullptr;
    locationKey->Reset();
    return locate(GetMaster_32000242(), character, location, locationKey);
}

int MissionMgr::FindLeia(GameObjNotifier** character, GameObjNotifier** location, ObjectKey* locationKey) {
    *character = nullptr;
    *location = nullptr;
    locationKey->Reset();
    return locate(GetMaster_31000241(), character, location, locationKey);
}

int MissionMgr::FindHan(GameObjNotifier** character, GameObjNotifier** location, ObjectKey* locationKey) {
    *character = nullptr;
    *location = nullptr;
    locationKey->Reset();
    return locate(GetMaster_33000243(), character, location, locationKey);
}

int MissionMgr::FindCharacter38(GameObjNotifier** character, GameObjNotifier** location, ObjectKey* locationKey) {
    *character = nullptr;
    *location = nullptr;
    locationKey->Reset();
    ObjectKey key;
    key.m_value = kKeyChewbacca;
    return locate(GetMasterInFamilies_38_3c(&key), character, location, locationKey);
}

int MissionMgr::FindDarth(GameObjNotifier** character, GameObjNotifier** location, ObjectKey* locationKey) {
    *character = nullptr;
    *location = nullptr;
    locationKey->Reset();
    return locate(GetMaster_35000281(), character, location, locationKey);
}

int MissionMgr::FindEmperor(GameObjNotifier** character, GameObjNotifier** location, ObjectKey* locationKey) {
    *character = nullptr;
    *location = nullptr;
    locationKey->Reset();
    return locate(GetMaster_34000280(), character, location, locationKey);
}

int __stdcall MissionMgr::IsStoryCharacterKey(const ObjectKey* key) {
    const uint32_t value = key->m_value;
    return value == kKeyLuke || value == kKeyLeia || value == kKeyHan || value == kKeyChewbacca || value == kKeyDarth ||
           value == kKeyEmperor;
}

int __stdcall MissionMgr::IsStoryCharacterFree(Character* character) {
    return !(character->m_flagsAc & 1) && (character->m_stateFlags & 1) && !(character->m_stateFlags & 0x1000) &&
                   !(character->m_flags78 & role_flag::kAdrift)
               ? 1
               : 0;
}

int __stdcall MissionMgr::IsStoryCharacterIdle(Character* character) {
    return IsStoryCharacterFree(character) && !(character->m_flags78 & kRoleOnMission) ? 1 : 0;
}

// ---- the steps of the story -----------------------------------------------------------------------------------------

int MissionMgr::UpdateHanCapturedAtPalace(NotifyContext* ctx) {
    StoryActor han;
    int ok = FindHan(&han.m_character, &han.m_location, &han.m_locationKey) ? 1 : 0;
    if (han.placed()) {
        int atPalace = 0;
        if (isCapturedAndSettled(han.character())) atPalace = han.m_locationKey.m_value == kKeyPalaceSystem;
        ok = SetHanCapturedAtPalace(atPalace, ctx) && ok ? 1 : 0;
    }
    return ok;
}

int MissionMgr::UpdateLukeDagobahStep(NotifyContext* ctx) {
    int ok = 1;
    if ((m_storyFlags & story_flag::kLukeDagobahRequired) && !(m_storyFlags & story_flag::kHanCapturedAtPalace)) {
        StoryActor luke;
        ok = FindLuke(&luke.m_character, &luke.m_location, &luke.m_locationKey) ? 1 : 0;
        if (luke.placed() && IsStoryCharacterIdle(luke.character()))
            ok = pulseFlag([this](int on, NotifyContext* c) { return SetLukeDagobah(on, c); }, ok, ctx);
    }
    return ok;
}

int MissionMgr::UpdateLukePalaceStep(NotifyContext* ctx) {
    int ok = 1;
    if (!(m_storyFlags & story_flag::kHanCapturedAtPalace)) return ok;
    StoryActor luke;
    ok = FindLuke(&luke.m_character, &luke.m_location, &luke.m_locationKey) ? 1 : 0;
    if (!luke.placed() || !IsStoryCharacterFree(luke.character())) return ok;
    if (!(luke.character()->m_flags78 & kRoleOnMission))
        return pulseFlag([this](int on, NotifyContext* c) { return SetLukePalace(on, c); }, ok, ctx);

    ObjectKey missionKey;
    luke.character()->GetMissionKey(&missionKey);
    if (missionKey.Family() != kFamilyDagobahMission) return ok;
    ObjectKey lookupKey;
    luke.character()->GetMissionKey(&lookupKey);
    Mission* mission = static_cast<Mission*>(GetMasterInFamilies_40_80(&lookupKey));
    if (!mission || mission->m_phase != kPhaseTrainingStarted) return ok;
    return mission->CompleteIfOutcomePhase(ctx) && ok ? 1 : 0;
}

int MissionMgr::UpdateLeiaPalaceStep(NotifyContext* ctx) {
    int ok = 1;
    if (m_storyFlags & story_flag::kHanCapturedAtPalace) {
        StoryActor leia;
        ok = FindLeia(&leia.m_character, &leia.m_location, &leia.m_locationKey) ? 1 : 0;
        if (leia.placed() && IsStoryCharacterIdle(leia.character()))
            ok = pulseFlag([this](int on, NotifyContext* c) { return SetLeiaPalace(on, c); }, ok, ctx);
    }
    return ok;
}

int MissionMgr::UpdateChewbaccaPalaceStep(NotifyContext* ctx) {
    int ok = 1;
    if (m_storyFlags & story_flag::kHanCapturedAtPalace) {
        StoryActor chewbacca;
        ok = FindCharacter38(&chewbacca.m_character, &chewbacca.m_location, &chewbacca.m_locationKey);
        if (chewbacca.placed() && IsStoryCharacterIdle(chewbacca.character()))
            ok = pulseFlag([this](int on, NotifyContext* c) { return SetChewbaccaPalace(on, c); }, ok, ctx);
    }
    return ok;
}

int MissionMgr::UpdateDarthPickupStep(NotifyContext* ctx) {
    StoryActor luke;
    StoryActor leia;
    StoryActor han;
    StoryActor chewbacca;
    StoryActor darth;
    int ok = FindLuke(&luke.m_character, &luke.m_location, &luke.m_locationKey) ? 1 : 0;
    ok = FindLeia(&leia.m_character, &leia.m_location, &leia.m_locationKey) && ok ? 1 : 0;
    ok = FindHan(&han.m_character, &han.m_location, &han.m_locationKey) && ok ? 1 : 0;
    ok = FindCharacter38(&chewbacca.m_character, &chewbacca.m_location, &chewbacca.m_locationKey) && ok ? 1 : 0;
    int result = FindDarth(&darth.m_character, &darth.m_location, &darth.m_locationKey) && ok ? 1 : 0;
    if (!luke.placed() || !leia.placed() || !han.placed() || !chewbacca.placed() || !darth.placed()) return result;

    bool heroesTaken = false;
    if ((luke.character()->m_flagsAc & 1) && (leia.character()->m_flagsAc & 1) && (han.character()->m_flagsAc & 1) &&
        ((chewbacca.character()->m_flagsAc & 1) || (chewbacca.character()->m_stateFlags & 8)))
        heroesTaken = true;

    int anyCaptive = 0;
    GameObjNotifier* container = GetGameRootMember_dc();
    result = container && result ? 1 : 0;
    if (container) {
        CharacterIterator members(container, 1, kSideEmpire);
        members.Begin();
        while (members.Current() && !anyCaptive) {
            anyCaptive = static_cast<Character*>(members.Current())->m_flagsAc & 1;
            members.Advance();
        }
    }
    if (!anyCaptive || !heroesTaken || !IsStoryCharacterFree(darth.character())) return result;

    if (!(darth.character()->m_flags78 & kRoleOnMission))
        return pulseFlag([this](int on, NotifyContext* c) { return SetDarthPickup(on, c); }, result, ctx);

    ObjectKey missionKey;
    const bool onPickup = darth.character()->GetMissionKey(&missionKey)->Family() == kFamilyPickupMission;
    if (onPickup || !(darth.character()->m_flags78 & role_flag::kCanResignFromMission)) return result;
    ObjectKey resignKey;
    if (!darth.character()->GetMissionKey(&resignKey)->IsSet()) return result;
    return darth.character()->SetMissionResignRequest(1, ctx) && result ? 1 : 0;
}

int MissionMgr::UpdateDarthToLukeStep(NotifyContext* ctx) {
    int ok = 1;
    if (!(m_storyFlags & story_flag::kFinalBattleReady)) return ok;
    StoryActor luke;
    StoryActor darth;
    StoryActor emperor;
    ok = FindLuke(&luke.m_character, &luke.m_location, &luke.m_locationKey) ? 1 : 0;
    ok = FindDarth(&darth.m_character, &darth.m_location, &darth.m_locationKey) && ok ? 1 : 0;
    ok = FindEmperor(&emperor.m_character, &emperor.m_location, &emperor.m_locationKey) && ok ? 1 : 0;
    if (luke.placed() && darth.placed() && emperor.placed() && darth.m_locationKey.m_value != luke.m_locationKey.m_value)
        ok = pulseFlag([this](int on, NotifyContext* c) { return SetDarthToLukeFinalBattle(on, c); }, ok, ctx);
    return ok;
}

int MissionMgr::UpdateDarthToEmperorStep(NotifyContext* ctx) {
    int ok = 1;
    if (!(m_storyFlags & story_flag::kFinalBattleReady)) return ok;
    StoryActor luke;
    StoryActor darth;
    StoryActor emperor;
    ok = FindLuke(&luke.m_character, &luke.m_location, &luke.m_locationKey) ? 1 : 0;
    ok = FindDarth(&darth.m_character, &darth.m_location, &darth.m_locationKey) && ok ? 1 : 0;
    ok = FindEmperor(&emperor.m_character, &emperor.m_location, &emperor.m_locationKey) && ok ? 1 : 0;
    if (luke.placed() && darth.placed() && emperor.placed() && darth.m_locationKey.m_value == luke.m_locationKey.m_value &&
        darth.m_locationKey.m_value != emperor.m_locationKey.m_value)
        ok = pulseFlag([this](int on, NotifyContext* c) { return SetDarthToEmperorFinalBattle(on, c); }, ok, ctx);
    return ok;
}

int MissionMgr::UpdateFinalBattleReadyStep(NotifyContext* ctx) {
    int allFound = 1;
    int ready = 0;
    if (!(m_storyFlags & story_flag::kFinalBattle)) {
        StoryActor luke;
        StoryActor darth;
        StoryActor emperor;
        int ok = FindLuke(&luke.m_character, &luke.m_location, &luke.m_locationKey) ? 1 : 0;
        ok = FindDarth(&darth.m_character, &darth.m_location, &darth.m_locationKey) && ok ? 1 : 0;
        allFound = FindEmperor(&emperor.m_character, &emperor.m_location, &emperor.m_locationKey) && ok ? 1 : 0;
        if (luke.placed() && darth.placed() && emperor.placed()) {
            const int darthIdle = IsStoryCharacterIdle(darth.character());
            const int emperorIdle = IsStoryCharacterIdle(emperor.character());
            Character* hero = luke.character();
            if (isCapturedAndSettled(hero) && luke.m_locationKey.m_value != kKeyPalaceSystem &&
                (static_cast<Luke*>(hero)->m_storyFlags & Luke::kFinalBattleReady) && darthIdle && emperorIdle)
                ready = 1;
        }
    }
    return SetFinalBattleReady(ready, ctx) && allFound ? 1 : 0;
}

int MissionMgr::UpdateFinalBattleStep(NotifyContext* ctx) {
    int ok = 1;
    if (!(m_storyFlags & story_flag::kFinalBattleReady)) return ok;
    StoryActor luke;
    StoryActor darth;
    StoryActor emperor;
    ok = FindLuke(&luke.m_character, &luke.m_location, &luke.m_locationKey) ? 1 : 0;
    ok = FindDarth(&darth.m_character, &darth.m_location, &darth.m_locationKey) && ok ? 1 : 0;
    ok = FindEmperor(&emperor.m_character, &emperor.m_location, &emperor.m_locationKey) && ok ? 1 : 0;
    if (luke.placed() && darth.placed() && emperor.placed() && darth.m_locationKey.m_value == luke.m_locationKey.m_value &&
        darth.m_locationKey.m_value == emperor.m_locationKey.m_value) {
        const int set = SetFinalBattle(1, ctx);
        ok = UpdateFinalBattleReadyStep(ctx) && (set && ok) ? 1 : 0;
    }
    return ok;
}

// ---- the OnXChanged handlers that create the story's missions -----------------------------------------------------------

namespace {

constexpr uint32_t kTypeDagobahMission = 0x71000043;
constexpr uint32_t kTypePalaceMission = 0x64000044;
constexpr uint32_t kTypePickupMission = 0x73000082;
constexpr uint32_t kSystemDagobah = 0x98000441;

ObjectKey keyFor(uint32_t value) {
    ObjectKey key;
    key.m_value = value;
    return key;
}

}  // namespace

int MissionMgr::OnLukeDagobahRequiredChanged(int on, NotifyContext* ctx) {
    if (!on) return 1;
    const int stepped = UpdateLukeDagobahStep(ctx) ? 1 : 0;
    const int rearmed = ArmTimerAfterSetFlag(0x392, 0, this, &m_statA, ctx);
    return rearmed && stepped ? 1 : 0;
}

int MissionMgr::OnLukeDagobahChanged(int on, NotifyContext* ctx) {
    if (!on) return 1;
    ObjectKey objective = keyFor(kSystemDagobah);
    ObjectKey type = keyFor(kTypeDagobahMission);
    ObjectKey person = keyFor(kKeyLuke);
    return CreateMissionForCharacter(&objective, &type, &person, ctx) ? 1 : 0;
}

int MissionMgr::OnLukePalaceChanged(int on, NotifyContext* ctx) {
    if (!on) return 1;
    ObjectKey objective = keyFor(kKeyHan);
    ObjectKey type = keyFor(kTypePalaceMission);
    ObjectKey person = keyFor(kKeyLuke);
    return CreateMissionForCharacter(&objective, &type, &person, ctx) ? 1 : 0;
}

int MissionMgr::OnHanCapturedAtPalaceChanged(int on, NotifyContext* ctx) {
    if (!on) return 1;
    SetBountyHuntersActive(0, ctx);
    GameObjNotifier* rules = GetGameRootMember_bc();
    int ok = rules != nullptr;
    if (rules) {
        ObjectKey han = keyFor(kKeyHan);
        ok = static_cast<RulesMgr*>(rules)->RulesMgr::SyncUnitView(&han, 1, ctx) && ok ? 1 : 0;
    }
    return ok;
}

int MissionMgr::OnLeiaPalaceChanged(int on, NotifyContext* ctx) {
    if (!on) return 1;
    ObjectKey objective = keyFor(kKeyHan);
    ObjectKey type = keyFor(kTypePalaceMission);
    ObjectKey person = keyFor(kKeyLeia);
    return CreateMissionForCharacter(&objective, &type, &person, ctx) ? 1 : 0;
}

int MissionMgr::OnChewbaccaPalaceChanged(int on, NotifyContext* ctx) {
    if (!on) return 1;
    ObjectKey objective = keyFor(kKeyHan);
    ObjectKey type = keyFor(kTypePalaceMission);
    ObjectKey person = keyFor(kKeyChewbacca);
    return CreateMissionForCharacter(&objective, &type, &person, ctx) ? 1 : 0;
}

int MissionMgr::OnDarthPickupChanged(int on, NotifyContext* ctx) {
    if (!on) return 1;
    ObjectKey objective = keyFor(kKeyPalaceSystem);
    ObjectKey type = keyFor(kTypePickupMission);
    ObjectKey person = keyFor(kKeyDarth);
    return CreateMissionForCharacter(&objective, &type, &person, ctx) ? 1 : 0;
}

int MissionMgr::OnDarthToLukeFinalBattleChanged(int on, NotifyContext* ctx) {
    if (!on) return 1;
    StoryActor luke;
    int ok = FindLuke(&luke.m_character, &luke.m_location, &luke.m_locationKey) ? 1 : 0;
    StoryActor darth;
    ok = FindDarth(&darth.m_character, &darth.m_location, &darth.m_locationKey) && ok ? 1 : 0;
    if (!darth.m_character) return ok;
    if (!luke.m_locationKey.OrNull()) return ok;
    ObjectKeyValueSet team;
    ObjectKey darthKey = keyFor(kKeyDarth);
    const int added = team.AddKeyValue(&darthKey, 0);
    ok = added && ok ? 1 : 0;
    const int created = CreateMissionType41(darth.character()->ControlSide(), &luke.m_locationKey, &team, ctx);
    return created && ok ? 1 : 0;
}

int MissionMgr::OnDarthToEmperorFinalBattleChanged(int on, NotifyContext* ctx) {
    if (!on) return 1;
    StoryActor emperor;
    int ok = FindEmperor(&emperor.m_character, &emperor.m_location, &emperor.m_locationKey) ? 1 : 0;
    StoryActor darth;
    ok = FindDarth(&darth.m_character, &darth.m_location, &darth.m_locationKey) && ok ? 1 : 0;
    StoryActor luke;
    ok = FindLuke(&luke.m_character, &luke.m_location, &luke.m_locationKey) && ok ? 1 : 0;
    if (!(darth.m_character && luke.m_character && emperor.m_locationKey.OrNull() && ok)) return ok;

    Character* darthCharacter = darth.character();
    Character* lukeCharacter = luke.character();
    const bool bothEmpire = (darthCharacter->ControlSide() == kSideEmpire) && (lukeCharacter->ControlSide() == kSideEmpire) &&
                            !(darthCharacter->m_flags78 & kRoleOnMission) && !(lukeCharacter->m_flags78 & kRoleOnMission);
    if (!bothEmpire) return 0;
    ObjectKeyValueSet team;
    ObjectKey darthKey = keyFor(kKeyDarth);
    ObjectKey lukeKey = keyFor(kKeyLuke);
    const int addedDarth = team.AddKeyValue(&darthKey, 0);
    const int addedLuke = team.AddKeyValue(&lukeKey, 0);
    const int created = CreateMissionType41(darthCharacter->ControlSide(), &emperor.m_locationKey, &team, ctx);
    return created && (addedLuke && addedDarth) ? 1 : 0;
}

int MissionMgr::OnFinalBattleChanged(int on, NotifyContext* ctx) {
    if (!on) return 1;
    StoryActor luke;
    StoryActor darth;
    StoryActor emperor;
    int winningSide = 0;
    int ok = FindLuke(&luke.m_character, &luke.m_location, &luke.m_locationKey) ? 1 : 0;
    ok = FindDarth(&darth.m_character, &darth.m_location, &darth.m_locationKey) && ok ? 1 : 0;
    ok = FindEmperor(&emperor.m_character, &emperor.m_location, &emperor.m_locationKey) && ok ? 1 : 0;
    if (luke.m_locationKey.OrNull() && darth.m_locationKey.OrNull() && emperor.m_locationKey.OrNull()) {
        const bool sameLocation = luke.m_locationKey.m_value == emperor.m_locationKey.m_value &&
                                  darth.m_locationKey.m_value == emperor.m_locationKey.m_value;
        ok = sameLocation && ok ? 1 : 0;
    }

    if (ok && luke.m_character && darth.m_character && emperor.m_character) {
        Character* lukeCharacter = luke.character();
        ObjectKey ownerKey;
        const uint32_t ownerFamily = GetOwnerKeyValue(lukeCharacter, &ownerKey)->Family();
        if (ownerFamily >= kFamilySystemBegin && ownerFamily < kFamilySystemEnd) {
            ObjectKey systemKey;
            System* system = static_cast<System*>(GetMasterInFamilies_90_98(GetOwnerKeyValue(lukeCharacter, &systemKey)));
            if (system) {
                ok = system->SyncViewForEvent(kSyncModeBattle, 1, ctx) ? 1 : 0;
                ok = system->SyncViewForEvent(kSyncModeBattle, 2, ctx) && ok ? 1 : 0;
            }
        }

        NotifyContext everySide(*ctx);
        everySide.m_flag04 = 1;
        everySide.m_flag08 = 3;
        const auto applyToAllViews = [&](int& accumulated) {
            GameObj* battleCharacters[] = {static_cast<GameObj*>(lukeCharacter), static_cast<GameObj*>(darth.character()),
                                           static_cast<GameObj*>(emperor.character())};
            for (GameObj* character : battleCharacters) {
                accumulated = character->ApplyAllToView(1, &everySide) && accumulated ? 1 : 0;
                accumulated = character->ApplyAllToView(2, &everySide) && accumulated ? 1 : 0;
            }
        };
        applyToAllViews(ok);

        if (lukeCharacter->m_force >= g_gnprtb1807) {
            winningSide = kSideAlliance;
            ok = lukeCharacter->Virtual134(ctx) && ok ? 1 : 0;
            ObjectKey lukeKeyForDarth;
            ok = darth.character()->Virtual131(lukeCharacter->GetKey(&lukeKeyForDarth), ctx) && ok ? 1 : 0;
            ObjectKey lukeKeyForEmperor;
            ok = emperor.character()->Virtual131(lukeCharacter->GetKey(&lukeKeyForEmperor), ctx) && ok ? 1 : 0;
            ok = darth.character()->SetCanEscape(0, ctx) && ok ? 1 : 0;
            ok = emperor.character()->SetCanEscape(0, ctx) && ok ? 1 : 0;
        } else {
            winningSide = kSideEmpire;
            ok = lukeCharacter->SetCanEscape(0, ctx) && ok ? 1 : 0;
            ok = InjureStoryCharacter(ctx) && ok ? 1 : 0;
        }
        applyToAllViews(ok);
    }

    if (ok) {
        const uint32_t emperorSide = emperor.character()->ControlSide();
        ok = luke.character()->ControlSide() == emperorSide && darth.character()->ControlSide() == emperorSide ? 1 : 0;
    }

    bool needsRescue = false;
    if (luke.m_character && luke.m_location && darth.m_character && darth.m_location) {
        const GameObjRoot* person = winningSide == kSideAlliance ? luke.m_character : darth.m_character;
        const GameObjRoot* holder = winningSide == kSideAlliance ? luke.m_location : darth.m_location;
        needsRescue = ((holder->m_flags ^ person->m_flags) & kControlSideMask) != 0;
    }
    if (!needsRescue) return ok;

    ObjectKeyValueSet team;
    ObjectKey personKey;
    team.AddKeyValue(luke.character()->GetKey(&personKey), 0);
    team.AddKeyValue(darth.character()->GetKey(&personKey), 0);
    team.AddKeyValue(emperor.character()->GetKey(&personKey), 0);
    ObjectKey destinationKey;
    GameObj* rescued = static_cast<GameObj*>(winningSide == kSideAlliance ? luke.m_character : darth.m_character);
    ok = FindPersonDestination(rescued, 0, &destinationKey) && ok ? 1 : 0;
    if (destinationKey.OrNull()) ok = CreateMissionType41WithLevels(winningSide, &destinationKey, &team, ctx) && ok ? 1 : 0;
    return ok;
}
