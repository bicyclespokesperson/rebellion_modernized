#include "gameobj/object_queries.h"
#include "mission/mission.h"

namespace {

}  // namespace

// Advances *key to the key after it in `set` (the first if unset); unset at the end (0x525b00).
void NextMemberKey(ObjectKeyValueSet* set, ObjectKey* key) {
    const ObjectKey previous = *key;
    key->Reset();
    AvlNode* node = nullptr;
    if (!previous.IsSet()) {
        node = set->First();
    } else {
        ObjectKeyNode* found = set->Find(&previous);
        if (!found) return;
        node = found->m_successor;
    }
    if (node) static_cast<ObjectKeyNode*>(node)->GetKey(key);
}

MissionMemberIterator::MissionMemberIterator(Mission* mission, const FamilyRange* range, int team, int decoys, int captured)
    : m_mission(mission),
      m_range(*range),
      m_wantTeam(team),
      m_wantDecoys(decoys),
      m_wantCaptured(captured),
      m_inTeam(0),
      m_inDecoys(0),
      m_inCaptured(0),
      m_current(nullptr) {}

MissionMemberIterator::~MissionMemberIterator() {  }

void MissionMemberIterator::StepKey() {
    if (m_inTeam) {
        if (m_wantTeam) NextMemberKey(&m_mission->m_team, &m_currentKey);
        m_inTeam = m_currentKey.IsSet();
        m_inDecoys = m_inTeam == 0;
    }
    if (m_inDecoys) {
        if (m_wantDecoys) NextMemberKey(&m_mission->m_decoys, &m_currentKey);
        m_inDecoys = m_currentKey.IsSet();
        m_inCaptured = m_inDecoys == 0;
    }
    if (m_inCaptured) {
        if (m_wantCaptured) NextMemberKey(&m_mission->m_captured, &m_currentKey);
        m_inCaptured = m_currentKey.IsSet();
    }
}

void MissionMemberIterator::ResolveCurrent() {
    m_current = m_currentKey.IsSet() ? static_cast<GameObj*>(GetMasterInFamilies_30_40(&m_currentKey)) : nullptr;
}

int MissionMemberIterator::Accepts() {
    int accepted = 1;
    if (m_current) {
        const uint32_t family = m_current->GetFamily();
        if (family < m_range.m_first || m_range.m_second <= family) accepted = 0;
    }
    return accepted;
}

MissionMemberIterator* MissionMemberIterator::Advance() {
    int accepted;
    do {
        StepKey();
        ResolveCurrent();
        accepted = m_currentKey.IsSet() ? Accepts() : 1;
    } while (!accepted);
    return this;
}

MissionMemberIterator* MissionMemberIterator::Begin() {
    m_inTeam = 0;
    m_inDecoys = 0;
    m_inCaptured = 0;
    m_currentKey.Reset();
    m_current = nullptr;
    if (m_wantTeam) {
        m_inTeam = 1;
    } else if (m_wantDecoys) {
        m_inDecoys = 1;
    } else if (m_wantCaptured) {
        m_inCaptured = 1;
    }
    StepKey();
    ResolveCurrent();
    if (!Accepts()) Advance();
    return this;
}

int MissionMemberIterator::Count() {
    int count = 0;
    Begin();
    while (m_current) {
        ++count;
        Advance();
    }
    return count;
}

int MissionMemberIterator::HasAny() {
    Begin();
    return m_current != nullptr;
}

#define REB_MISSION_ITERATOR_IMPL(Name, first, last, team, decoys, captured)                       \
    Name::Name(Mission* mission) : MissionMemberIterator(mission, &kRange##Name, team, decoys, captured) {} \
    Name::~Name() {  }

namespace {
constexpr FamilyRange kRangeMissionPersonIterator(0x30, 0x40);
constexpr FamilyRange kRangeMissionPersonTeamIterator(0x30, 0x40);
constexpr FamilyRange kRangeMissionPersonCapturedIterator(0x30, 0x40);
constexpr FamilyRange kRangeMissionSpecialForcesIterator(0x3c, 0x40);
constexpr FamilyRange kRangeMissionSpecialForcesTeamIterator(0x3c, 0x40);
constexpr FamilyRange kRangeMissionSpecialForcesDecoyIterator(0x3c, 0x40);
constexpr FamilyRange kRangeMissionCharacterIterator(0x30, 0x3c);
constexpr FamilyRange kRangeMissionCharacterTeamIterator(0x30, 0x3c);
constexpr FamilyRange kRangeMissionCharacterDecoyIterator(0x30, 0x3c);
constexpr FamilyRange kRangeMissionMinorTeamIterator(0x38, 0x3c);
constexpr FamilyRange kRangeMissionMinorDecoyIterator(0x38, 0x3c);
constexpr FamilyRange kRangeMissionMajorTeamIterator(0x30, 0x38);
constexpr FamilyRange kRangeMissionMajorDecoyIterator(0x30, 0x38);
}  // namespace

REB_MISSION_ITERATOR_IMPL(MissionPersonIterator, 0x30, 0x40, 1, 1, 1)
REB_MISSION_ITERATOR_IMPL(MissionPersonTeamIterator, 0x30, 0x40, 1, 0, 0)
REB_MISSION_ITERATOR_IMPL(MissionPersonCapturedIterator, 0x30, 0x40, 0, 0, 1)
REB_MISSION_ITERATOR_IMPL(MissionSpecialForcesIterator, 0x3c, 0x40, 1, 1, 1)
REB_MISSION_ITERATOR_IMPL(MissionSpecialForcesTeamIterator, 0x3c, 0x40, 1, 0, 0)
REB_MISSION_ITERATOR_IMPL(MissionSpecialForcesDecoyIterator, 0x3c, 0x40, 0, 1, 0)
REB_MISSION_ITERATOR_IMPL(MissionCharacterIterator, 0x30, 0x3c, 1, 1, 1)
REB_MISSION_ITERATOR_IMPL(MissionCharacterTeamIterator, 0x30, 0x3c, 1, 0, 0)
REB_MISSION_ITERATOR_IMPL(MissionCharacterDecoyIterator, 0x30, 0x3c, 0, 1, 0)
REB_MISSION_ITERATOR_IMPL(MissionMinorTeamIterator, 0x38, 0x3c, 1, 0, 0)
REB_MISSION_ITERATOR_IMPL(MissionMinorDecoyIterator, 0x38, 0x3c, 0, 1, 0)
REB_MISSION_ITERATOR_IMPL(MissionMajorTeamIterator, 0x30, 0x38, 1, 0, 0)
REB_MISSION_ITERATOR_IMPL(MissionMajorDecoyIterator, 0x30, 0x38, 0, 1, 0)
