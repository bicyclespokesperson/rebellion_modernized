#include "mission/mission_jedi_training.h"

#include "character/character.h"
#include "data/random.h"
#include "gameobj/object_queries.h"
#include "gameobj/object_type_db.h"
#include "mission/mission_external.h"
#include "mission/mission_rules.h"

extern "C" void __cdecl LAB_00571630();

extern int g_missionJediTrainingEventKindsReady;      // 0x6bba14
extern EventKindSet g_missionJediTrainingEventKinds;  // 0x6bba18


// The MissionOrderCheck record as the Jedi Training target check reads it.
struct TrainingOrderCheck {
    int m_unk00;                    // +0x00
    int m_hasTeam;                  // +0x04
    int m_useKeySets;               // +0x08: 0 = the three object-reference lists, else the three key sets
    int m_unk0c;                    // +0x0c
    int m_side;                     // +0x10
    int m_unk14;                    // +0x14
    ObjectRefList* m_refLists[3];   // +0x18, +0x1c, +0x20
    ObjectKeyValueSet* m_keySets[3];  // +0x24, +0x28, +0x2c
};

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


template <class T>
T FieldAt(void* object, int offset) {
    return *reinterpret_cast<T*>(static_cast<char*>(object) + offset);
}

bool InCharacterFamilies(uint32_t family) { return family >= 0x30 && family < 0x3c; }

uint32_t RecordKey(void* record) {
    return (FieldAt<uint32_t>(record, 0x18) & 0xffffff) | (FieldAt<uint32_t>(record, 0x2c) << 24);
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
            if (result && FieldAt<int>(record, 0xb8) != 0) *out = record;
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
            if (FieldAt<int16_t>(student, 0x8c) == 0) Reject(status, out, kReasonNoForce, -1);
            if (status->m_second == static_cast<uint32_t>(mission_status::kOk)) {
                ObjectKey teacherKey;
                ObjectKey studentKey;
                static_cast<GameObjRoot*>(teacher)->GetKey(&teacherKey);
                static_cast<GameObjRoot*>(student)->GetKey(&studentKey);
                if (studentKey.m_value != teacherKey.m_value && FieldAt<int16_t>(teacher, 0x8c) <= FieldAt<int16_t>(student, 0x8c))
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
            if (FieldAt<int>(student, 0xa0) + FieldAt<int>(student, 0xa4) == 0) Reject(status, out, kReasonNoForce, -1);
            if (status->m_second == static_cast<uint32_t>(mission_status::kOk)) {
                const uint32_t studentKey = RecordKey(student);
                const uint32_t teacherKey = RecordKey(teacher);
                if (studentKey != teacherKey &&
                    FieldAt<int>(teacher, 0xa0) + FieldAt<int>(teacher, 0xa4) <= FieldAt<int>(student, 0xa0))
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
    out->m_unk00 = reinterpret_cast<void*>(FUN_00592aa0);
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

int MissionJediTraining::CheckTarget(MissionOrderCheck* orderCheck, StatusCode* status, int* out) {
    TrainingOrderCheck* check = reinterpret_cast<TrainingOrderCheck*>(orderCheck);
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    int result = FUN_005868c0(orderCheck, status, out);
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
        return FUN_00586b00(0, &g_missionJediTrainingEventKinds);
    }
    return 1;
}

void ConstructMissionJediTrainingEventKinds() { new (&g_missionJediTrainingEventKinds) EventKindSet(); }

void RegisterMissionJediTrainingEventKindsAtExit() { atexit(LAB_00571630); }

void MissionJediTraining::InitStatics() {
    ConstructMissionJediTrainingEventKinds();
    RegisterMissionJediTrainingEventKindsAtExit();
}

MissionJediTrainingIterator::MissionJediTrainingIterator(GameObjNotifier* parent)
    : ChildIterator(parent, &kJediTrainingFamilies, 1) {}

MissionJediTrainingIterator::~MissionJediTrainingIterator() { REB_KEEP_VPTR_STORE(); }
