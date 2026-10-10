#include "tactical/tactical_groups.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_externals.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_ship.h"
#include "tactical/tactical_simulator.h"

// Death Star (0x5ba3a0-0x5ba8b0) and the headless simulator's handling of it.

TacticalDeathStar::TacticalDeathStar(uint32_t family, uint32_t id, int notCompleted, int hasFacility, float hull,
                                     float hullNow, int hangarFree) {
    m_channelA.m_data = nullptr;
    m_channelB.m_data = nullptr;
    m_objectFamily = family;
    m_objectId = id;
    m_hullMax = hull;
    m_hull = hullNow;
    m_notCompleted = notCompleted;
    m_hasFacility = hasFacility;
    m_charge = 100.0f;
    m_inFlight = 0;
    m_aboard = 0;
    m_capacity = hangarFree;
    m_fired = 0;
}

TacticalDeathStar::~TacticalDeathStar() { REB_KEEP_VPTR_STORE(); }

int TacticalDeathStar::IsOperational() {
    if (m_hasFacility != 0) return 0;
    if (m_hull == 0.0f) return 0;
    return m_fired != 1;
}

int TacticalDeathStar::IsReady() {
    if (m_hasFacility != 0) return 0;
    if (m_hull == 0.0f) return 0;
    if (m_notCompleted != 0) return 0;
    return m_fired != 1;
}

DeathStarResult* TacticalDeathStar::BuildResult(DeathStarResult* out) {
    out->m_alive = m_hull > 0.0f ? 1 : 0;
    out->m_hull = m_hull;
    out->m_unk10 = m_fired;
    return out;
}

CapitalShipResult* TacticalDeathStar::BuildCapitalShipResult(CapitalShipResult* out) {
    out->m_unk00 = static_cast<int>(m_id);
    out->m_unk08 = static_cast<int>(m_objectFamily);
    out->m_unk0c = static_cast<int>(m_objectId);
    out->m_hull = m_hull;
    out->m_speedCH = 0;
    out->m_shieldRechargeCH = 0;
    out->m_hyperdriveDamage = 0;
    out->m_tractorBeamCH = 0;
    out->m_weaponRechargeCH = 0;
    out->m_unk28 = m_fired != 0 ? 9 : 2;
    return out;
}

void TacticalDeathStar::ResetCharge(int) { m_charge = 0; }

void TacticalDeathStar::Charge(float milliseconds) {
    if (!(m_hull > 0.0f) || m_hasFacility != 0) return;
    const float step = static_cast<float>((static_cast<double>(milliseconds) * 0.33333334f) * 0.001f);
    if (m_charge < 100.0f && !(static_cast<double>(step) + m_charge < 100.0f)) {
        if (GetObjectManager()->m_playerSide == 1) FUN_005ba980(FUN_005ba8b0(), 0x113, 1);
    }
    const double charge = static_cast<double>(step) + m_charge;
    m_charge = static_cast<float>(charge < 100.0f ? charge : 100.0f);
}

void TacticalDeathStar::Destroy() {
    m_charge = 0;
    m_hull = 0;
    TacticalHangar::Disband();
}

void TacticalDeathStar::ResetCharged() {
    m_charge = 0;
    m_hull = 0;
}

// ---- the simulator -----------------------------------------------------------------------------------------------

int HeadlessSimulator::AddDeathStar(int family, int id, int notCompleted, int hasFacility, float hull, float hullNow,
                                    int hangarFree) {
    TacticalDeathStar* deathStar = new TacticalDeathStar(family, id, notCompleted, hasFacility, hull, hullNow, hangarFree);
    RegisterDeathStar(deathStar);
    // BUG(original): a failed allocation is dereferenced here and above.
    return deathStar->m_id;
}

void HeadlessSimulator::RegisterDeathStar(TacticalDeathStar* deathStar) {
    deathStar->AssignHandle(1);
    deathStar->m_ownerHandle = deathStar->m_id;
    GetTacticalRegistry()->m_deathStar = deathStar;
}

int HeadlessSimulator::SideZeroHasMoreFighters() {
    unsigned sideZero = 0;
    unsigned sideOne = 0;
    TacticalUnit* deathStarUnit = GetTacticalRegistry()->GetDeathStar();
    if (deathStarUnit == nullptr) return 0;
    TacticalDeathStar* deathStar = static_cast<TacticalDeathStar*>(deathStarUnit);
    if (!deathStar->IsReady()) return 0;
    for (UnitRefNode* node = m_active.FirstNode(); node; node = node->Next()) {
        TacticalShipBase* unit = node->Value();
        if (unit->GetUnitClass() != 1) continue;
        if (node->Value()->m_side == 0) {
            ++sideZero;
        } else {
            ++sideOne;
        }
    }
    return sideOne < sideZero;
}

int HeadlessSimulator::DockInHangar(TacticalShipBase* unit) {
    int placed = 0;
    if (unit->m_state != 2) return 0;
    if (unit->m_carrierHandle != 0) {
        TacticalHangar* hangar = GetTacticalRegistry()->FindLaunchInterface(unit->m_carrierHandle);
        if (hangar != nullptr) {
            GetHeadlessSimulator();
            TacticalUnit* deathStarUnit = GetTacticalRegistry()->GetDeathStar();
            TacticalDeathStar* deathStar = static_cast<TacticalDeathStar*>(deathStarUnit);
            TacticalHangar* deathStarHangar = deathStar ? static_cast<TacticalHangar*>(deathStar) : nullptr;
            if (deathStarHangar == hangar) {
                if (deathStar->IsOperational() == 1 && hangar->Dock(unit) == 1) placed = 1;
            } else {
                TacticalShipBase* carrier = static_cast<TacticalCapitalShip*>(hangar);
                if (IsActiveUnit(carrier) == 1 && hangar->Dock(unit) == 1) placed = 1;
            }
        }
    }
    if (placed == 0) {
        for (UnitRefNode* node = m_sides[unit->m_side]->m_fleet.FirstNode(); node; node = node->Next()) {
            TacticalShipBase* carrier = node->Value();
            if (IsActiveUnit(carrier) == 1 && static_cast<TacticalCapitalShip*>(carrier)->Dock(unit) == 1) {
                placed = 1;
                break;
            }
        }
        if (placed == 0 && unit->m_side == 1) {
            GetHeadlessSimulator();
            TacticalUnit* deathStarUnit = GetTacticalRegistry()->GetDeathStar();
            if (deathStarUnit != nullptr) {
                TacticalDeathStar* deathStar = static_cast<TacticalDeathStar*>(deathStarUnit);
                if (deathStar->IsOperational()) placed = deathStar->Dock(unit);
            }
        }
    }
    if (placed == 1) {
        unit->m_state = 5;
        m_sides[unit->m_side]->m_hangars.RemoveById(unit->m_id);
    }
    return placed;
}
