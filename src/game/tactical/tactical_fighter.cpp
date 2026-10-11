#include "tactical/tactical_groups.h"
#include "core/reb_x87.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_externals.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_ship.h"
#include "tactical/tactical_simulator.h"

// Fighter squadron (0x5b8540-0x5ba3a0). x87 conventions as in tactical_ship.cpp.

namespace {

constexpr float kPi = 3.1415927f;

}  // namespace

TacticalFighter::TacticalFighter(uint32_t family, uint32_t id, int notAlliance, uint32_t typeIndex, String name,
                                 int carrierHandle, float field64, float field68, float field60, int squadSize,
                                 int currentSize, int zero, float hyperdriveCount, int grid02, int grid12,
                                 int grid22, int grid32, int grid01, int grid11, int grid21, int grid31, int grid00,
                                 int grid10, int grid20, int grid30, float a42, float a41, float a40,
                                 int totalTorpedoes, float torpedoRange) {
    m_objectFamily = family;
    m_objectId = id;
    m_side = notAlliance;
    m_unitKind = 1;
    m_typeIndex = typeIndex;
    m_name.Assign(name);
    m_carrierHandle = carrierHandle;
    m_speed = static_cast<float>(static_cast<double>(field64) * 0.4f);
    m_commanderBonus = field68;
    m_hullMax = static_cast<float>(static_cast<double>(squadSize) * 8.0f);
    m_hull = static_cast<float>(static_cast<double>(currentSize) * 8.0f);
    __builtin_memcpy(&m_unk3f0, &zero, sizeof zero);
    m_value398 = a42;
    m_value394 = a41;
    m_value3a0 = a40;
    m_totalTorpedoes = totalTorpedoes;
    m_torpedoRange = torpedoRange;
    m_shieldMax = field60;
    m_shield = field60;
    m_hyperdriveCount = hyperdriveCount;
    m_hyperdriveReady = hyperdriveCount != 0.0f;
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
    m_groupHandle = 0;
    m_unk178 = 0;
    m_chargingCount = 0;
    m_unk3b4 = 0;
    m_firepowerTotal = 0;
    for (unsigned index = 0; index < 4; ++index) {
        TacticalWeaponArc& arc = m_arcs[index];
        const double total = (static_cast<double>(arc.m_countB) * 1.0f + static_cast<double>(arc.m_countC) * 1.0f) +
                             static_cast<double>(arc.m_countA) * 1.0f;
        arc.m_chargeMax = static_cast<float>(total);
        m_firepowerTotal = static_cast<float>(total + m_firepowerTotal);
        m_chargeQueue[index] = -1;
        arc.m_chargeState = arc.m_chargeMax > 0.0f ? 1 : 0;
    }
    m_weaponRecharge = 0.25f;
    m_unk3d8 = m_firepowerTotal;
    m_unk3e0 = m_firepowerTotal;
    m_unk3dc = m_firepowerTotal;
    m_shieldRecharge = 1.0f;
    m_unk3f4 = 0;
    m_scale = static_cast<float>(reb_sin(static_cast<double>(kPi) * 0.49444446f));
    m_unk08 = 0.3f;
    const double unk0c = static_cast<double>(0.3f) * 4.0f;
    m_weaponCH = 0;
    m_hitCount = 0;
    m_radius = static_cast<float>(unk0c);
    m_unk160 = static_cast<float>(unk0c * 1.2f);
    m_tractoredBy[0] = 0;
    m_tractoredBy[1] = 0;
    m_tractoredBy[2] = 0;
    m_tractoredBy[3] = 0;
    m_unk3a4 = 0;
    m_unk154 = 0;
    m_hasGravityWell = 0;
    m_turnAxis.m_x = static_cast<float>(static_cast<double>(kPi) * 0.083333336f);
    m_turnAxis.m_y = 0;
    m_turnAxis.m_z = 0;
    m_turnAxis.m_cachedLength = -1.0f;
    if (GetTacticalGame()->m_abstract == 0) {
        if (GetTurnRate() > 1.0f) {
            m_turnAxis.m_y = 0;
            m_turnAxis.m_z = 0;
            m_turnAxis.m_cachedLength = -1.0f;
            m_turnAxis.m_x = static_cast<float>(((static_cast<double>(kPi) * 0.5f) * GetTurnRate()) * 0.11111111f);
        } else {
            m_turnAxis.m_x = static_cast<float>(static_cast<double>(kPi) * 0.083333336f);
            m_turnAxis.m_y = 0;
            m_turnAxis.m_z = 0;
            m_turnAxis.m_cachedLength = -1.0f;
        }
    }
}

TacticalFighter::~TacticalFighter() {  }

double TacticalFighter::GetWeaponRecharge() {
    const double value = (static_cast<double>(m_hull) * 3.751f) / 8.0f;
    m_weaponRecharge = static_cast<float>(value);
    return value;
}

double TacticalFighter::GetShieldRecharge() {
    const double value = (static_cast<double>(m_hull) * 0.025f) / 8.0f;
    m_shieldRecharge = static_cast<float>(value);
    return value;
}

double TacticalFighter::GetSpeed() {
    float tractored = 0.0f;
    if (m_unk3b4 == 0.0f) {
        for (UnitRefNode* node = m_unk360.FirstNode(); node; node = node->Next()) {
            tractored = static_cast<float>(node->Value()->TractorPower() + tractored);
        }
#ifdef REB_NO_TACTICAL_3D
        const double skill = GetHeadlessSimulator()->CommanderSkillB(m_side);
#else
        const double skill = GetTacticalGame()->m_abstract != 0 ? GetHeadlessSimulator()->CommanderSkillB(m_side)
                                                                 : FUN_005ad7e0(GetObjectManager(), m_side);
#endif
        const double base = skill + m_speed;
        const double speed = base - ((base * 0.25f) * static_cast<double>(m_speedCH) + tractored);
        if (speed > 0.0f) return speed;
    }
    return 0.0f;
}

double TacticalFighter::GetTurnRate() {
    if (GetSpeed() == 0.0f) return 1.0f;
#ifdef REB_NO_TACTICAL_3D
    GetHeadlessSimulator()->CommanderSkillB(m_side);
#else
    if (GetTacticalGame()->m_abstract == 1) {
        GetHeadlessSimulator()->CommanderSkillB(m_side);
    } else {
        FUN_005ad7e0(GetObjectManager(), m_side);
    }
#endif
    const double turn = static_cast<double>(m_commanderBonus) - -1.0f;
    if (turn < 9.0f) return turn;
    return 9.0f;
}

FightSquadResult* TacticalFighter::BuildResult(FightSquadResult* out) {
    const int state = m_state;
    out->m_unk00 = static_cast<int>(m_id);
    out->m_unk08 = static_cast<int>(m_objectFamily);
    out->m_unk0c = static_cast<int>(m_objectId);
    if (state == 0xb) {
        out->m_squadSize = 0;
    } else {
        const uint32_t squads = reb_ftou(m_hull / 8.0f);
        out->m_squadSize = squads < 1 ? 1 : static_cast<int>(squads);
    }
    const int carrier = m_carrierHandle;
    out->m_unk14 = 0;
    out->m_unk10 = carrier;
    if (carrier != 0) {
        TacticalHangar* launcher = GetTacticalRegistry()->FindLaunchInterface(carrier);
        if (launcher != nullptr) out->m_unk14 = (m_state == 1 || m_state == 5) ? 1 : 0;
    }
    out->m_unk1c = m_state;
    return out;
}

// The same code as the capital ship's slot 50 (0x5b00c0), shared by the linker.
void TacticalFighter::Virtual50() { FUN_005ba980(FUN_005ba8b0(), 1, 1); }

#ifndef REB_NO_TACTICAL_3D
TacticalClient * g_tacticalClient{};
TacticalHub * g_tacticalHub{};
TacticalObjectManager * g_tacticalObjectManager{};
TacticalSoundManager * g_tacticalSoundManager{};
TacticalTurnSync * g_tacticalTurnSync{};
#endif
float g_shipMinTurnStep{};
