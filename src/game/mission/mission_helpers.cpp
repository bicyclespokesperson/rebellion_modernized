#include <new>

#include "character/character.h"
#include "character/special_forces.h"
#include "core/utility_classes.h"
#include "data/dat_record.h"
#include "economy/manu_mgr.h"
#include "gameobj/object_queries.h"
#include "gameobj/object_type_db.h"
#include "gameobj/status_code.h"
#include "mission/mission.h"
#include "mission/mission_external.h"


namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusMissionCategory = 0x40;
constexpr uint32_t kSideControlBits = 0xc0;
constexpr uint32_t kAllianceControl = 0x40;
constexpr uint32_t kEmpireControl = 0x80;

bool isPersonFamily(uint32_t family) { return family >= 0x30 && family < 0x40; }

ObjectKey recordKey(DatRecord* record) {
    ObjectKey key;
    key.m_value = (record->m_third << 24) | (record->m_id & 0xffffff);
    return key;
}

}  // namespace

void MissionMemberSummary::CopyAllButFirst(const MissionMemberSummary& other) {
    if (this != &other) {
        m_characterAbilityMask = other.m_characterAbilityMask;
        m_hasAlliance = other.m_hasAlliance;
        m_hasEmpire = other.m_hasEmpire;
        m_kind = other.m_kind;
    }
}

// ---- EventKindNode ----------------------------------------------------------------------------------------

EventKindNode::EventKindNode() : m_value(1), m_unknown(0) {}

EventKindNode::EventKindNode(const EventKindNode& other)
    : IntKeyNode(other), m_value(other.m_value), m_unknown(other.m_unknown) {}

EventKindNode::~EventKindNode() {  }

CloneableNode* EventKindNode::Clone() {
    void* memory = ::operator new(sizeof(EventKindNode));
    return memory ? new (memory) EventKindNode(*this) : nullptr;
}

int EventKindNode::Read(istream* in) {
    const int result = IntKeyNode::Read(in);
    if (result) {
        ReadValue32(in, &m_value);
        ReadValue32(in, &m_unknown);
    }
    return result;
}

int EventKindNode::Write(ostream* out) {
    const int result = IntKeyNode::Write(out);
    if (result) {
        WriteValue32(out, &m_value);
        WriteValue32(out, &m_unknown);
    }
    return result;
}

// ---- ManuMgrData (the class belongs to the economy module, its methods sit in this address range) ---------

ManuMgrData::ManuMgrData() {}

ManuMgrData::~ManuMgrData() {  }

int ManuMgrData::Read(istream* in) {
    const int result = GameObjData::Read(in);
    if (result) m_queue.Read(in);
    return result;
}

int ManuMgrData::Write(ostream* out) {
    const int result = GameObjData::Write(out);
    if (result) m_queue.Write(out);
    return result;
}

int ManuMgrData::Checksum(Crc32* crc) {
    int result = GameObjData::Checksum(crc);
    if (result) result = m_queue.Checksum(crc);
    return result;
}

// ---- MissionPhaseLevels -----------------------------------------------------------------------------------

MissionPhaseLevels::MissionPhaseLevels() : m_levels(0) {}

MissionPhaseLevels::~MissionPhaseLevels() {  }

MissionPhaseLevels& MissionPhaseLevels::operator=(const MissionPhaseLevels& other) {
    if (this != &other) m_levels = other.m_levels;
    return *this;
}

int MissionPhaseLevels::Read(istream* in) {
    ReadValue32(in, &m_levels);
    return 1;
}

int MissionPhaseLevels::Write(ostream* out) {
    WriteValue32(out, &m_levels);
    return 1;
}

int MissionPhaseLevels::Checksum(Crc32* crc) {
    crc->AddInt(m_levels & 0xf);
    crc->AddInt((m_levels >> 4) & 0xf);
    crc->AddInt((m_levels >> 8) & 0xf);
    crc->AddInt((m_levels >> 12) & 0xf);
    return 1;
}

// ---- MissionData ------------------------------------------------------------------------------------------

MissionData::MissionData() : m_unk18(1), m_unk1c(0) {}

MissionData::~MissionData() {  }

int MissionData::Read(istream* in) {
    int result = GameObjData::Read(in);
    if (result) {
        ReadValue32(in, &m_unk18);
        ReadValue32(in, &m_unk1c);
        result = m_statA.Read(in);
        if (result) {
            result = m_statB.Read(in);
            if (result) result = m_levels.Read(in);
        }
    }
    return result;
}

int MissionData::Write(ostream* out) {
    int result = GameObjData::Write(out);
    if (result) {
        WriteValue32(out, &m_unk18);
        WriteValue32(out, &m_unk1c);
        result = m_statA.Write(out);
        if (result) {
            result = m_statB.Write(out);
            if (result) result = m_levels.Write(out);
        }
    }
    return result;
}

int MissionData::Checksum(Crc32* crc) {
    int result = GameObjData::Checksum(crc);
    if (result) {
        crc->AddInt(m_unk18);
        crc->AddInt(m_unk1c);
        result = m_statA.Checksum(crc);
        if (result) {
            result = m_statB.Checksum(crc);
            if (result) result = m_levels.Checksum(crc);
        }
    }
    return result;
}

GameObjData* Mission::CreateData() {
    void* memory = ::operator new(sizeof(MissionData));
    return memory ? new (memory) MissionData() : nullptr;
}

// ---- MissionCheckArgs -------------------------------------------------------------------------------------

MissionCheckArgs::MissionCheckArgs(int a04, int a08, int a0c, int controlSide, DatRecord* record, void* refs1, void* refs2,
                                   void* refs3, ObjectKeyValueSet* team, ObjectKeyValueSet* decoys,
                                   ObjectKeyValueSet* captured, MissionMemberSummary* summary, GameObj* target,
                                   GameObj* objective)
    : m_flag04(a04),
      m_flag08(a08),
      m_flag0c(a0c),
      m_controlSide(controlSide),
      m_record(record),
      m_refs{refs1, refs2, refs3},
      m_team(team),
      m_decoys(decoys),
      m_captured(captured),
      m_summary(summary),
      m_target(target),
      m_objective(objective) {}

MissionCheckArgs::~MissionCheckArgs() {  }

// ---- person resolution ------------------------------------------------------------------------------------

int ResolveRoleFromKey(ObjectKey* key, GameObj** out) {
    *out = nullptr;
    GameObjNotifier* object = GetMasterInFamilies_01_ff(key);
    int found = 0;
    if (object) {
        found = isPersonFamily(object->GetFamily());
        if (found) *out = static_cast<GameObj*>(object);
    }
    return found;
}

int SplitRoleFromKey(ObjectKey* key, GameObj** person, GameObj** specialForces, GameObj** character) {
    *person = nullptr;
    *specialForces = nullptr;
    *character = nullptr;
    int found = ResolveRoleFromKey(key, person);
    if (found) {
        const uint32_t family = (*person)->GetFamily();
        if (family >= 0x3c && family < 0x40) {
            *specialForces = *person;
        } else if (family >= 0x30 && family < 0x3c) {
            *character = *person;
        } else {
            found = 0;
        }
    }
    return found;
}

int ResolvePersonRecord(ObjectKey* key, DatRecord** out) {
    *out = nullptr;
    DatRecord* record = FindDataRecord(AsLookupKey(key));
    int found = 0;
    if (record) {
        found = isPersonFamily(recordKey(record).Family());
        if (found) *out = record;
    }
    return found;
}

int SplitPersonRecord(ObjectKey* key, DatRecord** person, DatRecord** specialForces, DatRecord** character) {
    *person = nullptr;
    *specialForces = nullptr;
    *character = nullptr;
    int found = ResolvePersonRecord(key, person);
    if (found) {
        const uint32_t family = recordKey(*person).Family();
        if (family >= 0x3c && family < 0x40) {
            *specialForces = *person;
        } else if (family >= 0x30 && family < 0x3c) {
            *character = *person;
        } else {
            found = 0;
        }
    }
    return found;
}

// ---- member summaries -------------------------------------------------------------------------------------

int AddPersonToMemberSummary(GameObj* person, int a, int b, MissionMemberSummary* summary, StatusCode* status) {
    int result = 1;
    *status = StatusCode(kStatusCategory, kStatusOk);
    const uint32_t control = person->m_flags & kSideControlBits;
    if (control == kAllianceControl) {
        summary->m_hasAlliance = 1;
    } else if (control == kEmpireControl) {
        summary->m_hasEmpire = 1;
    } else {
        result = 0;
    }
    if (result) {
        const uint32_t family = person->GetFamily();
        if (family >= 0x3c && family < 0x40) {
            summary->m_specialForceMissionBits |= static_cast<SpecialForces*>(person)->GetRecordMissionBits();
        } else {
            summary->m_characterAbilityMask |= GetCharacterMemberMask();
        }
    }
    return result;
}

namespace {

int addKeysToSummary(ObjectKeyValueSet* set, int a, MissionMemberSummary* summary, StatusCode* status, int ok) {
    for (AvlNode* node = set->First(); node && ok && status->m_second == static_cast<uint32_t>(kStatusOk);
         node = node->m_successor) {
        ObjectKey key;
        static_cast<ObjectKeyNode*>(node)->GetKey(&key);
        GameObj* person = nullptr;
        ok = 0;
        if (ResolveRoleFromKey(&key, &person)) ok = AddPersonToMemberSummary(person, a, 0, summary, status);
    }
    return ok;
}

}  // namespace

int FillMemberSummaryForMission(ObjectKeyValueSet* team, ObjectKeyValueSet* decoys, ObjectKeyValueSet* captured,
                                        MissionMemberSummary* summary, StatusCode* status) {
    summary->CopyAllButFirst(MissionMemberSummary{});
    *status = StatusCode(kStatusCategory, kStatusOk);
    int ok = addKeysToSummary(team, 0, summary, status, 1);
    ok = addKeysToSummary(decoys, 1, summary, status, ok);
    for (AvlNode* node = captured->First(); node; node = node->m_successor) {
        if (!ok) return 0;
        if (status->m_second != static_cast<uint32_t>(kStatusOk)) break;
        ObjectKey key;
        static_cast<ObjectKeyNode*>(node)->GetKey(&key);
        GameObj* person = nullptr;
        ok = 0;
        if (ResolveRoleFromKey(&key, &person)) ok = AddPersonToMemberSummary(person, 1, 0, summary, status);
    }
    if (!ok) return 0;
    if (status->m_second != static_cast<uint32_t>(kStatusOk)) return ok;
    const uint32_t alliance = summary->m_hasAlliance;
    const uint32_t empire = summary->m_hasEmpire;
    if (alliance && empire) {
        *status = StatusCode(kStatusCategory, 0x14);
    } else if (!alliance && !empire) {
        *status = StatusCode(kStatusCategory, 0x16);
    } else {
        summary->m_kind = alliance ? 1 : 2;
    }
    return ok;
}

int CheckSummaryAgainstRecord(DatRecord* record, MissionMemberSummary* summary, StatusCode* status) {
    auto* mission = static_cast<MissionSD*>(record);
    *status = StatusCode(kStatusCategory, kStatusOk);
    if (summary->m_specialForceMissionBits != (mission->m_specialForceEligibility & summary->m_specialForceMissionBits) ||
        summary->m_characterAbilityMask != (mission->m_targetFlags & summary->m_characterAbilityMask)) {
        *status = StatusCode(kStatusMissionCategory, 1);
    }
    if (status->m_second != static_cast<uint32_t>(kStatusOk)) return 1;
    const uint32_t alliance = summary->m_hasAlliance;
    const uint32_t empire = summary->m_hasEmpire;
    if (alliance) {
        if (empire) {
            *status = StatusCode(kStatusCategory, 0x14);
            return 1;
        }
        if (mission->m_isAlliance == 0) {
            *status = StatusCode(kStatusMissionCategory, 1);
            return 1;
        }
    } else if (!empire) {
        *status = StatusCode(kStatusCategory, 0x16);
        return 1;
    }
    if (empire == 0) return 1;
    if (mission->m_isEmpire != 0) return 1;
    *status = StatusCode(kStatusMissionCategory, 1);
    return 1;
}
