#include "core/reb_abi.h"
#include <new>

#include "character/character.h"
#include "core/vptr_store.h"
#include "data/gnprtb.h"
#include "data/sd_records.h"
#include "gameobj/object_family.h"


extern int g_characterEventKindsBuilt;       // 0x6b2938
extern EventKindSet g_characterEventKinds;   // 0x6b2940

// The kind-name formatter shared with GameObj (0x4f6030).
void REB_THISCALL AppendGameObjName(ObjectBase* base, String* out, int kind);

namespace {

constexpr uint32_t kExistingFlag = 0x40;
constexpr uint32_t kOnMissionFlag = 0x80;
constexpr uint32_t kCapturedFlag = 0x1;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusCharacterCategory = 0x30;
constexpr int kStatusCommanding = 1;
constexpr int kStatusCaptured = 2;
constexpr int kStatusInjured = 3;
constexpr int kStatusNotQualified = 4;
constexpr int kStatusNoForce = 6;
constexpr int kActivityKindInjuryBlocked = 0x240;
constexpr int kNameKindForceTitle = 7;
constexpr int16_t kForceTitleGroup = 2;
constexpr int kRecordCommandIndexNone = 0;
constexpr int kRecordCommandIndexCommander = 1;
constexpr int kRecordCommandIndexAdmiral = 2;
constexpr int kRecordCommandIndexGeneral = 3;

void setStatus(StatusCode* out, int category, int code) { *out = StatusCode(category, code); }

bool statusIsOk(const StatusCode* status) { return status->m_second == static_cast<uint32_t>(kStatusOk); }

int16_t forceTitleStringId(uint16_t force) {
    if (force < 10) return static_cast<int16_t>(0x8754);
    if (force < 20) return static_cast<int16_t>(0x8749);
    if (force < 80) return static_cast<int16_t>(0x8750);
    if (force < 100) return static_cast<int16_t>(0x8751);
    if (force < 120) return static_cast<int16_t>(0x8752);
    return static_cast<int16_t>(0x8753);
}

}  // namespace

// ---- construction -----------------------------------------------------------------------------------------

Character::Character()
    : Role(),
      m_enhancedDiplomacy(0),
      m_enhancedEspionage(0),
      m_enhancedShipyardRD(0),
      m_enhancedTrainingFacilRD(0),
      m_enhancedConstructionYardRD(0),
      m_enhancedCombat(0),
      m_enhancedLeadership(0),
      m_enhancedLoyalty(0),
      m_force(0),
      m_forceExperience(0),
      m_forceTraining(0),
      m_leadershipAdjustment(0),
      m_injury(0),
      m_commandKind(0),
      m_state(4),
      m_missionHyperdriveModifier(static_cast<int16_t>(g_gnprtbDefault)),
      m_encounterKey(),
      m_traitorDiscoveredKey(),
      m_forceUserDiscoveredKey(),
      m_commandingKey(),
      m_flagsAc(0) {}

Character::~Character() { REB_KEEP_VPTR_STORE(); }

// The secondary vtable slot is entered with `this` pointing at the ObjectBase subobject.
void REB_THISCALL AppendCharacterName(ObjectBase* base, String* out, int kind) {
    if (kind != kNameKindForceTitle) {
        AppendGameObjName(base, out, kind);
        return;
    }
    const Character* character = static_cast<Character*>(base);
    const ResourceStringId title = {forceTitleStringId(static_cast<uint16_t>(character->m_force)), kForceTitleGroup};
    String text;
    text.SetFromResource(&title);
    *out += text;
}

void Character::WriteName(String* out, int kind) { AppendCharacterName(this, out, kind); }

int Character::SetRecord(DatRecord* record) {
    if (!record) return 0;
    const int familyMatches = AssignRecord(record);
    if (familyMatches) SetId(record->m_id);
    return familyMatches;
}

MajorCharacterSD* Character::GetCharacterRecord() const { return static_cast<MajorCharacterSD*>(m_record); }

GameObjData* Character::CreateData() { return new CharacterData(); }

// ---- queries over the SD record -----------------------------------------------------------------------------

int Character::RecordCanBeAdmiral() {
    MajorCharacterSD* record = GetCharacterRecord();
    return record ? record->m_moreFields[4] : 0;
}

int Character::RecordCanBeCommander() {
    MajorCharacterSD* record = GetCharacterRecord();
    return record ? record->m_moreFields[5] : 0;
}

int Character::RecordCanBeGeneral() {
    MajorCharacterSD* record = GetCharacterRecord();
    return record ? record->m_moreFields[6] : 0;
}

int Character::RecordIsJediTrainer() {
    MajorCharacterSD* record = GetCharacterRecord();
    return record ? record->m_moreFields[8] : 0;
}

int Character::RecordIsUnableToBetray() {
    MajorCharacterSD* record = GetCharacterRecord();
    return record ? record->m_moreFields[7] : 0;
}

int GetCharacterMemberMask() { return 0x10000; }

ObjectKey* Character::GetHolderKey(ObjectKey* out) {
    const ObjectKey fallback;
    const CharacterData* data = static_cast<CharacterData*>(m_data);
    new (out) ObjectKey(data ? data->m_holderKey : fallback);
    return out;
}

int Character::GetDataField34() {
    const CharacterData* data = static_cast<CharacterData*>(m_data);
    return data ? data->m_field34 : 0;
}

int Character::CanTrainForce(int* out) {
    if (RecordIsJediTrainer() && HasForcePotentialA15(m_force)) {
        *out = 1;
        return 1;
    }
    *out = 0;
    return 1;
}

int Character::GetRecordSide() {
    const MajorCharacterSD* record = GetCharacterRecord();
    int side = 0;
    if (record) {
        if (record->m_field40 == 0) {
            if (record->m_field44 == 0) return 0;
        } else if (record->m_field44 == 0) {
            return 1;
        }
        if (record->m_field40 == 0) side = 2;
    }
    return side;
}

// ---- virtual overrides --------------------------------------------------------------------------------------

int Character::GetTransitSpeed(int arg) {
    int speed = 0;
    if (m_stateFlags & kExistingFlag) speed = m_missionHyperdriveModifier;
    return speed;
}

int Character::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_characterEventKinds;
    return result;
}

int Character::CanBeControlledBy(int side) { return side != 3; }

int Character::CheckOrder(int a, int orderKind, StatusCode* out) {
    setStatus(out, kStatusCategory, kStatusNotSupported);
    const int result = BuildEventKinds();
    if (result && g_characterEventKinds.Contains(orderKind)) setStatus(out, kStatusCategory, kStatusOk);
    return result;
}

int Character::CheckActivity(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusCategory, kStatusOk);
    const int result = Role::CheckActivity(request, out);
    if (result) {
        if (statusIsOk(out) && m_commandKind != 0) setStatus(out, kStatusCharacterCategory, kStatusCommanding);
        if (result && statusIsOk(out) && (m_flagsAc & kCapturedFlag))
            setStatus(out, kStatusCharacterCategory, kStatusCaptured);
    }
    return result;
}

int Character::CheckActivity2(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusCategory, kStatusOk);
    const int result = Role::CheckActivity2(request, out);
    if (result) {
        if (statusIsOk(out) && request->m_kind == kActivityKindInjuryBlocked && m_injury != 0)
            setStatus(out, kStatusCharacterCategory, kStatusInjured);
        if (result && statusIsOk(out) && request->m_kind == kActivityKindInjuryBlocked && (m_flagsAc & kCapturedFlag))
            setStatus(out, kStatusCharacterCategory, kStatusCaptured);
    }
    return result;
}

int Character::Virtual179(intptr_t a, int* out) {
    const ActivityRequest* request = reinterpret_cast<const ActivityRequest*>(a);
    int result = 1;
    *out = 0;
    StatusCode status;
    CheckActivity3(request, &status);
    if (statusIsOk(&status)) {
        int index = 0;
        result = MapTypeCodeToIndex(request->m_kind, &index);
        if (result) *out = m_commandKind == index && m_commandKind != 0 ? 1 : 0;
    }
    return result;
}

int Character::CheckActivity3(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusCategory, kStatusOk);
    int result = CheckActivityRequest(request, out);
    if (result) {
        if (statusIsOk(out) && (m_flags78 & kOnMissionFlag)) setStatus(out, kStatusCharacterCategory, kStatusCommanding);
        if (result) {
            if (statusIsOk(out) && m_injury != 0) setStatus(out, kStatusCharacterCategory, kStatusInjured);
            if (result && statusIsOk(out) && (m_flagsAc & kCapturedFlag))
                setStatus(out, kStatusCharacterCategory, kStatusCaptured);
        }
    }
    int index = 0;
    if (result) {
        result = MapTypeCodeToIndex(request->m_kind, &index);
        if (result && statusIsOk(out)) {
            int qualified = 1;
            switch (index) {
                case kRecordCommandIndexNone:
                    qualified = m_commandKind != 0;
                    break;
                case kRecordCommandIndexCommander:
                    qualified = RecordCanBeCommander();
                    break;
                case kRecordCommandIndexAdmiral:
                    qualified = RecordCanBeAdmiral();
                    break;
                case kRecordCommandIndexGeneral:
                    qualified = RecordCanBeGeneral();
                    break;
                default:
                    result = 0;
                    break;
            }
            if (result && !qualified) setStatus(out, kStatusCharacterCategory, kStatusNotQualified);
        }
    }
    return result;
}

int Character::CheckActivityStatus3(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusCategory, kStatusOk);
    return CheckActivity3(request, out);
}

int Character::CheckActivity4(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusCategory, kStatusOk);
    int result = CheckActivityRequest(request, out);
    if (result) {
        if (statusIsOk(out) && (m_flags78 & kOnMissionFlag)) setStatus(out, kStatusCharacterCategory, kStatusCommanding);
        if (result) {
            if (statusIsOk(out) && m_injury != 0) setStatus(out, kStatusCharacterCategory, kStatusInjured);
            if (result) {
                if (statusIsOk(out) && (m_flagsAc & kCapturedFlag))
                    setStatus(out, kStatusCharacterCategory, kStatusCaptured);
                if (result && statusIsOk(out) && !HasForcePotentialA13(m_force))
                    setStatus(out, kStatusCharacterCategory, kStatusNoForce);
            }
        }
    }
    return result;
}

int Character::CheckActivityStatus4(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusCategory, kStatusOk);
    return CheckActivity4(request, out);
}

int Character::GetDiplomacy() { return m_enhancedDiplomacy; }
int Character::GetEspionage() { return m_enhancedEspionage; }
int Character::GetShipyardRD() { return m_enhancedShipyardRD; }
int Character::GetTrainingFacilRD() { return m_enhancedTrainingFacilRD; }
int Character::GetConstructionYardRD() { return m_enhancedConstructionYardRD; }
int Character::GetCombat() { return m_enhancedCombat; }
int Character::GetLeadership() { return m_enhancedLeadership; }
int Character::GetLoyalty() { return m_enhancedLoyalty; }

int Character::Virtual117() { return m_flagsAc & kCapturedFlag; }

// ---- statics and iterators ----------------------------------------------------------------------------------

int Character::BuildEventKinds() {
    if (g_characterEventKindsBuilt) return 1;
    g_characterEventKindsBuilt = 1;
    return Role::InitEventKinds(0, &g_characterEventKinds) && g_characterEventKinds.Add(0x260, 1) != nullptr &&
           g_characterEventKinds.Add(0x261, 1) != nullptr && g_characterEventKinds.Add(0x262, 1) != nullptr &&
           g_characterEventKinds.Add(0x263, 1) != nullptr && g_characterEventKinds.Add(0x268, 1) != nullptr;
}

int Character::InitEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    *out = g_characterEventKinds;
    return result;
}


CharacterIterator::CharacterIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &family::kCharacters, stateFilter) {}

CharacterIterator::CharacterIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &family::kCharacters, stateFilter) {}

CharacterIterator::~CharacterIterator() { REB_KEEP_VPTR_STORE(); }
EventKindSet g_characterEventKinds{};
int g_characterEventKindsBuilt{};
int g_gnprtb1C00{};
int g_gnprtb1C01{};
int g_gnprtb1C02{};
int g_gnprtbA00{};
int g_gnprtbA01{};
int g_gnprtbA02{};
int g_gnprtbA03{};
int g_gnprtbA04{};
int g_gnprtbA05{};
int g_gnprtbA06{};
int g_gnprtbA07{};
int g_gnprtbA08{};
int g_gnprtbA09{};
int g_gnprtbA0A{};
int g_gnprtbA0B{};
int g_gnprtbA0C{};
int g_gnprtbA0D{};
int g_gnprtbA0E{};
int g_gnprtbA0F{};
int g_gnprtbA10{};
int g_gnprtbA11{};
int g_gnprtbA12{};
int g_gnprtbA13{};
int g_gnprtbA14{};
int g_gnprtbA15{};
int g_gnprtbA16{};
int g_gnprtbA17{};
int g_gnprtbA18{};
int g_gnprtbA19{};
int g_gnprtbA20{};
int g_gnprtbA21{};
int g_gnprtbDefault{};
int g_gnprtbDifficulty{};
int g_sdprtbA00Side1{};
int g_sdprtbA00Side2{};
int g_sdprtbA01Side1{};
int g_sdprtbA01Side2{};
int g_sdprtbA02Side1{};
int g_sdprtbA02Side2{};
