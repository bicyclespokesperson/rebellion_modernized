#include "tactical/tactical_objmgr.h"

#include "tactical/tactical_externals.h"

extern float g_tacticalSideScaleBase;     // 0x66c3d8
extern float g_tacticalSideScaleSquared;  // 0x6bc4a4
extern float g_tacticalGameScaleBase;     // 0x66bad8
extern float g_tacticalGameScaleSquared;  // 0x6bc244

namespace {

#ifndef REB_NO_TACTICAL_3D
constexpr float kMillisecondsPerSecond = 1000.0f;
constexpr int kMinimumRunningMode = 2;
constexpr int kHubRunning = 1;

int hubState(TacticalHub* hub) { return *reinterpret_cast<int*>(hub); }
#endif

}  // namespace

TacticalObjectManager* GetObjectManager() {
#ifdef REB_NO_TACTICAL_3D
    return nullptr;
#else
    return g_tacticalObjectManager;
#endif
}

void DestroyObjectManager() {
#ifndef REB_NO_TACTICAL_3D
    if (g_tacticalObjectManager) g_tacticalObjectManager->ScalarDeletingDestructor(1);
    g_tacticalObjectManager = nullptr;
#endif
}

void InitTacticalSideScale() { g_tacticalSideScaleSquared = g_tacticalSideScaleBase * g_tacticalSideScaleBase; }

void InitTacticalGameScale() { g_tacticalGameScaleSquared = g_tacticalGameScaleBase * g_tacticalGameScaleBase; }

#ifndef REB_NO_TACTICAL_3D
void TacticalObjectManager::Virtual02() {
    if (static_cast<unsigned>(m_mode) > kMinimumRunningMode) Virtual01();
}

void TacticalObjectManager::Virtual03() {
    if (static_cast<unsigned>(m_mode) > kMinimumRunningMode) Virtual00();
}

void TacticalObjectManager::Virtual00() {
    ++m_pauseCount;
    if (m_pauseCount < 0) m_pauseCount = 0;
    FUN_005c0f80(m_pauseCount != 0);
}

void TacticalObjectManager::Virtual01() {
    --m_pauseCount;
    if (m_pauseCount < 0) m_pauseCount = 0;
    FUN_005c0f80(m_pauseCount != 0);
    if (m_pauseCount == 0) FUN_0059e840(FUN_0059ed70());
}

int TacticalObjectManager::PlayerSideIsActive() {
    const int side = GetObjectManager()->m_playerSide;
    return GetObjectManager()->m_sides[side]->m_status == 1;
}

int TacticalObjectManager::PlayerSideCanAct(int side) {
    if (side < 2 && FUN_005ae1d0(this, side == 0) != 1) return m_sides[side]->HasNoShips();
    return 0;
}

int TacticalObjectManager::CanPlayerCommandShip(void* ship) {
    if (hubState(FUN_0059ed70()) == kHubRunning && ship) {
        const int shipSide = *reinterpret_cast<int*>(static_cast<char*>(ship) + 0x168);
        if (shipSide == m_playerSide && m_sides[m_playerSide]->m_unk84 == 0) return 1;
    }
    return 0;
}

int TacticalObjectManager::CanPlayerCommandFighter(void* fighter) {
    if (hubState(FUN_0059ed70()) == kHubRunning && fighter) {
        const int fighterSide = *reinterpret_cast<int*>(static_cast<char*>(fighter) + 0x50);
        if (fighterSide == m_playerSide && m_sides[m_playerSide]->m_unk84 == 0) return 1;
    }
    return 0;
}

int TacticalObjectManager::AnySideHasFlag() {
    if (m_sides[m_playerSide]->m_unk84 != 0) return 1;
    return m_sides[m_playerSide == 0]->m_unk84 != 0;
}

void TacticalObjectManager::SetSideValue(int side, float seconds) {
    if (side < 2) m_sides[side]->m_valueScaled = seconds * kMillisecondsPerSecond;
}

void TacticalObjectManager::SetSideFlag(int side, int value) {
    if (side < 2) m_sides[side]->m_unk80 = value;
}

void TacticalObjectManager::SetControlFlags(int sideA, int sideB) {
    m_sides[0]->m_controlFlag = sideA;
    m_sides[1]->m_controlFlag = sideB;
}

void TacticalObjectManager::SetSystemInfo(int field44, std::string name) {
    m_systemField44 = field44;
    (m_systemName = name);
}
#endif

float g_tacticalGameScaleBase = 3.0f;
float g_tacticalGameScaleSquared{};
float g_tacticalSideScaleBase = 3.0f;
float g_tacticalSideScaleSquared{};
