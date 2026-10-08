#include "tactical/tactical_externals.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_ship.h"
#include "tactical/tactical_ship_ext.h"
#include "tactical/tactical_simulator.h"

// Capital ship (0x5af4e0-0x5b8540). x87 conventions as in tactical_ship.cpp.

namespace {

constexpr float kPi = 3.1415927f;

}  // namespace

TacticalCapitalShip::TacticalCapitalShip(uint32_t family, uint32_t id, int notAlliance, uint32_t typeIndex, String name,
                                         float hull, float currentHull, float damageControl, int freeHangar,
                                         float tractorPower, float tractorRange, int gravityWell, float interdiction,
                                         float field64, float field68, float field60, float hyperdriveCount,
                                         float weaponRecharge, float shieldRecharge, int grid02, int grid12,
                                         int grid22, int grid32, int grid01, int grid11, int grid21, int grid31,
                                         int grid00, int grid10, int grid20, int grid30, int speedCH, int shieldCH,
                                         int weaponCH, int hyperdriveCH, int tractorCH, float a42, float a41,
                                         float a40, int hundred) {
    m_objectFamily = family;
    m_objectId = id;
    m_side = notAlliance;
    m_unitKind = 0;
    m_typeIndex = typeIndex;
    m_name.Assign(name);
    m_hullMax = hull;
    m_hull = currentHull;
    m_capacity = freeHangar;
    m_aboard = 0;
    m_tractorPower = tractorPower;
    m_speed = static_cast<float>((static_cast<double>(field64) * 0.4f) * 0.6f);
    m_commanderBonus = field68;
    m_shieldMax = field60;
    m_shield = field60;
    m_hyperdriveCount = hyperdriveCount;
    m_hyperdriveReady = hyperdriveCount != 0.0f;
    m_value3a0 = a40;
    m_weaponRecharge = static_cast<float>(static_cast<double>(weaponRecharge) * 3.75f);
    m_shieldRecharge = shieldRecharge;
    m_weaponCH = weaponCH;
    m_arcs[0].m_countA = grid02;
    m_arcs[0].m_countC = grid00;
    m_arcs[0].m_countB = grid01;
    m_arcs[2].m_countA = grid12;
    m_arcs[2].m_countC = grid10;
    m_arcs[2].m_countB = grid11;
    m_arcs[3].m_countA = grid22;
    m_arcs[3].m_countC = grid20;
    m_arcs[3].m_countB = grid21;
    m_arcs[1].m_countA = grid32;
    m_arcs[1].m_countC = grid30;
    m_arcs[1].m_countB = grid31;
    m_value398 = a42;
    m_value394 = a41;
    m_carrierHandle = 0;
    m_groupHandle = 0;
    m_unk178 = 0;
    m_tractorTarget = 0;
    m_chargingCount = 0;
    m_unk3b0 = 0;
    m_unk3b4 = 0;
    m_tractorRange = tractorRange;
    m_unk3ac = 0;
    m_firepowerTotal = 0;
    for (unsigned index = 0; index < 4; ++index) {
        TacticalWeaponArc& arc = m_arcs[index];
        const double total = (static_cast<double>(arc.m_countB) * 1.0f + static_cast<double>(arc.m_countA) * 1.0f) +
                             static_cast<double>(arc.m_countC) * 1.0f;
        arc.m_chargeMax = static_cast<float>(total);
        m_firepowerTotal = static_cast<float>(total + m_firepowerTotal);
        m_chargeQueue[index] = -1;
        arc.m_chargeState = 0;
        if (arc.m_chargeMax > 0.0f) {
            if (m_weaponCH < 4) {
                arc.m_chargeState = 1;
                arc.m_charge = arc.m_chargeMax;
            } else {
                ClearArcCharge(index);
                QueueArcCharge(index);
            }
        }
    }
    m_unk3f4 = 0;
    m_unk3d8 = m_firepowerTotal;
    m_unk3e0 = m_firepowerTotal;
    m_unk3dc = m_firepowerTotal;
    m_unk3f0 = damageControl;
    m_totalTorpedoes = 0;
    m_torpedoRange = 0;
    m_tractorCH = tractorCH;
    m_hitCount = tractorCH + hyperdriveCH + shieldCH + speedCH;
    m_speedCH = speedCH;
    m_shieldCH = shieldCH;
    m_hyperdriveCH = hyperdriveCH;
    m_unk04 = static_cast<float>(X87Sin(static_cast<double>(kPi) * 0.49444446f));
    m_tractoredBy[0] = 0;
    m_tractoredBy[1] = 0;
    m_tractoredBy[2] = 0;
    m_tractoredBy[3] = 0;
    m_unk154 = 0;
    m_unk3a4 = 0;
    m_hasGravityWell = gravityWell != 0;
    m_unk08 = 3.0f;
    m_unk0c = m_typeIndex == 0x15 ? 19.0f : static_cast<float>(3.0f * 1.2f);
    m_unk400 = hundred;
    m_unk160 = static_cast<float>(static_cast<double>(m_unk0c) * 1.2f);
    m_turnAxis.m_x = static_cast<float>(static_cast<double>(kPi) * 0.083333336f);
    m_turnAxis.m_y = 0;
    m_turnAxis.m_z = 0;
    m_turnAxis.m_cachedLength = -1.0f;
    if (GetTacticalGame()->m_abstract == 0 && GetTurnRate() > 1.0f) {
        const float halfPi = static_cast<float>(static_cast<double>(kPi) * 0.5f);
        double turn = (GetTurnRate() * halfPi) * 0.11111111f;
        if (m_typeIndex == 0x15) {
            const float limit = static_cast<float>(static_cast<double>(halfPi) * 0.033333335f);
            if (!(turn < limit)) turn = limit;
        }
        m_turnAxis.m_x = static_cast<float>(turn);
        m_turnAxis.m_y = 0;
        m_turnAxis.m_z = 0;
        m_turnAxis.m_cachedLength = -1.0f;
    }
}

TacticalCapitalShip::~TacticalCapitalShip() { REB_KEEP_VPTR_STORE(); }

double TacticalCapitalShip::GetWeaponRecharge() {
    const float scaled = static_cast<float>(ScaleByHull(m_weaponRecharge));
    const double power = scaled - (static_cast<double>(m_weaponRecharge) * 0.25f) * static_cast<double>(m_weaponCH);
    if (power < 0.0f) return 0.0f;
    return power;
}

double TacticalCapitalShip::GetShieldRecharge() {
    const float scaled = static_cast<float>(ScaleByHull(m_shieldRecharge));
    const double power = scaled - (static_cast<double>(m_shieldRecharge) * 0.25f) * static_cast<double>(m_shieldCH);
    if (power < 0.0f) return 0.0f;
    return power;
}

double TacticalCapitalShip::GetTurnRate() {
    if (GetSpeed() == 0.0f) return 1.0f;
    const double skill = GetTacticalGame()->m_abstract != 0 ? GetHeadlessSimulator()->CommanderSkillA(m_side)
                                                             : FUN_005ad750(GetObjectManager(), m_side);
    const double turn = skill + m_commanderBonus;
    if (turn < 9.0f) return turn;
    return 9.0f;
}

CapitalShipResult* TacticalCapitalShip::BuildResult(CapitalShipResult* out) {
    CapitalShipResult* const result = out;
    result->m_unk00 = static_cast<int>(m_id);
    result->m_unk08 = static_cast<int>(m_objectFamily);
    result->m_unk0c = static_cast<int>(m_objectId);
    result->m_speedCH = static_cast<int>(m_speedCH);
    result->m_hyperdriveDamage = static_cast<int>(m_hyperdriveCH);
    result->m_tractorBeamCH = static_cast<int>(m_tractorCH);
    result->m_shieldRechargeCH = static_cast<int>(m_shieldCH);
    result->m_weaponRechargeCH = static_cast<int>(m_weaponCH);
    if (m_state == 0xb) {
        result->m_hull = 0;
    } else if (1.0f < m_hull || 1.0f == m_hull) {
        result->m_hull = m_hull;
    } else {
        result->m_hull = 1.0f;
    }
    result->m_unk28 = m_state;
    return result;
}

void __thiscall TacticalCapitalShipLaunchPoint(TacticalHangar* hangar, TacticalVector* out) {
    TacticalCapitalShip* ship = static_cast<TacticalCapitalShip*>(hangar);
    const double y = static_cast<double>(ship->m_position.m_y) - ship->m_unk08;
    out->m_x = ship->m_position.m_x;
    out->m_y = static_cast<float>(y);
    out->m_z = ship->m_position.m_z;
    out->m_cachedLength = -1.0f;
}

void TacticalCapitalShip::GetLaunchPoint(TacticalVector* out) { TacticalCapitalShipLaunchPoint(this, out); }

void TacticalCapitalShip::ApplyStatus(const void* record) {
    const int* fields = static_cast<const int*>(record);
    m_state = fields[0x28 / 4];
    m_hull = *reinterpret_cast<const float*>(&fields[0x10 / 4]);
    m_speedCH = fields[0x14 / 4];
    m_shieldCH = fields[0x18 / 4];
    m_hyperdriveCH = fields[0x1c / 4];
    m_tractorCH = fields[0x20 / 4];
    m_weaponCH = fields[0x24 / 4];
}

int TacticalCapitalShip::GetSoundId() {
    if (m_hullMax < 1100.0f) return 0x19;
    if (m_hullMax < 2000.0f) return 0x18;
    return 0x17;
}
