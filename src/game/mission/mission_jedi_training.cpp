#include "data/sd_record_bases.h"
#include "mission/mission_jedi_training.h"

#include "character/character.h"
#include "data/random.h"
#include "gameobj/object_queries.h"
#include "gameobj/object_type_db.h"
#include "mission/mission_external.h"
#include "mission/mission_rules.h"


extern int g_missionJediTrainingEventKindsReady;      // 0x6bba14
extern EventKindSet g_missionJediTrainingEventKinds;  // 0x6bba18


// The MissionCheckArgs record as the Jedi Training target check reads it: the original names its fields by offset (+0x04
// the team flag, +0x08 the key-set flag, +0x10 the side, +0x18.. the three reference lists, +0x24.. the three key sets);
// a copy of those fields, so that the check does not depend on the record's memory layout.
struct TrainingOrderCheck {
    int m_hasTeam;
    int m_useKeySets;  // 0 = the three object-reference lists, else the three key sets
    int m_side;
    ObjectRefList* m_refLists[3];
    ObjectKeyValueSet* m_keySets[3];

    explicit TrainingOrderCheck(const MissionCheckArgs* args)
        : m_hasTeam(args->m_flag04),
          m_useKeySets(args->m_flag08),
          m_side(args->m_controlSide),
          m_refLists{static_cast<ObjectRefList*>(args->m_refs[0]), static_cast<ObjectRefList*>(args->m_refs[1]),
                     static_cast<ObjectRefList*>(args->m_refs[2])},
          m_keySets{args->m_team, args->m_decoys, args->m_captured} {}
};

// BUG(original): Read and Write handle the Teacher key but the checksum skips it (slot 7 is the shared forwarder to the
// rule-base sum, unlike Espionage's); keep it, multiplayer sync and save checksums depend on it.
int MissionJediTraining::Checksum(Crc32* crc) {
    return this->Mission::ChecksumImpl(crc);
}

namespace {
constexpr uint32_t kFamilyMissionJediTraining = 0x58;
constexpr FamilyRange kJediTrainingFamilies(0x58, 0x59);
constexpr int kEventTeacherChanged = 0x380;
constexpr int kTaskSucceeded = 3;
constexpr int kPhaseTeacherSelection = 2;
constexpr int kRequestPhase9 = 9;
constexpr int kRequestPhase10 = 10;
constexpr int kRequestPhase11 = 0xb;
constexpr int kStatusSourceOrder = 0x40;
constexpr int kReasonNoTeacher = 0x90;
constexpr int kReasonNoStudents = 0x94;
constexpr int kReasonNoForce = 5;
constexpr int kReasonStudentStronger = 0xd;


const SdRecordExt25* AsCharacterRecord(const void* record) { return static_cast<const SdRecordExt25*>(record); }

int RecordForceTotal(const void* record) {
    return static_cast<int>(AsCharacterRecord(record)->m_moreFields[2]) + static_cast<int>(AsCharacterRecord(record)->m_moreFields[3]);
}

int RecordFirstForceValue(const void* record) { return static_cast<int>(AsCharacterRecord(record)->m_moreFields[2]); }

bool InCharacterFamilies(uint32_t family) { return family >= 0x30 && family < 0x3c; }

uint32_t RecordKey(void* record) {
    const DatRecord* datRecord = static_cast<const DatRecord*>(record);
    return (datRecord->m_id & 0xffffff) | (datRecord->m_third << 24);
}

void Reject(StatusCode* status, int* out, int reason, int outValue) {
    *status = StatusCode(kStatusSourceOrder, reason);
    *out = outValue;
}

}  // namespace

// The helpers of CheckTarget (0x570940-0x570f80).
int FindTrainerInKeySet(int side, ObjectKeyValueSet* set, void** out) {  // 0x570e40
    int result = 1;
    *out = nullptr;
    for (AvlNode* node = set->First(); node && !*out && result; node = node->m_successor) {
        ObjectKey key;
        static_cast<ObjectKeyNode*>(node)->GetKey(&key);
        if (InCharacterFamilies(key.Family())) {
            ObjectKey lookupKey;
            static_cast<ObjectKeyNode*>(node)->GetKey(&lookupKey);
            Character* character = static_cast<Character*>(GetMasterInFamilies_30_3c(&lookupKey));
            result = character != nullptr;
            int canTrain = 0;
            if (result) {
                result = character->CanTrainForce(&canTrain);
                if (result && canTrain) *out = character;
            }
        }
    }
    if (result && *out && side != 1 && side != 2) result = 0;
    return result;
}

int FindTrainerInRefList(int side, ObjectRefList* list, void** out) {  // 0x570f80
    int result = 1;
    *out = nullptr;
    for (AvlNode* node = list->First(); node && !*out && result; node = node->m_successor) {
        ObjectKey key;
        static_cast<ObjectRefNode*>(node)->GetKey(&key);
        if (InCharacterFamilies(key.Family())) {
            ObjectKey lookupKey;
            static_cast<ObjectRefNode*>(node)->GetKey(&lookupKey);
            void* record = FindDataRecord(AsLookupKey(&lookupKey));
            result = record != nullptr;
            if (result && AsCharacterRecord(record)->m_moreFields[8] != 0) *out = record;
        }
    }
    if (result && *out && side != 1 && side != 2) result = 0;
    return result;
}

int CheckStudentKeySet(ObjectKeySet* set, TrainingOrderCheck* check, StatusCode* status, int* out) {  // 0x570940
    *status = MakeStatus(mission_status::kOk);
    int result = 1;
    void* teacher = nullptr;
    *out = 0;
    if (status->m_second == static_cast<uint32_t>(mission_status::kOk)) {
        result = FindTrainerInKeySet(check->m_side, check->m_keySets[0], &teacher);
        if (result && !teacher) Reject(status, out, kReasonNoTeacher, -1);
    }
    for (AvlNode* node = set->First(); node && result && status->m_second == static_cast<uint32_t>(mission_status::kOk);
         node = node->m_successor) {
        ObjectKey key;
        static_cast<ObjectKeyNode*>(node)->GetKey(&key);
        GameObj* any;
        GameObj* specialForces;
        GameObj* student;
        result = SplitRoleFromKey(&key, &any, &specialForces, &student);
        if (result && student && teacher) {
            if (static_cast<Character*>(student)->m_force == 0) Reject(status, out, kReasonNoForce, -1);
            if (status->m_second == static_cast<uint32_t>(mission_status::kOk)) {
                ObjectKey teacherKey;
                ObjectKey studentKey;
                static_cast<GameObjRoot*>(teacher)->GetKey(&teacherKey);
                static_cast<GameObjRoot*>(student)->GetKey(&studentKey);
                if (studentKey.m_value != teacherKey.m_value && static_cast<Character*>(teacher)->m_force <= static_cast<Character*>(student)->m_force)
                    Reject(status, out, kReasonStudentStronger, -1);
            }
        }
    }
    return result;
}

int CheckStudentRefList(ObjectRefList* list, TrainingOrderCheck* check, StatusCode* status, int* out) {  // 0x570bc0
    *status = MakeStatus(mission_status::kOk);
    int result = 1;
    void* teacher = nullptr;
    *out = 0;
    if (status->m_second == static_cast<uint32_t>(mission_status::kOk)) {
        result = FindTrainerInRefList(check->m_side, check->m_refLists[0], &teacher);
        if (result && !teacher) Reject(status, out, kReasonNoTeacher, -1);
    }
    for (AvlNode* node = list->First(); node && result && status->m_second == static_cast<uint32_t>(mission_status::kOk);
         node = node->m_successor) {
        ObjectKey key;
        static_cast<ObjectRefNode*>(node)->GetKey(&key);
        DatRecord* any;
        DatRecord* specialForces;
        DatRecord* student;
        result = SplitPersonRecord(&key, &any, &specialForces, &student);
        if (result && student && teacher) {
            if (RecordForceTotal(student) == 0) Reject(status, out, kReasonNoForce, -1);
            if (status->m_second == static_cast<uint32_t>(mission_status::kOk)) {
                const uint32_t studentKey = RecordKey(student);
                const uint32_t teacherKey = RecordKey(teacher);
                if (studentKey != teacherKey &&
                    RecordForceTotal(teacher) <= RecordFirstForceValue(student))
                    Reject(status, out, kReasonStudentStronger, -1);
            }
        }
    }
    return result;
}

MissionJediTraining::MissionJediTraining() {}

MissionJediTraining::~MissionJediTraining() { REB_KEEP_VPTR_STORE(); }

uint32_t MissionJediTraining::GetFamily() { return kFamilyMissionJediTraining; }

int MissionJediTraining::ApplyAll(NotifyContext* ctx) { return IsSideView() ? ApplyAllBody(ctx) : 0; }

int MissionJediTraining::ApplyAllBody(NotifyContext* ctx) { return ApplyAllWithExtraKey(this, ctx); }

int MissionJediTraining::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionJediTrainingEventKinds, out);
}

int MissionJediTraining::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionJediTrainingEventKinds, orderKind, out);
}

int MissionJediTraining::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionJediTraining::CreateViewObject() { return NewOrNull<MissionJediTraining>(); }

GameObjNotifier* MissionJediTraining::Create() { return NewOrNull<MissionJediTraining>(); }

int MissionJediTraining::SetTeacherKey(ObjectKey* value, NotifyContext* ctx) {
    return SetExtraKeyProperty(this, value, ctx);
}

int MissionJediTraining::ApplyExtraKey(NotifyContext* ctx) { return ApplyExtraKeyProperty(this, ctx); }

int MissionJediTraining::OnExtraKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogKey("MissionJediTrainingTeacherKeyNotif", "Teacher", newKey, oldKey, 0, ctx);
    return result;
}

int MissionJediTraining::RequestPhase9(NotifyContext* ctx) {
    int result = 1;
    if (IsDataField1cAbove6() && !HasCompletionStatus()) result = CompleteWith(kRequestPhase9, ctx) != 0;
    return result;
}

int MissionJediTraining::RequestPhase10(NotifyContext* ctx) {
    int result = 1;
    if (IsDataField1cAbove6() && !HasCompletionStatus()) result = CompleteWith(kRequestPhase10, ctx) != 0;
    return result;
}

int MissionJediTraining::RemoveMember(ObjectKey* member, int decoy, NotifyContext* ctx) {
    int result = Mission::RemoveMember(member, decoy, ctx) != 0;
    if (!HasCompletionStatus() && m_extraKey.m_value == member->m_value) {
        if (!CompleteWith(kRequestPhase11, ctx) || !result) return 0;
        return 1;
    }
    return result;
}

int MissionJediTraining::OnPhaseChanged(int oldValue, int newValue, NotifyContext* ctx) {
    int result = MissionRuleBase::OnPhaseChanged(oldValue, newValue, ctx) != 0;
    if (newValue != kPhaseTeacherSelection) return result;

    ObjectKeyValueSet team;
    MissionCharacterTeamIterator members(this);
    for (members.Begin(); members.Current(); members.Advance()) {
        ObjectKey key;
        members.Current()->GetKey(&key);
        result = team.AddKeyValue(&key, 0);
    }
    void* teacher = nullptr;
    const int found = FindTrainerInKeySet(static_cast<int>(ControlSide()), &team, &teacher);
    const int ok = found && result;
    result = teacher && ok ? 1 : 0;
    if (teacher) {
        ObjectKey teacherKey;
        static_cast<GameObjRoot*>(teacher)->GetKey(&teacherKey);
        result = SetTeacherKey(&teacherKey, ctx) && result ? 1 : 0;
    }
    return result;
}

int MissionJediTraining::Virtual157(GameObj* person, int* out) {
    *out = 0;
    const int isCharacter = InCharacterFamilies(person->GetFamily());
    Character* student = isCharacter ? static_cast<Character*>(person) : nullptr;
    Character* teacher = static_cast<Character*>(GetMasterInFamilies_30_3c(&m_extraKey));
    int result = teacher && isCharacter ? 1 : 0;
    if (teacher && student) {
        const int computed = ComputeJediTrainingDifference(student->m_force, teacher->m_force, out);
        result = computed && result ? 1 : 0;
    }
    return result;
}

int MissionJediTraining::Virtual158(NotifyContext* ctx) { return 1; }

int MissionJediTraining::Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) {
    int result = 1;
    *out = 0;
    if (!PercentChance(roll)) return result;
    if (!InCharacterFamilies(person->GetFamily())) result = 0;
    Character* student = result ? static_cast<Character*>(person) : nullptr;
    Character* teacher = static_cast<Character*>(GetMasterInFamilies_30_3c(&m_extraKey));
    result = teacher && result ? 1 : 0;
    if (!student || !teacher) return result;

    int gain;
    const int computed = ComputeJediTrainingGain(student->m_force, teacher->m_force, &gain);
    result = computed && result ? 1 : 0;
    if (student->m_force == gain) return result;
    const int trained = student->SetForceTraining(gain + (student->m_forceTraining - student->m_force), ctx);
    const int allOk = trained && result;
    return SetTaskStatus(kTaskSucceeded, ctx) && allOk ? 1 : 0;
}

int MissionJediTraining::Virtual160(NotifyContext* ctx) { return MissionStartIfIdle(this, ctx); }

int MissionJediTraining::CheckTarget(MissionCheckArgs* orderCheck, StatusCode* status, int* out) {
    TrainingOrderCheck checkView(orderCheck);
    TrainingOrderCheck* check = &checkView;
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    int result = MissionSystemTargetBase::CheckTarget(orderCheck, status, out);
    if (!check->m_hasTeam) return result;
    const int ok = static_cast<uint32_t>(mission_status::kOk);

    if (!check->m_useKeySets) {
        for (int i = 0; i < 3 && result; ++i) {
            if (status->m_second != static_cast<uint32_t>(ok)) break;
            result = CheckStudentRefList(check->m_refLists[i], check, status, out);
        }
    } else {
        for (int i = 0; i < 3 && result; ++i) {
            if (status->m_second != static_cast<uint32_t>(ok)) break;
            result = CheckStudentKeySet(check->m_keySets[i], check, status, out);
        }
    }

    if (result && status->m_second == static_cast<uint32_t>(ok)) {
        void* teacher = nullptr;
        int hasTeacher;
        if (!check->m_useKeySets) {
            result = FindTrainerInRefList(check->m_side, check->m_refLists[0], &teacher);
            hasTeacher = result && teacher;
        } else {
            result = FindTrainerInKeySet(check->m_side, check->m_keySets[0], &teacher);
            hasTeacher = result && teacher;
        }
        if (result && !hasTeacher) Reject(status, out, kReasonNoTeacher, -1);
    }

    if (!result) return 0;
    if (status->m_second != static_cast<uint32_t>(ok)) return result;
    if (!check->m_useKeySets) {
        if (check->m_refLists[0]->Size() > 1) return result;
        Reject(status, out, kReasonNoStudents, -1);
    } else {
        if (static_cast<uint16_t>(check->m_keySets[0]->Count()) > 1) return result;
        Reject(status, out, kReasonNoStudents, -1);
    }
    return result;
}

int MissionJediTraining::BuildEventKinds() {
    if (!g_missionJediTrainingEventKindsReady) {
        g_missionJediTrainingEventKindsReady = 1;
        return MissionSystemTargetBase::InitEventKinds(0, &g_missionJediTrainingEventKinds);
    }
    return 1;
}


MissionJediTrainingIterator::MissionJediTrainingIterator(GameObjNotifier* parent)
    : ChildIterator(parent, &kJediTrainingFamilies, 1) {}

MissionJediTrainingIterator::~MissionJediTrainingIterator() { REB_KEEP_VPTR_STORE(); }
EventKindSet g_missionJediTrainingEventKinds{};
int g_missionJediTrainingEventKindsReady{};
