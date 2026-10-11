#include "ai/ai_planner_util.h"
#include "ai/ai_planners.h"

using namespace ai_planner;
using ai::IsSectorKey;
using ai::IsSystemKey;

namespace {

constexpr uint32_t kHasLeaders = 1;
constexpr uint32_t kTargetChosen = 0x10;

// BUG(original): the last test is `(flags | 0x20) == 0`, which is never true, so a rescue target is never kept.
bool RescueKeepsTarget(uint32_t flags) {
    return (flags & 0x20000000) != 0 && (flags & 2) != 0 && (flags & 0x40) != 0 && (flags | 0x20) == 0;
}

bool Mission62KeepsTarget(uint32_t flags) { return (flags & 8) != 0 && (flags & 4) != 0 && (flags & 0x60) == 0; }

const CharacterHunt kRescueHunt = {0x4000, 0x44, 0x1d, 0x4000, 0x8000, 0x1000000, 0x40, 2, RescueKeepsTarget};
const CharacterHunt kMission62Hunt = {0x100, 0x41, 0x2f, 0x1000, 0x2000, 0x200000, 8, 4, Mission62KeepsTarget};

void ClearSectorBusy(AiSideController* owner, const ObjectKey& sectorKey, uint32_t flag) {
    AiSectorEval* sector = owner->m_sectors.FindEval(sectorKey);
    if (sector) sector->m_flags28.m_value &= ~flag;
}

}  // namespace

// ---- 0x61 Rescue ------------------------------------------------------------------------------------------------------------

AiRescuePlanner::AiRescuePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiFreePlanner(view, owner, characters) {
    m_missionKey = ObjectKey::Make(0x61, 0x11);
}

AiRescuePlanner::~AiRescuePlanner() {  }

int AiRescuePlanner::Virtual06() { return SumLeaderValue(this, 5); }

void AiRescuePlanner::Prepare() { ChooseLeader(this, 2, 5, 0x33, 0x4f400051, 0x7000000, 2); }

void AiRescuePlanner::Virtual19() { ChooseMembers(this, Virtual17(), 2); }

int AiRescuePlanner::ChooseTarget() { return ChooseCharacterTarget(this, kRescueHunt); }

void AiRescuePlanner::PickCandidate() { ChooseCandidateTarget(this, kRescueHunt); }

// ---- 0x62 free planner ------------------------------------------------------------------------------------------------------

AiMission62Planner::AiMission62Planner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiFreePlanner(view, owner, characters) {
    m_missionKey = ObjectKey::Make(0x62, 0x17);
}

AiMission62Planner::~AiMission62Planner() {  }

int AiMission62Planner::Virtual06() { return SumLeaderValue(this, 5); }

void AiMission62Planner::Prepare() { ChooseLeader(this, 0x10, 5, 0x33, 0xf400051, 0x7000000, 2); }

void AiMission62Planner::Virtual19() { ChooseMembers(this, Virtual17(), 0x10); }

int AiMission62Planner::ChooseTarget() { return ChooseCharacterTarget(this, kMission62Hunt); }

void AiMission62Planner::PickCandidate() { ChooseCandidateTarget(this, kMission62Hunt); }

// ---- 0x51 Diplomacy ---------------------------------------------------------------------------------------------------------

AiDiplomacyPlanner::AiDiplomacyPlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiFreePlanner(view, owner, characters) {
    m_missionKey = ObjectKey::Make(0x51, 0x10);
}

AiDiplomacyPlanner::~AiDiplomacyPlanner() {  }

int AiDiplomacyPlanner::Virtual06() { return SumLeaderValue(this, 0); }

int AiDiplomacyPlanner::Virtual13() { return NoteTargetSystem(this, 0x40); }

void AiDiplomacyPlanner::OnFinished() {
    AiSystemEval* system = m_owner->FindSystemEval(&m_target);
    if (system) system->m_flags2c.m_value &= ~0x20000000u;
}

int AiDiplomacyPlanner::CanUse(const ObjectKey* character) {
    ObjectKey scratch;
    m_selector.Add(m_characters->RankEligible(0, 2, 1, 0x7404051, 0, 0, 0x45, 10000, 1));
    const ObjectKey best = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    int usable = 1;
    AiCharacterRecord* top = static_cast<AiCharacterRecord*>(m_owner->m_characters.Find(&best));
    if (top) {
        AiCharacterRecord* candidate = static_cast<AiCharacterRecord*>(m_owner->m_characters.Find(character));
        if (candidate && candidate->m_values[0] < top->m_values[0]) usable = 0;
    }
    return usable;
}

void AiDiplomacyPlanner::Prepare() { ChooseLeader(this, 1, 0, 0x45, 0x7404051); }

int AiDiplomacyPlanner::ChooseTarget() {
    ObjectKey sector;
    ObjectKey scratch;
    int result = 0;
    m_target.Reset();
    m_flags.m_value &= ~kTargetChosen;
    if ((m_flags.m_value & kHasLeaders) == 0) {
        if (IsSystemKey(m_x34)) {
            AiSystemEval* system = m_owner->m_systems.FindEval(m_x34);
            if (system) system->m_flags2c.m_value &= ~0x20000000u;
        }
        m_x34.Reset();
        return (m_flags.m_value & kTargetChosen) != 0;
    }
    if (IsSystemKey(m_x34)) {
        AiSystemEval* system = m_owner->m_systems.FindEval(m_x34);
        if (system) {
            if ((system->m_flags2c.m_value & 0x40) != 0 && (system->m_flags30.m_value & 0x200) == 0) {
                m_target = m_x34;
                m_flags.m_value |= kTargetChosen;
                system->m_flags2c.m_value |= 0x20000000;
            }
            if ((m_flags.m_value & kTargetChosen) == 0) system->m_flags2c.m_value &= ~0x20000000u;
        }
    }
    if ((m_flags.m_value & kTargetChosen) != 0) return 1;

    m_x34.Reset();
    if (!IsSystemKey(m_x34)) {
        m_selector.Add(m_owner->RankSectors(0, 0x4000020, 4, 0, 0, 0, 7, 2));
        sector = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        if (IsSectorKey(sector)) {
            AiSectorEval* sectorEval = m_owner->m_sectors.FindEval(sector);
            if (sectorEval && sectorEval->Stat(0x8c) < 0x46) {
                m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&sector, 0, 0x40, 1, 0, 0x20000000, 0, 2));
                m_x34 = *m_selector.PickBest(&scratch);
                m_selector.Reset();
            }
        }
    }
    if (!IsSystemKey(m_x34) && IsSystemKey(m_destination)) {
        m_selector.Add(m_owner->RankSectorsByDistance(&m_destination, 0, 0x4000020, 0x10, 0, 0, 0, 2));
        sector = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&sector, 0, 0x40, 2, 0, 0x20000000, 0, 2));
        m_x34 = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    }
    if (!IsSystemKey(m_x34) && IsSystemKey(m_destination)) {
        m_selector.Add(m_owner->RankSystemsOfSectorByDistance(&sector, &m_destination, 0, 0x40, 0, 0, 0x20000000, 0, 2));
        m_x34 = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    }
    if (!IsSystemKey(m_x34)) {
        m_selector.Add(m_owner->RankSectors(0, 0x4000020, 0x10, 0, 0, 0, 3, 1));
        m_selector.Add(m_owner->RankSectors(0, 0x4000020, 0x10, 0, 0, 0, 5, 1));
        m_selector.Add(m_owner->RankSectorsRandomly(0, 0x4000020, 0x10, 0, 0, 0, 1));
        sector = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&sector, 0, 0x40, 2, 0, 0x20000000, 0, 1));
        m_x34 = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    }
    if (!IsSystemKey(m_x34)) {
        m_selector.Add(m_owner->RankSectors(0, 0x4000020, 0, 0, 0, 0, 3, 1));
        m_selector.Add(m_owner->RankSectorsRandomly(0, 0x20, 0, 0, 0, 0, 1));
        sector = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&sector, 0, 0x40, 0, 0, 0x20000000, 0, 2));
        m_x34 = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    }
    if (IsSystemKey(m_x34)) {
        m_target = m_x34;
        m_flags.m_value |= kTargetChosen;
        AiSystemEval* system = m_owner->m_systems.FindEval(m_target);
        if (system) system->m_flags2c.m_value |= 0x20000000;
    }
    if ((m_flags.m_value & kTargetChosen) != 0) result = 1;
    return result;
}

// ---- 0x54 free planner, subtype 5 -------------------------------------------------------------------------------------------

AiMission54State5Planner::AiMission54State5Planner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiFreePlanner(view, owner, characters), m_sectorKey() {
    m_missionKey = ObjectKey::Make(0x54, 0x15);
}

AiMission54State5Planner::~AiMission54State5Planner() {  }

void AiMission54State5Planner::Read(istream* in) {
    AiFreePlanner::Read(in);
    m_sectorKey.Read(in);
}

void AiMission54State5Planner::Write(ostream* out) {
    AiFreePlanner::Write(out);
    m_sectorKey.Write(out);
}

int AiMission54State5Planner::Virtual06() { return SumLeaderValue(this, 1); }

void AiMission54State5Planner::Virtual12(int event) {
    if (event == 0) {
        m_flags.m_value |= 0x80000000;
    } else if (event == 2) {
        m_x34.Reset();
        ResetMissionMark();
    } else if (event == 3) {
        AiSystemEval* system = m_owner->FindSystemEval(&m_x34);
        if (system) system->NoteTouched();
        m_x34.Reset();
        m_stall = 0;
        ResetMissionMark();
    }
}

void AiMission54State5Planner::OnFinished() { ClearSectorBusy(m_owner, m_sectorKey, 0x10000000); }

void AiMission54State5Planner::Prepare() {
    ChooseSystemLeaders(this, SystemLeaders{0x800002, 0x200, 0x7400051, 1, 0, -1, 1});
}

int AiMission54State5Planner::ChooseTarget() {
    ObjectKey scratch;
    int result = 0;
    m_target.Reset();
    m_flags.m_value &= ~kTargetChosen;
    if (IsSystemKey(m_x34)) {
        AiSystemEval* system = m_owner->m_systems.FindEval(m_x34);
        if (system && (system->m_flags2c.m_value & 0x80) != 0) {
            m_target = m_x34;
            m_flags.m_value |= kTargetChosen;
        }
    }
    if ((m_flags.m_value & kTargetChosen) != 0) return 1;

    ClearSectorBusy(m_owner, m_sectorKey, 0x10000000);
    if (m_owner->Stat(0x1c4) < 1) {
        if (!IsSystemKey(m_destination))
            m_selector.Add(m_owner->RankSectorsRandomly(0, 0x40, 0, 0, 0x10000000, 0x4000000, 1));
        else
            m_selector.Add(m_owner->RankSectorsByDistance(&m_destination, 0, 0x40, 0, 0, 0x10000000, 0x4000000, 2));
        m_sectorKey = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        m_selector.Add(m_owner->RankSystemsOfSectorByAge(&m_sectorKey, 0, 0x80, 0, 0, 0, 0, 1));
        m_x34 = *m_selector.PickBest(&scratch);
    } else {
        const bool destinationIsSystem = IsSystemKey(m_destination);
        if (!destinationIsSystem)
            m_selector.Add(m_owner->RankSectorsRandomly(0, 0x40, 0, 0, 0x50000000, 0x4000000, 2));
        else
            m_selector.Add(m_owner->RankSectorsByDistance(&m_destination, 0, 0x40, 0, 0, 0x50000000, 0x4000000, 2));
        m_sectorKey = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&m_sectorKey, 0, 0x80, 0, 0, 0x8000000, 0, destinationIsSystem ? 2 : 1));
        m_x34 = *m_selector.PickBest(&scratch);
    }
    m_selector.Reset();
    if (IsSystemKey(m_x34)) {
        AiSectorEval* sector = m_owner->m_sectors.FindEval(m_sectorKey);
        if (sector) {
            sector->m_flags28.m_value |= 0x10000000;
            m_target = m_x34;
            m_flags.m_value |= kTargetChosen;
        }
    }
    if ((m_flags.m_value & kTargetChosen) != 0) result = 1;
    return result;
}

// ---- 0x52 Espionage, subtype 6 ----------------------------------------------------------------------------------------------

AiEspionageState6Planner::AiEspionageState6Planner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiFreePlanner(view, owner, characters), m_sectorKey() {
    m_missionKey = ObjectKey::Make(0x52, 0x13);
}

AiEspionageState6Planner::~AiEspionageState6Planner() {  }

void AiEspionageState6Planner::Read(istream* in) {
    AiFreePlanner::Read(in);
    m_sectorKey.Read(in);
}

void AiEspionageState6Planner::Write(ostream* out) {
    AiFreePlanner::Write(out);
    m_sectorKey.Write(out);
}

int AiEspionageState6Planner::Virtual06() { return SumLeaderValue(this, 1); }

void AiEspionageState6Planner::Virtual12(int event) {
    if (event == 0) {
        m_flags.m_value |= 0x80000000;
    } else if (event == 2) {
        m_x34.Reset();
        m_destination.Reset();
        ResetMissionMark();
    } else if (event == 3) {
        AiSystemEval* system = m_owner->FindSystemEval(&m_x34);
        if (system) system->NoteTouched();
        m_x34.Reset();
        m_destination.Reset();
        m_stall = 0;
        ResetMissionMark();
    }
}

void AiEspionageState6Planner::OnFinished() { ClearSectorBusy(m_owner, m_sectorKey, 0x20000000); }

void AiEspionageState6Planner::Prepare() {
    ChooseSystemLeaders(this, SystemLeaders{0x10800002, 8, 0x7400051, 1, 0x33, -1, 1});
}

int AiEspionageState6Planner::ChooseTarget() {
    ObjectKey scratch;
    int result = 0;
    m_target.Reset();
    m_flags.m_value &= ~kTargetChosen;
    if (IsSystemKey(m_x34)) {
        AiSystemEval* system = m_owner->m_systems.FindEval(m_x34);
        if (system && (system->m_flags30.m_value & 4) != 0 && (system->m_flags2c.m_value & 0x200) != 0) {
            m_target = m_x34;
            m_flags.m_value |= kTargetChosen;
        }
    }
    if ((m_flags.m_value & kTargetChosen) != 0) return 1;

    ClearSectorBusy(m_owner, m_sectorKey, 0x20000000);
    if (!IsSystemKey(m_destination))
        m_selector.Add(m_owner->RankSectorsRandomly(0, 0x400, 8, 0, 0x20000000, 0x4000000, 1));
    else
        m_selector.Add(m_owner->RankSectorsByDistance(&m_destination, 0, 0x400, 8, 0, 0x20000000, 0x4000000, 2));
    m_sectorKey = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSectorByAge(&m_sectorKey, 0, 0x200, 4, 0, 0, 0, 1));
    m_x34 = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (IsSystemKey(m_x34)) {
        AiSectorEval* sector = m_owner->m_sectors.FindEval(m_sectorKey);
        if (sector) {
            sector->m_flags28.m_value |= 0x20000000;
            m_target = m_x34;
            m_flags.m_value |= kTargetChosen;
        }
    }
    if ((m_flags.m_value & kTargetChosen) != 0) result = 1;
    return result;
}
