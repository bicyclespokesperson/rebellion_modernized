#include "tactical/tactical_simulator.h"

#include "tactical/tactical_externals.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_ship.h"

namespace {

// The lists are objects with their own vtable; the call must stay virtual even though the list is a member.
__attribute__((noinline)) SimUnitNode* firstNode(SimUnitList* list) { return list->First(); }

constexpr float kStalemateSeconds = 1200.0f;
constexpr float kDeathStarWins = -1.0f;

TacticalDeathStar* asDeathStar(TacticalUnit* unit) { return static_cast<TacticalDeathStar*>(unit); }

int deathStarHasFlag(TacticalUnit* deathStar) { return asDeathStar(deathStar)->m_fired; }

// Destroys every unit in the list (RetireUnit retires one unit; the successor is read first because the list changes).
void retireAll(HeadlessSimulator* simulator, SimUnitList* list) {
    SimUnitNode* node = firstNode(list);
    while (node) {
        SimUnitNode* next = node->Next();
        simulator->RetireUnit(reinterpret_cast<TacticalShipBase*>(node->Value()));
        node = next;
    }
}

void retireSide(HeadlessSimulator* simulator, SimulatorSide* side) {
    retireAll(simulator, &side->m_fleet);
    retireAll(simulator, &side->m_hangars);
}

bool anyUnitActive(SimUnitList* list, bool found) {
    for (SimUnitNode* node = firstNode(list); node; node = node->Next()) {
        if (found) return found;
        if (node->Value()->m_state == kUnitActive) found = true;
    }
    return found;
}

bool sideHasActiveUnit(SimulatorSide* side) {
    bool found = anyUnitActive(&side->m_fleet, false);
    if (found) return true;
    return anyUnitActive(&side->m_hangars, false);
}

}  // namespace

int HeadlessSimulator::Step(float step) {
    m_elapsed = step + m_elapsed;
    m_sides[0]->Refresh(0);
    m_sides[1]->Refresh(0);
    const int resultB = m_sides[1]->StepSide(step);
    const int resultA = m_sides[0]->StepSide(step);
    m_sides[0]->Refresh(0);
    m_sides[1]->Refresh(0);

    const float strengthA = m_sides[0]->m_strength;
    const float strengthB = m_sides[1]->m_strength;
    if (strengthA < m_bestStrength[0]) {
        m_bestStrength[0] = strengthA;
        m_stagnantTime[0] = 0;
    } else {
        m_stagnantTime[0] = step + m_stagnantTime[0];
    }
    if (strengthB < m_bestStrength[1]) {
        m_stagnantTime[1] = 0;
        m_bestStrength[1] = strengthB;
    } else {
        m_stagnantTime[1] = step + m_stagnantTime[1];
    }

    if (resultA == 0 && resultB == 0) {
        const float stagnant = m_stagnantTime[1] <= m_stagnantTime[0] ? m_stagnantTime[1] : m_stagnantTime[0];
        if (stagnant < kStalemateSeconds) return 0;
        float scoreA = m_sides[0]->m_strength;
        const float scoreB = m_sides[1]->m_strength;
        TacticalUnit* deathStar = FUN_005a66a0(GetTacticalRegistry());
        if (deathStar && asDeathStar(deathStar)->IsOperational()) scoreA = kDeathStarWins;
        if (!(scoreA < scoreB)) {
            if (m_sides[1]->HasNoShipsAtAll()) {
                m_sides[1]->m_unkB0 = 1;
            } else {
                retireSide(this, m_sides[1]);
            }
        }
        if (scoreA <= scoreB) {
            if (m_sides[0]->HasNoShipsAtAll()) {
                m_sides[0]->m_unkB0 = 1;
                return 0;
            }
            retireSide(this, m_sides[0]);
        }
        return 0;
    }

    m_outcome = BattleOutcome();
    int sideHasDeathStar[2] = {0, 0};
    if (m_sides[0]->m_unk84 == kUnitWithdrawing) m_outcome.m_sideAFinal = 1;
    if (m_sides[1]->m_unk84 == kUnitWithdrawing) m_outcome.m_sideBFinal = 1;
    TacticalUnit* deathStar = FUN_005a66a0(GetTacticalRegistry());
    if (deathStar) {
        if (deathStarHasFlag(FUN_005a66a0(GetTacticalRegistry())) != 0) sideHasDeathStar[1] = 1;
    }
    for (SimUnitNode* node = firstNode(&m_units); node; node = node->Next()) {
        SimUnit* unit = node->Value();
        if (unit->m_state == kUnitDeathStarPresent) sideHasDeathStar[unit->m_side] = 1;
    }
    m_outcome.m_sideAFinal &= sideHasDeathStar[0];
    m_outcome.m_sideBFinal &= sideHasDeathStar[1];

    deathStar = FUN_005a66a0(GetTacticalRegistry());
    const bool deathStarDecides = deathStar && asDeathStar(FUN_005a66a0(GetTacticalRegistry()))->IsOperational();
    if (!deathStarDecides) {
        if (!sideHasActiveUnit(m_sides[1]) && m_outcome.m_sideAFinal == 0) m_outcome.m_sideAAlive = 1;
    }
    if (!sideHasActiveUnit(m_sides[0]) && m_outcome.m_sideBFinal == 0) m_outcome.m_sideBAlive = 1;
    if (m_outcome.m_sideBAlive != 0 && m_outcome.m_sideAAlive != 0) {
        m_outcome.m_sideBAlive = 0;
        m_outcome.m_sideAAlive = 0;
    }
    RecordSurvivors();
    return 1;
}

void HeadlessSimulator::RecordSurvivors() {
    for (SimUnitNode* outer = firstNode(&m_units); outer; outer = outer->Next()) {
        SimUnit* unit = outer->Value();
        if (unit->GetUnitClass() != 1) continue;
        const int state = unit->m_state;
        if (state != kUnitActive && state != kUnitDockedOrActive && state != kUnitWithdrawing &&
            state != kUnitDeathStarPresent) {
            continue;
        }
        bool covered = false;
        for (SimUnitNode* inner = firstNode(&m_units); inner; inner = inner->Next()) {
            if (covered) break;
            SimUnit* candidate = inner->Value();
            if (candidate->GetUnitClass() != 0) continue;
            if (candidate->m_side != unit->m_side) continue;
            if (candidate->m_state != kUnitActive && candidate->m_state != kUnitDeathStarPresent) continue;
            if (static_cast<TacticalCapitalShip*>(reinterpret_cast<TacticalShipBase*>(candidate))->Dock(reinterpret_cast<TacticalShipBase*>(unit)) == 1) covered = true;
        }
        if (covered) continue;
        TacticalUnit* deathStar = FUN_005a66a0(GetTacticalRegistry());
        if (!deathStar) continue;
        if (!asDeathStar(deathStar)->IsOperational() && deathStarHasFlag(deathStar) == 0) continue;
        if (unit->m_side != 1) continue;
        static_cast<TacticalHangar*>(asDeathStar(deathStar))->Dock(reinterpret_cast<TacticalShipBase*>(unit));
    }
}

// ---- units registered with the simulator ---------------------------------------------------------------------------

int HeadlessSimulator::RandomBetween(int low, int high) { return m_random.Next(high - low) + low; }

int HeadlessSimulator::AddCharacter(TacticalUnit* character) {
    TacticalCharacterUnit* unit = static_cast<TacticalCharacterUnit*>(character);
    m_characters.AddWithId(unit);
    if (unit->m_isFirstCommander != 0) {
        m_firstCommander[unit->m_notAlliance] = unit->m_id;
        return unit->m_id;
    }
    if (unit->m_isSecondCommander != 0) m_secondCommander[unit->m_notAlliance] = unit->m_id;
    return unit->m_id;
}

int HeadlessSimulator::AddSpecialForce(TacticalUnit* force) {
    m_specialForces.AddWithId(force);
    return force->m_id;
}

int HeadlessSimulator::AddLaunchedFighter(TacticalUnit* fighter) {
    SimUnit* unit = reinterpret_cast<SimUnit*>(fighter);
    m_units.Add(reinterpret_cast<TacticalShipBase*>(unit));
    m_launched.Add(reinterpret_cast<TacticalShipBase*>(unit));
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(fighter) + 0xec);
}
