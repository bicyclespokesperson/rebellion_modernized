#include "character/character_mgr.h"

#include "data/dat_record.h"
#include "data/game_math.h"
#include "data/gnprtb.h"
#include "data/random.h"
#include "data/sd_records.h"
#include "economy/side.h"
#include "rules/loyalty_params.h"
#include "events/command_classes.h"
#include "fleet/capship.h"
#include "fleet/fleet.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "galaxy/system_rules.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_queries.h"
#include "mission/mission_rules.h"
#include "story/story_characters.h"

// Setup of the people of a new game, the checks the person queries forward, and the reactions to what happens to people.

DatRecord* GetFirstRecordOfType(int type);  // 0x51cad0
ObjectKey* __thiscall GetAiRecordKey(DatRecord* record, ObjectKey* out);  // 0x402e40
int __thiscall FUN_00556c70(GameObjNotifier* rules, ObjectKey* key, int side, NotifyContext* ctx);

namespace {

constexpr uint32_t kCoruscantKey = 0x90000109;
constexpr uint32_t kYavinKey = 0x92000121;
constexpr uint32_t kEmperorKey = 0x34000280;
constexpr uint32_t kDarthKey = 0x35000281;
constexpr uint32_t kLukeKey = 0x32000242;
constexpr uint32_t kLeiaKey = 0x31000241;
constexpr uint32_t kHanKey = 0x33000243;
constexpr uint32_t kMonMothmaKey = 0x30000240;
constexpr uint32_t kChewbaccaKey = 0x38000343;
constexpr uint32_t kBossKeys[] = {0x38000380, 0x38000381, 0x38000382, 0x38000383, 0x3800038c};
constexpr uint32_t kHeroKeys[] = {kLukeKey, kLeiaKey, kHanKey, kChewbaccaKey, 0x38000344, 0x38000341};
constexpr int kStateUsable = 1;
constexpr int kStateExisting = 3;
constexpr int kStateAlways = 4;
constexpr uint32_t kCreatedFlag = 0x2;
constexpr uint32_t kDestroyedFlag = 0x8;
constexpr uint32_t kCapturedFlag = 0x1;
constexpr uint32_t kForceAwareFlag = 0x10;
constexpr uint32_t kForcePotentialFlag = 0x20;
constexpr uint32_t kCanEscapeFlag = 0x100;
constexpr uint32_t kDiscoveringForceUserFlag = 0x80;
constexpr uint32_t kMissionFlagHidden = 0x80;
constexpr int kInjuryReasonEncounter = 10;
constexpr uint32_t kHeritageSystemId = 0x109;

bool inFamilies(const GameObjNotifier* object, uint32_t first, uint32_t last) {
    const uint32_t family = const_cast<GameObjNotifier*>(object)->GetFamily();
    return family >= first && family < last;
}

bool inFamilies(const ObjectKey* key, uint32_t first, uint32_t last) {
    const uint32_t family = key->Family();
    return family >= first && family < last;
}

ObjectKey keyOf(uint32_t value) {
    ObjectKey key;
    key.m_value = value;
    return key;
}

GameObj* asGameObj(GameObjNotifier* object) { return static_cast<GameObj*>(object); }
Character* asCharacter(GameObjNotifier* object) { return static_cast<Character*>(object); }

DatRecord* nextRecord(DatRecord* record) { return reinterpret_cast<DatRecord*>(static_cast<AvlNode*>(record)->m_successor); }

int contextAsArgument(NotifyContext* ctx) { return static_cast<int>(reinterpret_cast<intptr_t>(ctx)); }

}  // namespace

// ---- location, counting -----------------------------------------------------------------------------------------------

int CharacterMgr::FindLocation(GameObj* person, GameObjNotifier** ship, GameObjNotifier** fleet, GameObjNotifier** system) {
    *ship = nullptr;
    *fleet = nullptr;
    *system = nullptr;
    GameObjNotifier* parent = nullptr;
    int result = person->GetParent(&parent);
    if (result) {
        if (inFamilies(parent, 0x14, 0x1c)) {
            *ship = parent;
        } else if (inFamilies(parent, 0x90, 0x98)) {
            *system = parent;
        }
        if (result) {
            if (*ship != nullptr) {
                parent = nullptr;
                result = asGameObj(*ship)->GetParent(&parent);
                if (!result) return 0;
                if (inFamilies(parent, 8, 0x10)) *fleet = parent;
            }
            if (result && *fleet != nullptr) {
                parent = nullptr;
                result = asGameObj(*fleet)->GetParent(&parent);
                if (result && inFamilies(parent, 0x90, 0x98)) *system = parent;
            }
        }
    }
    return result;
}

int CharacterMgr::CountUncreatedMinors(int side) {
    int count = 0;
    GameObjNotifier* root = GetGameRootMember_b0();
    if (root != nullptr) {
        MinorCharacterIterator it(root, kStateAlways, side);
        for (it.Begin(); it.Current() != nullptr; it.Advance()) {
            if (!(asGameObj(it.Current())->m_stateFlags & kCreatedFlag)) ++count;
        }
    }
    return count;
}

// ---- forwarded person checks (slots 111-115) ----------------------------------------------------------------------------

int CharacterMgr::ForwardVirtual179(const ObjectKey* key, int a, int* out) {
    int result = 1;
    GameObjNotifier* object = GetMasterInFamilies_01_ff(key);
    if (object != nullptr) {
        if (!inFamilies(object, 0x30, 0x3c)) result = 0;
        if (result) result = asCharacter(object)->Virtual179(a, out);
    }
    return result;
}

int CharacterMgr::ForwardCheckActivity3(const ObjectKey* key, const ActivityRequest* request, StatusCode* out) {
    *out = StatusCode(1, 0xffffffff);
    int result = 1;
    GameObjNotifier* object = GetMasterInFamilies_01_ff(key);
    if (object != nullptr) {
        if (!inFamilies(object, 0x30, 0x3c)) result = 0;
        if (result) {
            Character* person = asCharacter(object);
            GameObjNotifier* ship = nullptr;
            GameObjNotifier* fleet = nullptr;
            GameObjNotifier* system = nullptr;
            int index = 0;
            result = 0;
            if (MapTypeCodeToIndex(request->m_kind, &index)) {
                result = FindLocation(person, &ship, &fleet, &system);
                if (result && system == nullptr) *out = StatusCode(0x30, 4);
            }
            if (result) {
                if (out->m_second == 0xffffffff && fleet == nullptr && index == 2) *out = StatusCode(0x30, 5);
                if (result && out->m_second == 0xffffffff) result = person->CheckActivity3(request, out);
            }
        }
    }
    return result;
}

int CharacterMgr::ForwardCheckActivityStatus3(const ObjectKey* key, const ActivityRequest* request, StatusCode* out) {
    *out = StatusCode(1, 1);
    int result = 1;
    GameObjNotifier* object = GetMasterInFamilies_01_ff(key);
    if (object != nullptr) {
        if (!inFamilies(object, 0x30, 0x3c)) result = 0;
        if (result) result = asCharacter(object)->CheckActivityStatus3(request, out);
    }
    return result;
}

int CharacterMgr::ForwardCheckActivity4(const ObjectKey* key, const ActivityRequest* request, StatusCode* out) {
    *out = StatusCode(1, 0xffffffff);
    int result = 1;
    GameObjNotifier* object = GetMasterInFamilies_01_ff(key);
    if (object != nullptr) {
        if (!inFamilies(object, 0x30, 0x3c)) result = 0;
        if (result && out->m_second == 0xffffffff) result = asCharacter(object)->CheckActivity4(request, out);
    }
    return result;
}

int CharacterMgr::ForwardCheckActivityStatus4(const ObjectKey* key, const ActivityRequest* request, StatusCode* out) {
    *out = StatusCode(1, 1);
    int result = 1;
    GameObjNotifier* object = GetMasterInFamilies_01_ff(key);
    if (object != nullptr) {
        if (!inFamilies(object, 0x30, 0x3c)) result = 0;
        Character* person = asCharacter(object);
        const int forwarded = result;
        result = 0;
        if (forwarded) result = person->CheckActivityStatus4(request, out);
        if (result && out->m_second == 0xffffffff) {
            const uint32_t controlSide = person->ControlSide();
            const int view = controlSide == 1 ? 1 : (controlSide == 2 ? 2 : 0);
            ObjectKey target;
            request->GetTarget(&target);
            Character* other = asCharacter(GetObjectInFamilies_30_3c(view, &target));
            result = other != nullptr;
            if (result && static_cast<uint32_t>(other->GetRecordSide()) != person->ControlSide()) *out = StatusCode(1, 0x24);
        }
    }
    return result;
}

// ---- game setup -------------------------------------------------------------------------------------------------------

int CharacterMgr::CreateRecordCharacter(DatRecord* record, NotifyContext* ctx) {
    SdRecord* sd = static_cast<SdRecord*>(record);
    uint32_t side = 0;
    if (sd->m_field40 != 0) {
        if (sd->m_field44 == 0) side = 1;
    } else if (sd->m_field44 != 0) {
        side = 2;
    }
    if (side == 0 || side > 3) return 0;
    GameObjNotifier* root = GetGameRootMember_b0();
    int result = root != nullptr;
    GameObj* created = nullptr;
    if (root != nullptr) {
        ObjectKey key;
        GetAiRecordKey(record, &key);
        created = asGameObj(root)->CreateChild(&key, static_cast<int>(side), ctx, 0, 0);
        result = created != nullptr && result;
    }
    if (created != nullptr) result = created->Virtual47(contextAsArgument(ctx)) && result;
    return result;
}

int CharacterMgr::CreateRecordCharacters(NotifyContext* ctx) {
    int result = 1;
    for (DatRecord* record = GetFirstRecordOfType(0x30); record != nullptr; record = nextRecord(record))
        result = CreateRecordCharacter(record, ctx) && result;
    for (DatRecord* record = GetFirstRecordOfType(0x38); record != nullptr; record = nextRecord(record))
        result = CreateRecordCharacter(record, ctx) && result;
    return result;
}

int CharacterMgr::PlaceCharacter(const ObjectKey* key, GameObjNotifier* location, NotifyContext* ctx) {
    GameObj* person = static_cast<GameObj*>(GetMasterInFamilies_30_3c(key));
    int result = person != nullptr;
    if (person != nullptr) {
        const uint32_t created = person->m_stateFlags & kCreatedFlag;
        result = !created && result;
        if (!created) {
            ObjectKey locationKey;
            location->GetKey(&locationKey);
            result = person->Reparent(&locationKey, ctx) && result;
            result = person->SetCreated(1, ctx) && result;
            result = person->SetCompleted(1, ctx) && result;
        }
    }
    return result;
}

int CharacterMgr::RecruitRandomMinor(GameObjNotifier* location, int side, ObjectKey* out, NotifyContext* ctx) {
    out->Reset();
    GameObjNotifier* root = GetGameRootMember_b0();
    int result = root != nullptr;
    if (root != nullptr) {
        int remaining = CountUncreatedMinors(side);
        if (remaining != 0) {
            --remaining;
            const int pick = GameRandom(remaining);
            GameObjNotifier* chosen = nullptr;
            int index = 0;
            MinorCharacterIterator it(root, kStateAlways, side);
            for (it.Begin(); it.Current() != nullptr && chosen == nullptr; it.Advance()) {
                if (!(asGameObj(it.Current())->m_stateFlags & kCreatedFlag)) {
                    if (index == pick) chosen = it.Current();
                    ++index;
                }
            }
            ObjectKey chosenKey;
            chosen->GetKey(&chosenKey);
            *out = chosenKey;
            ObjectKey placeKey;
            chosen->GetKey(&placeKey);
            result = PlaceCharacter(&placeKey, location, ctx) && result;
            if (remaining == 0) {
                const int mapped = side == 1 ? 1 : (side == 2 ? 2 : 0);
                result = mapped != 0 && result;
                if (mapped != 0) {
                    Side* sideObject = static_cast<Side*>(GetGameRootSideMemberThunk(mapped));
                    result = sideObject != nullptr && result;
                    if (sideObject != nullptr) result = sideObject->SetRecruitmentDone(1, ctx) && result;
                }
            }
        }
    }
    return result;
}

int CharacterMgr::PickRandomPlace(uint32_t side, GameObjNotifier** place, NotifyContext* ctx) {
    *place = nullptr;
    int systems = 0;
    int fleets = 0;
    for (GameObjNotifier* system = GetFirstMasterInFamilies_90_98(); system != nullptr;
         system = asGameObj(system)->FindNextSibling(&family::kSystems, 1)) {
        if ((asGameObj(system)->m_stateFlags & kStateUsable) && system->ControlSide() == side) ++systems;
    }
    for (GameObjNotifier* fleet = GetFirstMasterInFamilies_08_10(); fleet != nullptr;
         fleet = asGameObj(fleet)->FindNextSibling(&family::kFleets, 1)) {
        asGameObj(fleet)->UpdateUsable(ctx);
        if ((asGameObj(fleet)->m_stateFlags & kStateUsable) && fleet->ControlSide() == side) ++fleets;
    }
    int pick = GameRandom(fleets - 1 + systems);
    if (pick < systems) {
        int index = 0;
        for (GameObjNotifier* system = GetFirstMasterInFamilies_90_98(); system != nullptr && *place == nullptr;
             system = asGameObj(system)->FindNextSibling(&family::kSystems, 1)) {
            if ((asGameObj(system)->m_stateFlags & kStateUsable) && system->ControlSide() == side) {
                if (index == pick) *place = system;
                ++index;
            }
        }
    } else {
        pick -= systems;
        GameObjNotifier* chosenFleet = nullptr;
        int index = 0;
        for (GameObjNotifier* fleet = GetFirstMasterInFamilies_08_10(); fleet != nullptr && chosenFleet == nullptr;
             fleet = asGameObj(fleet)->FindNextSibling(&family::kFleets, 1)) {
            if ((asGameObj(fleet)->m_stateFlags & kStateUsable) && fleet->ControlSide() == side) {
                if (index == pick) chosenFleet = fleet;
                ++index;
            }
        }
        int shipCount = 0;
        {
            CapShipIterator counter(chosenFleet, kStateUsable, side);
            shipCount = counter.Count();
        }
        const int shipPick = GameRandom(shipCount - 1);
        CapShipIterator it(chosenFleet, kStateUsable, side);
        index = 0;
        for (it.Begin(); it.Current() != nullptr && *place == nullptr; it.Advance()) {
            if (index == shipPick) *place = it.Current();
            ++index;
        }
    }
    return *place != nullptr;
}

int CharacterMgr::RecruitMinorAtRandomPlace(int side, ObjectKey* out, NotifyContext* ctx) {
    out->Reset();
    GameObjNotifier* place = nullptr;
    int result = PickRandomPlace(side, &place, ctx) != 0;
    if (place != nullptr) result = RecruitRandomMinor(place, side, out, ctx) && result;
    return result;
}

int CharacterMgr::PlaceAtRandomSystem(const ObjectKey* key, NotifyContext* ctx) {
    GameObjNotifier* person = GetMasterInFamilies_30_3c(key);
    int result = person != nullptr;
    if (person != nullptr) {
        GameObjNotifier* place = nullptr;
        result = PickRandomPlace(person->ControlSide(), &place, ctx) && result;
        if (place != nullptr) result = PlaceCharacter(key, place, ctx) && result;
    }
    return result;
}

int CharacterMgr::PlaceStartingCharacters(NotifyContext* ctx) {
    ObjectKey recruited;
    ObjectKey coruscantKey = keyOf(kCoruscantKey);
    GameObjNotifier* coruscant = GetMasterInFamilies_90_98(&coruscantKey);
    ObjectKey yavinKey = keyOf(kYavinKey);
    GameObjNotifier* yavin = GetMasterInFamilies_90_98(&yavinKey);
    GameObjNotifier* firstFacility = GetFirstMasterInFamilies_20_22();
    GameObjNotifier* headquarters = firstFacility != nullptr ? firstFacility->m_parent : nullptr;
    if (coruscant == nullptr || yavin == nullptr || headquarters == nullptr) return 0;

    ObjectKey key = keyOf(kEmperorKey);
    int ok = PlaceCharacter(&key, coruscant, ctx) != 0;
    key = keyOf(kDarthKey);
    ok = PlaceAtRandomSystem(&key, ctx) && ok;
    for (const uint32_t bossKey : kBossKeys) {
        key = keyOf(bossKey);
        ok = PlaceAtRandomSystem(&key, ctx) && ok;
    }
    int count = 0;
    ok = GetStartingCharacterCount(2, m_galaxySize, &count) && ok;
    for (int i = 0; i < count; ++i) ok = RecruitMinorAtRandomPlace(2, &recruited, ctx) && ok;

    key = keyOf(kMonMothmaKey);
    ok = PlaceCharacter(&key, headquarters, ctx) && ok;
    for (const uint32_t heroKey : kHeroKeys) {
        key = keyOf(heroKey);
        ok = PlaceCharacter(&key, yavin, ctx) && ok;
    }
    ok = GetStartingCharacterCount(1, m_galaxySize, &count) && ok;
    for (int i = 0; i < count; ++i) ok = RecruitMinorAtRandomPlace(1, &recruited, ctx) && ok;
    return ok;
}

// ---- loyalty -----------------------------------------------------------------------------------------------------------

int CharacterMgr::AdjustAllLoyalty(int amount, int side, NotifyContext* ctx) {
    int result = 0;
    if (side >= 1 && side <= 3) {
        result = side != 3;
        if (result && amount != 0) {
            for (GameObjNotifier* person = GetFirstMasterInFamilies_30_3c(); person != nullptr;
                 person = NextCharacterSibling(asGameObj(person)))
                result = asCharacter(person)->AdjustLoyaltyForSide(amount, side, ctx) && result;
        }
    }
    return result;
}

int CharacterMgr::ShiftLoyaltyForSystem(System* system, int side, NotifyContext* ctx) {
    int result = 1;
    if (side != 0) {
        const int empire = system->GetDataWord68(2);
        const int alliance = system->GetDataWord68(1);
        result = AdjustAllLoyalty(ComputeSideShare(side, alliance, empire), side, ctx) != 0;
    }
    return result;
}

int CharacterMgr::OnSystemOwnerChanged(const ObjectKey* systemKey, int destroyed, NotifyContext* ctx) {
    System* system = static_cast<System*>(GetMasterInFamilies_90_98(systemKey));
    int result = system != nullptr;
    if (system != nullptr && destroyed == 0) {
        const int owner = system->GetDataWord60();
        if (owner != 3) {
            int side = owner;
            if (!(system->m_stateFlags & kDestroyedFlag) && static_cast<uint32_t>(owner) != system->ControlSide())
                side = owner != 1 ? 1 : 2;
            const int alliance = system->SumWords48And54(1);
            const int empire = system->SumWords48And54(2);
            if (!AdjustAllLoyalty(ComputeSideShare(side, alliance, empire), side, ctx)) result = 0;
        }
    }
    return result;
}

int CharacterMgr::OnEmperorMoved(const ObjectKey* object, const ObjectKey* from, const ObjectKey* to, int unused,
                                 NotifyContext* ctx) {
    int result = 1;
    GameObjNotifier* person = GetMasterInFamilies_01_ff(object);
    if (person != nullptr && inFamilies(person, 0x34, 0x35)) result = UpdateSeatOfPower(ctx) != 0;
    return result;
}

int CharacterMgr::OnEmperorChanged(const ObjectKey* object, int a, int b, NotifyContext* ctx) {
    GameObjNotifier* person = GetMasterInFamilies_30_3c(object);
    int result = person != nullptr;
    if (person != nullptr && inFamilies(person, 0x34, 0x35)) result = UpdateSeatOfPower(ctx) && result;
    return result;
}

int CharacterMgr::OnSystemControlChanged(const ObjectKey* systemKey, uint32_t* before, uint32_t* after, NotifyContext* ctx) {
    int result = 1;
    if (inFamilies(systemKey, 0x90, 0x98)) {
        System* system = static_cast<System*>(GetMasterInFamilies_90_98(systemKey));
        if (system != nullptr) result = OnCoruscantControlChanged(system, before, after, ctx) != 0;
    }
    return result;
}

int CharacterMgr::OnCoruscantControlChanged(System* system, uint32_t* before, uint32_t* after, NotifyContext* ctx) {
    int result = 1;
    if (system->m_id == kHeritageSystemId) result = ShiftLoyaltyForControlChange(system, before, after, ctx) != 0;
    return result;
}

int CharacterMgr::ShiftLoyaltyForControlChange(System* system, uint32_t* before, uint32_t* after, NotifyContext* ctx) {
    const uint32_t oldBits = *before;
    int result = 1;
    if (((oldBits ^ *after) & 3) != 0) {
        const int neither = (oldBits & 0x30) == 0 && (*after & 0x30) == 0;
        if ((oldBits & 3) != 3) {
            const int bonus = RollSideBonus(oldBits & 3, 0);
            result = AdjustAllLoyalty(bonus, *before & 3, ctx) != 0;
        }
        if ((*after & 3) != 3) {
            const int bonus = RollSideBonus(*after & 3, 1);
            result = AdjustAllLoyalty(bonus, *after & 3, ctx) && result;
        }
        (void)neither;
        return UpdateSeatOfPower(ctx) && result ? 1 : 0;
    }
    return result;
}

int CharacterMgr::ShiftLoyaltyForHeadquarters(GameObjNotifier* facility, int moved, NotifyContext* ctx) {
    int result = 1;
    if (moved == 0) result = AdjustAllLoyalty(RollBonusA20(), 1, ctx) != 0;
    return result;
}

int CharacterMgr::OnFacilityOrPersonMoved(const ObjectKey* object, const ObjectKey* other, const ObjectKey* unused, int moved,
                                          NotifyContext* ctx) {
    int result = 1;
    GameObjNotifier* first = GetMasterInFamilies_01_ff(object);
    if (first != nullptr) {
        if (inFamilies(first, 0x20, 0x22)) {
            GameObjNotifier* second = GetMasterInFamilies_01_ff(other);
            result = second != nullptr;
            if (second != nullptr) result = ShiftLoyaltyForHeadquarters(first, moved, ctx) && result;
        }
        if (inFamilies(first, 0x30, 0x3c) && moved != 0) result = UpdateLeadershipAdjustment(asCharacter(first), ctx) && result;
    }
    return result;
}

int CharacterMgr::OnLocationChanged(const ObjectKey* object, const ObjectKey* from, const ObjectKey* to, NotifyContext* ctx) {
    int result = 1;
    if (inFamilies(object, 0x30, 0x3c)) {
        Character* person = asCharacter(GetMasterInFamilies_30_3c(object));
        if (person == nullptr) return 0;
        if (inFamilies(person, 0x34, 0x35)) result = UpdateSeatOfPower(ctx) && result;
        if (person->m_commandKind == 0) return result;
        int loseCommand = 1;
        if (inFamilies(from, 0x14, 0x1c) && inFamilies(to, 0x14, 0x1c)) {
            GameObjNotifier* fromShip = GetMasterInFamilies_01_ff(from);
            result = fromShip != nullptr && result;
            GameObjNotifier* toShip = GetMasterInFamilies_01_ff(to);
            result = toShip != nullptr && result;
            ObjectKey fromParent;
            ObjectKey toParent;
            if (fromShip != nullptr) result = asGameObj(fromShip)->GetValidParentKey(&fromParent) && result;
            if (toShip != nullptr) result = asGameObj(toShip)->GetValidParentKey(&toParent) && result;
            if (fromParent.OrNull() != nullptr && toParent.OrNull() != nullptr && inFamilies(&fromParent, 8, 0x10) &&
                inFamilies(&toParent, 8, 0x10) && fromParent.m_value == toParent.m_value)
                loseCommand = 0;
        }
        if (loseCommand) result = person->SetCommandKind(0, ctx) && result;
        return result;
    }
    if (inFamilies(object, 0x14, 0x1c)) {
        GameObjNotifier* ship = GetMasterInFamilies_14_1c(object);
        if (ship == nullptr) return 0;
        CharacterIterator it(ship, kStateExisting);
        for (it.Begin(); it.Current() != nullptr; it.Advance()) {
            Character* member = asCharacter(it.Current());
            if (member->m_commandKind != 0) result = member->SetCommandKind(0, ctx) && result;
        }
        return result;
    }
    return 1;
}

// ---- Force discovery, encounters ------------------------------------------------------------------------------------------

int CharacterMgr::DiscoverForceUser(Character* discoverer, NotifyContext* ctx) {
    GameObjNotifier* parent = nullptr;
    int result = 0;
    if (discoverer->m_flagsAc & kDiscoveringForceUserFlag) result = discoverer->GetParent(&parent);
    if (result) {
        CharacterIterator it(parent, kStateUsable, discoverer->ControlSide());
        for (it.Begin(); it.Current() != nullptr; it.Advance()) {
            Character* candidate = asCharacter(it.Current());
            if (candidate->m_flagsAc & kCapturedFlag) continue;
            if (candidate->m_flags78 & kMissionFlagHidden) continue;
            ObjectKey discovererKey;
            discoverer->GetKey(&discovererKey);
            ObjectKey candidateKey;
            candidate->GetKey(&candidateKey);
            if (candidateKey.m_value == discovererKey.m_value) continue;
            if (!(candidate->m_flagsAc & kForcePotentialFlag)) continue;
            if (candidate->m_flagsAc & kForceAwareFlag) continue;
            int proceed = 0;
            ObjectKey keyAgain;
            candidate->GetKey(&keyAgain);
            if (keyAgain.m_value != kLeiaKey) {
                proceed = 1;
            } else {
                ObjectKey discovererAgain;
                discoverer->GetKey(&discovererAgain);
                if (discovererAgain.m_value == kLukeKey && (static_cast<Luke*>(discoverer)->m_storyFlags & Luke::kKnowsHeritage)) {
                    proceed = 1;
                    result = static_cast<Leia*>(candidate)->SetKnowsHeritage(1, ctx) && result;
                }
            }
            if (proceed) {
                const int aware = candidate->SetForceAware(1, ctx);
                ObjectKey discovered;
                candidate->GetKey(&discovered);
                result = discoverer->SetForceUserDiscoveredKey(&discovered, ctx) && aware && result ? 1 : 0;
            }
        }
    }
    return result;
}

int CharacterMgr::BeginEncounter(Character* detected, Character* detector, NotifyContext* ctx) {
    int bothFree = !detected->m_encounterKey.IsSet();
    if (bothFree) bothFree = !detector->m_encounterKey.IsSet();
    if (!bothFree) return 0;

    NotifyContext local(*ctx);
    local.m_flag04 = 1;
    local.m_reserved14 = CallGameHook2AFC();
    int ok = 1;
    ObjectKey ownerTemp;
    ObjectKey* owner = GetOwnerKeyValue(detected, &ownerTemp);
    if (inFamilies(owner, 0x90, 0x98)) {
        ObjectKey ownerAgain;
        System* system = static_cast<System*>(GetMasterInFamilies_90_98(GetOwnerKeyValue(detected, &ownerAgain)));
        if (system != nullptr) {
            const int first = system->SyncViewForEvent(6, 1, &local);
            const int second = system->SyncViewForEvent(6, 2, &local);
            ok = second && first;
        }
    }
    NotifyContext everyone(local);
    everyone.m_flag04 = 1;
    everyone.m_flag08 = 3;
    ok = detected->ApplyAllToView(1, &everyone) && ok;
    ok = detected->ApplyAllToView(2, &everyone) && ok;
    ok = detector->ApplyAllToView(1, &everyone) && ok;
    ok = detector->ApplyAllToView(2, &everyone) && ok;
    ObjectKey detectorKey;
    detector->GetKey(&detectorKey);
    ok = detected->SetEncounterKey(&detectorKey, &local) && ok;
    ObjectKey detectedKey;
    detected->GetKey(&detectedKey);
    const int result = detector->SetEncounterKey(&detectedKey, &local) && ok;
    detected->SetDataField34(local.m_reserved14);
    detector->SetDataField34(local.m_reserved14);
    return result;
}

int CharacterMgr::InjureByEscapeBase(Character* person, NotifyContext* ctx) {
    int duration = 0;
    const int rolled = ComputeEscapeBase(&duration);
    const int injured = person->Virtual188(duration, kInjuryReasonEncounter, ctx);
    return injured && rolled ? 1 : 0;
}

namespace {

struct EncounterRoles {
    Character* luke = nullptr;
    Character* leia = nullptr;
    Character* darth = nullptr;
    Character* emperor = nullptr;

    void classify(Character* person) {
        ObjectKey key;
        person->GetKey(&key);
        if (key.m_value == kLukeKey) {
            luke = person;
        } else if (key.m_value == kLeiaKey) {
            leia = person;
        } else if (key.m_value == kDarthKey) {
            darth = person;
        } else if (key.m_value == kEmperorKey) {
            emperor = person;
        }
    }
};

}  // namespace

namespace {

int forceExperienceGain(Character* learner, Character* teacher, int base, int percent) {
    int gain = base;
    const int difference = teacher->m_force - learner->m_force;
    if (difference > 0) gain = MaxInt(gain, ScalePercent(difference, percent));
    return gain;
}

}  // namespace

int CharacterMgr::ResolveEncounterFight(Character* member, Character* other, NotifyContext* ctx) {
    ObjectKey key;
    other->GetKey(&key);
    int valid = key.m_value == member->m_encounterKey.m_value;
    if (valid) {
        member->GetKey(&key);
        valid = key.m_value == other->m_encounterKey.m_value;
        if (valid) valid = other->GetDataField34() != 0 && member->GetDataField34() != 0;
    }
    if (!valid) return 0;
    if (other->GetDataField34() != member->GetDataField34()) return 0;

    NotifyContext local(*ctx);
    local.m_flag04 = 1;
    local.m_reserved14 = member->GetDataField34();
    const int oldInjury = member->m_injury;
    ObjectKey otherKey;
    other->GetKey(&otherKey);
    int ok = member->RollCombatCheck(&otherKey, other->GetCombat(), &local) != 0;
    int injuryOfMember = member->m_injury - oldInjury;
    int injuryOfOther = 0;
    const int otherCombat = other->GetCombat();
    const int memberCombat = member->GetCombat();
    if (injuryOfMember == 0) {
        ok = RollDurationFromDifference(memberCombat, otherCombat, &injuryOfMember) && ok;
        if (injuryOfMember != 0) ok = member->Virtual188(injuryOfMember, kInjuryReasonEncounter, &local) && ok;
    }
    if (injuryOfOther == 0) {
        ok = RollDurationFromDifference(otherCombat, memberCombat, &injuryOfOther) && ok;
        if (injuryOfOther != 0) ok = other->Virtual188(injuryOfOther, kInjuryReasonEncounter, &local) && ok;
    }
    if (injuryOfOther != 0) ok = member->SetBaseCombat(member->m_baseCombat + g_gnprtbA12, &local) && ok;
    if (injuryOfMember != 0) ok = other->SetBaseCombat(other->m_baseCombat + g_gnprtbA12, &local) && ok;

    ObjectKey unset;
    ok = member->SetEncounterKey(&unset, &local) && ok;
    ObjectKey unsetOther;
    ok = other->SetEncounterKey(&unsetOther, &local) && ok;
    member->SetDataField34(0);
    other->SetDataField34(0);
    NotifyContext everyone(local);
    everyone.m_flag04 = 1;
    everyone.m_flag08 = 3;
    ok = member->ApplyAllToView(1, &everyone) && ok;
    ok = member->ApplyAllToView(2, &everyone) && ok;
    ok = other->ApplyAllToView(1, &everyone) && ok;
    ok = other->ApplyAllToView(2, &everyone) && ok;

    EncounterRoles roles;
    roles.classify(member);
    roles.classify(other);
    Character* luke = roles.luke;
    Character* leia = roles.leia;
    if (luke != nullptr) {
        if (roles.darth != nullptr) {
            bool lukeFree = true;
            if (!(static_cast<Luke*>(luke)->m_storyFlags & Luke::kKnowsHeritage)) {
                ok = static_cast<Luke*>(luke)->SetKnowsHeritage(1, &local) && ok;
                if (luke->m_flagsAc & kCapturedFlag) {
                    lukeFree = false;
                } else if (luke->m_force < g_gnprtb0C05) {
                    ok = InjureByEscapeBase(luke, &local) && ok;
                }
            }
            if (lukeFree && !(luke->m_flagsAc & kCapturedFlag)) {
                const int gain = forceExperienceGain(luke, roles.darth, g_gnprtb0C0C, g_gnprtb0C00);
                ok = luke->SetForceExperience(luke->m_forceExperience + gain, ctx) && ok;
            }
        }
        if (roles.emperor != nullptr && !(luke->m_flagsAc & kCapturedFlag)) {
            const int gain = forceExperienceGain(luke, roles.emperor, g_gnprtb0C0D, g_gnprtb0C01);
            ok = luke->SetForceExperience(luke->m_forceExperience + gain, ctx) && ok;
        }
    }
    if (leia != nullptr) {
        if (roles.darth != nullptr && !(leia->m_flagsAc & kCapturedFlag) && (leia->m_flagsAc & kForceAwareFlag)) {
            const int gain = forceExperienceGain(leia, roles.darth, g_gnprtb0C0E, g_gnprtb0C02);
            ok = leia->SetForceExperience(leia->m_forceExperience + gain, ctx) && ok;
        }
        if (roles.emperor != nullptr && !(leia->m_flagsAc & kCapturedFlag) && (leia->m_flagsAc & kForceAwareFlag)) {
            const int gain = forceExperienceGain(leia, roles.emperor, g_gnprtb0C0F, g_gnprtb0C03);
            ok = leia->SetForceExperience(leia->m_forceExperience + gain, ctx) && ok;
        }
    }
    return ok;
}

int CharacterMgr::AssignCommand(const ObjectKey* personKey, const ActivityRequest* request, NotifyContext* ctx) {
    GameObjNotifier* object = GetMasterInFamilies_01_ff(personKey);
    if (object == nullptr) return 0;
    if (!inFamilies(object, 0x30, 0x3c)) return 0;
    Character* person = asCharacter(object);
    int index = 0;
    int ok = MapTypeCodeToIndex(request->m_kind, &index) != 0;
    if (person->m_commandKind == index) index = 0;
    ObjectKey holderKey;
    if (index != 0) {
        GameObjNotifier* ship = nullptr;
        GameObjNotifier* fleet = nullptr;
        GameObjNotifier* system = nullptr;
        GameObjNotifier* other = nullptr;
        ok = FindLocation(person, &ship, &fleet, &system) && ok;
        if (system != nullptr) {
            ObjectKey locationKey;
            if (fleet != nullptr) {
                fleet->GetKey(&locationKey);
            } else {
                system->GetKey(&locationKey);
            }
            holderKey = locationKey;
            if (fleet != nullptr) {
                static_cast<Fleet*>(fleet)->FindShipByNumber(index, &other);
            } else {
                static_cast<System*>(system)->FindCharacterByCommandKind(index, &other);
            }
        }
        if (other != nullptr) {
            ObjectKey personKeyAgain;
            person->GetKey(&personKeyAgain);
            ObjectKey otherKey;
            other->GetKey(&otherKey);
            if (otherKey.m_value != personKeyAgain.m_value) asCharacter(other)->SetCommandKind(0, ctx);
        }
    }
    person->Virtual189(request, &holderKey, ctx);
    return 0;
}

int CharacterMgr::ReportTarget(const ObjectKey* personKey, const ActivityRequest* request, NotifyContext* ctx) {
    int result = 1;
    GameObjNotifier* object = GetMasterInFamilies_01_ff(personKey);
    if (object != nullptr) {
        if (!inFamilies(object, 0x30, 0x3c)) result = 0;
        if (inFamilies(object, 0x30, 0x3c)) {
            GameObjNotifier* rules = GetGameRootMember_bc();
            result = rules != nullptr && result;
            if (rules != nullptr) {
                ObjectKey target;
                request->GetTarget(&target);
                result = FUN_00556c70(rules, &target, static_cast<int>(object->ControlSide()), ctx) && result;
            }
        }
    }
    return result;
}

int CharacterMgr::RollCaptiveEscape(Character* captive, int value, NotifyContext* ctx) {
    int result = 1;
    if (value != 0) {
        const uint32_t flags = captive->m_flagsAc;
        int escaped = 0;
        GameObjNotifier* holder = nullptr;
        int ok = captive->GetParent(&holder) && (flags & kCanEscapeFlag);
        int average = 0;
        int troops = 0;
        if ((captive->m_flagsAc & kCanEscapeFlag) && holder != nullptr) {
            int members = 0;
            int total = 0;
            CharacterIterator it(holder, kStateUsable, captive->ControlSide());
            for (it.Begin(); it.Current() != nullptr; it.Advance()) {
                Character* member = asCharacter(it.Current());
                if (member->Virtual117() == 0) {
                    ++members;
                    total += member->GetCombat();
                }
            }
            if (members != 0) average = total / members;
            TroopRegIterator troopIt(holder, kStateUsable, captive->ControlSide());
            troops = troopIt.Count();
        }
        const int espionage = captive->GetEspionage();
        const int combat = captive->GetCombat();
        const int rolled = RollEscapeTable(captive->GetRecordSide(), combat, espionage, average, troops, &escaped);
        result = rolled && ok;
        if (escaped != 0) result = captive->SetEscapeRequest(1, ctx) && result;
    }
    return result;
}
