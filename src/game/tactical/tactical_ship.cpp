#include "tactical/tactical_ship.h"

#include "tactical/tactical_externals.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_ship_ext.h"
#include "tactical/tactical_simulator.h"

// Conventions of this file: the original keeps intermediate results on the x87 stack (the process runs with a 53-bit
// mantissa), so those are `double` here; every value the original stores to a `float` field or temporary goes through
// an explicit (float) cast, which makes the compiler store and reload it.

// ---- weapon arc ---------------------------------------------------------------------------------------------------

TacticalWeaponArc::TacticalWeaponArc() {
    FUN_005d2530(&m_profileA);
    FUN_005d24e0(&m_profileB);
    FUN_005d2490(&m_profileC);
    m_unk54 = 0;
    m_unk5c = 0;
    m_unk58 = 0;
    m_damageA = 0;
    m_damageB = 0;
    m_damageC = 0;
    m_unk60 = 0;
    m_charge = 0;
    m_firepower = 0;
    m_chargeMax = 0;
    m_countA = 0;
    m_countB = 0;
    m_countC = 0;
    m_chargeState = 1;
}

TacticalWeaponArc::~TacticalWeaponArc() {
    FUN_005d24d0(&m_profileC);
    FUN_005d2520(&m_profileB);
    FUN_005d2570(&m_profileA);
}

// ---- construction -------------------------------------------------------------------------------------------------

TacticalShipBase::TacticalShipBase() {
    m_channelA.m_data = nullptr;
    m_channelB.m_data = nullptr;
    m_state = 0;
    m_unk35c = 0;
    m_carrierHandle = 0;
    m_groupHandle = 0;
    m_unk178 = 0;
    m_unk15c = 0;
    m_value398 = 16.0f;
    m_value394 = 8.0f;
    m_value3a0 = 35.0f;
    m_tractorRange = 0;
    m_tractorTarget = 0;
    m_chargingCount = 0;
    m_unk3b0 = 0;
    m_unk3b4 = 0;
    m_tractorPower = 0;
    m_unk3ac = 0;
    m_shieldMax = 10.0f;
    m_shield = 10.0f;
    m_unk3c4 = 0;
    m_hullMax = 50.0f;
    m_hull = 50.0f;
    m_unk3d0 = 0;
    m_firepowerTotal = 100.0f;
    m_unk3d8 = 100.0f;
    m_unk3e0 = 100.0f;
    m_weaponRecharge = 50.0f;
    m_shieldRecharge = 50.0f;
    m_hyperdriveCount = 2.0f;
    m_unk3f0 = 33.0f;
    m_unk3f4 = 0;
    m_speed = 10.0f;
    m_speedCH = 0;
    m_shieldCH = 0;
    m_hyperdriveCH = 0;
    m_tractorCH = 0;
    m_weaponCH = 0;
    m_hitCount = 0;
    m_unk04 = static_cast<float>(X87Sin(static_cast<double>(3.1415927f) * static_cast<double>(0.49444446f)));
    m_tractoredBy[0] = 0;
    m_tractoredBy[1] = 0;
    m_tractoredBy[2] = 0;
    m_tractoredBy[3] = 0;
    m_unk154 = 0;
    m_hasGravityWell = 0;
    m_unk3a8 = 1;
    m_unk400 = 100;
    m_turnAxis.m_x = 0.12f;
    m_turnAxis.m_y = 0;
    m_turnAxis.m_z = 0;
    m_turnAxis.m_cachedLength = -1.0f;
    m_unk40c = 0;
    m_unk414 = 0;
    m_unk410 = 0;
}

TacticalShipBase::~TacticalShipBase() { REB_KEEP_VPTR_STORE(); }

TacticalHangar::TacticalHangar() {
    m_unk24 = 0;
    m_unk2c = 0;
    m_docked = new TacticalUnitTree();
    m_launched = new TacticalUnitTree();
    m_unk28 = new TacticalUnitTree();
    m_ownerHandle = 0;
    m_launchInterval = 5.0f;
    m_inFlight = 0;
    m_launchTime = 0;
    m_capacity = 0;
    m_aboard = 0;
}

TacticalHangar::~TacticalHangar() {
    REB_KEEP_VPTR_STORE();
    delete m_docked;
    m_docked = nullptr;
    delete m_launched;
    m_launched = nullptr;
    delete m_unk28;
    m_unk28 = nullptr;
}

// ---- hangar ---------------------------------------------------------------------------------------------------------

int TacticalHangar::TakeOut(TacticalShipBase* fighter) {
    if (m_inFlight == 0) return 0;
    --m_aboard;
    --m_inFlight;
    m_docked->RemoveUnit(fighter);
    return 1;
}

int TacticalHangar::Dock(TacticalShipBase* fighter) {
    if (m_inFlight >= m_capacity) return 0;
    fighter->m_carrierHandle = m_ownerHandle;
    fighter->m_state = 1;
    ++m_inFlight;
    m_launched->Add(fighter);
    return 1;
}

double TacticalHangar::GetLaunchInterval() { return m_launchInterval; }

int TacticalHangar::Launch(TacticalShipBase* fighter) {
    if (m_aboard >= m_capacity) return 0;
    ++m_inFlight;
    ++m_aboard;
    m_docked->Add(fighter);
    fighter->m_carrierHandle = m_ownerHandle;
    fighter->SetState1();
    if (m_launchTime == 0.0f && GetTacticalGame()->m_abstract == 0) {
        const float time = static_cast<float>(GetObjectManager()->m_clock);
        const float roll = static_cast<float>(static_cast<uint32_t>(FUN_005a8a70(GetObjectManager(), 0, 8)));
        m_launchTime = static_cast<float>(GetLaunchInterval() + roll + time);
    }
    return 1;
}

// ---- shared accessors -------------------------------------------------------------------------------------------

double TacticalShipBase::ScaleByHull(float value) { return (static_cast<double>(m_hull) * value) / m_hullMax; }

double TacticalShipBase::ScaleByHullFull() { return ScaleByHull(1.0f); }

double TacticalShipBase::ShieldFraction() {
    if (m_shieldMax == 0.0f) return 0.0f;
    return static_cast<double>(m_shield) / m_shieldMax;
}

double TacticalShipBase::WeaponRechargeFraction() {
    if (m_weaponRecharge == 0.0f) return 0.0f;
    return GetWeaponRecharge() / m_weaponRecharge;
}

double TacticalShipBase::SpeedFraction() {
    if (m_speed == 0.0f) return 0.0f;
    return GetSpeed() / m_speed;
}

double TacticalShipBase::HyperdriveFraction() {
    if (m_hyperdriveCount == 0.0f) return 0.0f;
    return WorkingHyperdrives() / m_hyperdriveCount;
}

double TacticalShipBase::ShieldRechargeFraction() {
    if (m_shieldRecharge == 0.0f) return 0.0f;
    return GetShieldRecharge() / m_shieldRecharge;
}

double TacticalShipBase::TractorFraction() {
    if (m_tractorPower == 0.0f) return 0.0f;
    return TractorPower() / m_tractorPower;
}

double TacticalShipBase::TractorPower() {
    const float scaled = static_cast<float>(ScaleByHull(m_tractorPower));
    const double power = scaled - (static_cast<double>(m_tractorPower) * 0.25f) * static_cast<double>(m_tractorCH);
    if (power < 0.0f) return 0.0f;
    return power;
}

double TacticalShipBase::MaxWeaponRange() {
    float best = 0.0f;
    for (TacticalWeaponArc& arc : m_arcs) {
        if (arc.m_countB != 0 && GetValue394() > best) best = static_cast<float>(GetValue394());
        if (arc.m_countA != 0 && GetValue398() > best) best = static_cast<float>(GetValue398());
        if (arc.m_countC != 0 && GetValue3a0() > best) best = static_cast<float>(GetValue3a0());
    }
    return best;
}

double TacticalShipBase::WorkingHyperdrives() {
    const double failed = m_hyperdriveCH;
    if (failed < m_hyperdriveCount) return m_hyperdriveCount - failed;
    return 0.0f;
}

int TacticalShipBase::UpdateHyperdriveReady() {
    const int ready = WorkingHyperdrives() != 0.0f && GetSpeed() != 0.0f;
    m_hyperdriveReady = ready;
    return ready;
}

double TacticalShipBase::TorpedoStrength() {
    return ScaleByHull(static_cast<float>(static_cast<double>(m_totalTorpedoes) * 0.53333336f));
}

void TacticalShipBase::SetState1() {
    m_fC_04 = 1;
    m_state = 1;
}

int TacticalShipBase::IsState1() { return m_state == 1; }

int TacticalShipBase::HasKind1Weapons() {
    for (const TacticalWeaponArc& arc : m_arcs) {
        if (arc.m_countA != 0) return 1;
    }
    return 0;
}

int TacticalShipBase::HasOnlyKind1Weapons() {
    for (const TacticalWeaponArc& arc : m_arcs) {
        if (arc.m_countA != 0) return 0;
        if (arc.m_countC != 0) return 0;
        if (arc.m_countB != 0) return 0;
    }
    return 1;
}

void TacticalShipBase::ClearArcCharge(unsigned arc) {
    if (arc < 4) {
        m_arcs[arc].m_chargeState = 0;
        m_arcs[arc].m_charge = 0;
    }
}

void TacticalShipBase::QueueArcCharge(unsigned arc) {
    if (arc >= 4) return;
    const unsigned count = m_chargingCount;
    unsigned index = 0;
    for (; index < count; ++index) {
        if (static_cast<unsigned>(m_chargeQueue[index]) == arc) return;
    }
    m_chargingCount = count + 1;
    m_chargeQueue[index] = static_cast<int>(arc);
}

// ---- critical hits ----------------------------------------------------------------------------------------------

void TacticalShipBase::ApplyCriticalHit(int kind) {
    ++m_hitCount;
    switch (static_cast<unsigned>(kind)) {
        case 0:
            m_shieldCH = m_shieldCH + 1 < 4 ? m_shieldCH + 1 : 4;
            break;
        case 1:
            m_weaponCH = m_weaponCH + 1 < 4 ? m_weaponCH + 1 : 4;
            break;
        case 2:
            m_tractorCH = m_tractorCH + 1 < 4 ? m_tractorCH + 1 : 4;
            if (GetTacticalGame()->m_abstract == 0 && m_channelB.m_data == nullptr && m_tractorTarget != 0 &&
                TractorPower() == 0.0f) {
                const int unit = m_id;
                GetObjectManager()->PostEvent(0x29, unit, m_tractorTarget, 1);
                m_unk3ac = 1;
            }
            break;
        case 3:
            m_speedCH = m_speedCH + 1 < 4 ? m_speedCH + 1 : 4;
            break;
        case 4: {
            const uint32_t next = m_hyperdriveCH + 1;
            const uint32_t limit = static_cast<uint32_t>(m_hyperdriveCount);
            m_hyperdriveCH = next < limit ? next : limit;
            break;
        }
    }
}

void TacticalShipBase::RepairCriticalHit(int kind) {
    if (m_hitCount == 0) return;
    --m_hitCount;
    switch (static_cast<unsigned>(kind)) {
        case 0:
            m_shieldCH = m_shieldCH - 1;
            break;
        case 1:
            m_weaponCH = m_weaponCH - 1;
            break;
        case 2:
            m_tractorCH = m_tractorCH - 1;
            break;
        case 3:
            m_speedCH = m_speedCH - 1;
            break;
        case 4:
            m_hyperdriveCH = m_hyperdriveCH - 1;
            break;
    }
}

// ---- weapons ------------------------------------------------------------------------------------------------------

uint32_t TacticalShipBase::SelectArc(int targetClass) {
    uint32_t best = 4;
    float bestFirepower = 0.0f;
    for (TacticalWeaponArc& arc : m_arcs) {
        arc.m_firepower = 0;
        arc.m_damageC = 0;
        arc.m_damageA = 0;
        arc.m_damageB = 0;
        if (arc.m_countB != 0 && targetClass == 0) {
            const double factor = static_cast<double>(arc.m_profileA.m_f0c) * arc.m_profileA.m_f04;
            arc.m_damageC = static_cast<float>(static_cast<double>(arc.m_countB) * factor);
        }
        if (arc.m_countA != 0) {
            const double factor = targetClass == 0 ? static_cast<double>(arc.m_profileB.m_f04) * arc.m_profileB.m_f0c
                                                   : static_cast<double>(arc.m_profileB.m_f04) * arc.m_profileB.m_f08;
            arc.m_damageA = static_cast<float>(static_cast<double>(arc.m_countA) * factor);
        }
        if (arc.m_countC != 0) {
            const double factor = targetClass == 0 ? static_cast<double>(arc.m_profileC.m_f0c) * arc.m_profileC.m_f04
                                                   : static_cast<double>(arc.m_profileC.m_f04) * arc.m_profileC.m_f08;
            arc.m_damageB = static_cast<float>(static_cast<double>(arc.m_countC) * factor);
        }
    }
    for (uint32_t index = 0; index < 4; ++index) {
        TacticalWeaponArc& arc = m_arcs[index];
        const double total = static_cast<double>(arc.m_damageB) + arc.m_damageC + arc.m_damageA;
        arc.m_firepower = static_cast<float>(total);
        if (total > bestFirepower) {
            bestFirepower = static_cast<float>(total);
            best = index;
        }
    }
    return best;
}

void TacticalShipBase::TakeDamage(float damage) {
    const float hullBefore = m_hull;
    if (hullBefore == 0.0f) return;
    const float shieldBefore = m_shield;
    const double shieldAfter = static_cast<double>(m_shield) - damage;
    m_shield = static_cast<float>(shieldAfter);
    if (shieldBefore > 0.0f) {
        const double perShield = 100.0f / static_cast<double>(shieldBefore);
        const double lost = static_cast<double>(shieldBefore) - shieldAfter;
        const double percent = lost * perShield;
        const float chance = percent < 1.0 ? 1.0f : static_cast<float>(percent);
        const uint32_t roll = GetHeadlessSimulator()->RandomBetween(0, 100);
        if (static_cast<double>(roll) <= chance) ApplyCriticalHit(0);
    }
    if (m_shield < 0.0f) {
        const float overflow = m_shield <= -1.0f ? m_shield : -1.0f;
        const double hullAfter = static_cast<double>(overflow) + m_hull;
        m_shield = 0;
        m_hull = static_cast<float>(hullAfter);
        if (hullAfter < 0.0) m_hull = 0.0f;
        const uint32_t roll = GetHeadlessSimulator()->RandomBetween(0, 100);
        const double severity = static_cast<double>(roll) + ((static_cast<double>(m_hull) - hullBefore) * 100.0f) / hullBefore;
        const float severityF = static_cast<float>(severity);
        if (severity > 50.0f) {
            if (severityF <= 65.0f) {
                ApplyCriticalHit(0);
            } else if (severityF <= 80.0f) {
                ApplyCriticalHit(1);
            } else if (severityF <= 90.0f) {
                ApplyCriticalHit(2);
            } else if (severityF <= 95.0f) {
                ApplyCriticalHit(3);
            } else if (severityF <= 100.0f) {
                ApplyCriticalHit(4);
            }
        }
    }
}

void TacticalShipBase::Recharge(float step) {
    if (GetUnitClass() == 1) {
        const double scaled = ScaleByHull(m_shieldMax);
        const float shield = m_shield;
        m_shieldMax = static_cast<float>(scaled);
        if (scaled <= shield) m_shield = static_cast<float>(scaled);
    }
    const double rate = GetShieldRecharge();
    const double missing = static_cast<double>(m_shieldMax) - m_shield;
    const double gain = rate * step;
    if (missing > 0.0) {
        const double total = gain + m_shield;
        m_shield = static_cast<float>(total < m_shieldMax ? total : static_cast<double>(m_shieldMax));
    }
    double budget = GetWeaponRecharge() * step;
    if (m_chargingCount >= 1 && budget > 0.0) {
        for (uint32_t position = 1; position <= m_chargingCount; ++position) {
            if (!(budget > 0.0)) break;
            TacticalWeaponArc& arc = m_arcs[m_chargeQueue[position - 1]];
            const double missingCharge = static_cast<double>(arc.m_chargeMax) - arc.m_charge;
            if (budget < missingCharge) {
                arc.m_charge = static_cast<float>(budget + arc.m_charge);
            } else {
                arc.m_charge = arc.m_chargeMax;
                budget = budget - missingCharge;
                arc.m_chargeState = 1;
            }
        }
    }
    // BUG(original): when an entry is removed the one that shifts into its place is not examined.
    for (uint32_t position = 0; position < m_chargingCount; ++position) {
        if (m_arcs[m_chargeQueue[position]].m_chargeState != 1) continue;
        for (uint32_t i = position; i < 3; ++i) m_chargeQueue[i] = m_chargeQueue[i + 1];
        if (m_chargingCount == 4) m_chargeQueue[3] = 0;
        --m_chargingCount;
    }
}

double TacticalShipBase::GetSpeed() {
    float tractored = 0.0f;
    if (m_unk3b4 == 0.0f) {
        for (TacticalUnitNode* node = m_unk360.FirstNode(); node; node = node->Next()) {
            tractored = static_cast<float>(node->Value()->TractorPower() + tractored);
        }
        const double skill = GetTacticalGame()->m_abstract != 0 ? GetHeadlessSimulator()->CommanderSkillA(m_side)
                                                                 : FUN_005ad750(GetObjectManager(), m_side);
        const double base = skill + m_speed;
        const double speed = base - ((base * 0.25f) * static_cast<double>(m_speedCH) + tractored);
        if (speed > 0.0f) return speed;
    }
    return 0.0f;
}

// ---- carrier helpers --------------------------------------------------------------------------------------------

int __stdcall TacticalShipIsInState3(int handle) {
    TacticalShipBase* ship = reinterpret_cast<TacticalShipBase*>(FUN_005a6340(GetTacticalRegistry(), handle));
    return ship != nullptr && ship->m_state == 3;
}

void TacticalHangar::ReleaseDocked() {
    for (TacticalUnitNode* node = m_docked->FirstNode(); node;) {
        TacticalUnitNode* next = node->Next();
        if (TacticalShipIsInState3(node->GetHandle()) == 0) {
            m_launched->Add(node->Value());
            node->Value()->m_state = 5;
            m_docked->RemoveById(node->GetHandle());
        }
        node = next;
    }
}

void TacticalHangar::Disband() {
    for (TacticalUnitNode* node = m_docked->FirstNode(); node; node = node->Next()) node->Value()->Destroy();
    for (TacticalUnitNode* node = m_launched->FirstNode(); node; node = node->Next()) node->Value()->Destroy();
    m_docked->Clear();
    m_launched->Clear();
}

int TacticalFighter::GetSoundId() { return 0x16 - (m_side != 0 ? 1 : 0); }

void TacticalShipBase::SetState2() {
    m_fC_04 = 0;
    m_state = 2;
}

TacticalShipBase* TacticalShipBase::GetNextShip() {
    AvlNode* next = TacticalUnit::m_successor;
    return next != nullptr ? static_cast<TacticalShipBase*>(static_cast<TacticalUnit*>(next)) : nullptr;
}
