#include "core/reb_abi.h"
#include "tactical/tactical_game.h"
#include "audio/sound_manager.h"

#include <new>

#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "tactical/tactical_externals.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_simulator.h"

namespace {

struct TypeMapEntry {
    uint16_t m_key;
    uint16_t m_value;
};

struct SpriteIdEntry {
    int m_index;
    int m_value;
};

constexpr float kMillisecondsPerSecond = 1000.0f;
constexpr float kInitialCameraDistance = 150.0f;
constexpr float kSimulatorStep = 1.0f;
constexpr int kForcedSystemField44 = 0x1b;

#include "tactical/tactical_game_tables.inc"

void insertTypeMapEntry(TypeMapTree* tree, const TypeMapEntry& entry) {
    void* memory = ::operator new(sizeof(TypeMapNode));
    TypeMapNode* node = nullptr;
    if (memory) {
        node = new (memory) TypeMapNode();
        node->m_key = entry.m_key;
        node->m_value = entry.m_value;
    }
    tree->Insert(node);
}

void copyOutcome(BattleOutcome* to, const BattleOutcome* from) { *to = *from; }

int destroysSimulatorIfAny() {
    HeadlessSimulator* simulator = g_headlessSimulator;
    if (simulator) {
        simulator->~HeadlessSimulator();
        operator delete(simulator);
    }
    g_headlessSimulator = nullptr;
    return 1;
}

}  // namespace

// ---- type map ------------------------------------------------------------------------------------------------

TypeMapNode::TypeMapNode() {}

TypeMapNode::~TypeMapNode() {}

TypeMapTree::TypeMapTree() {}

TypeMapTree::~TypeMapTree() {}

int TypeMapTree::IsLess(AvlNode* a, AvlNode* b) {
    const TypeMapNode* left = static_cast<const TypeMapNode*>(a);
    const TypeMapNode* right = static_cast<const TypeMapNode*>(b);
    bool less = left->m_key < right->m_key;
    if (left->m_key == right->m_key) less = left->m_value < right->m_value;
    return less;
}

// ---- singletons -----------------------------------------------------------------------------------------------

TacticalGame* GetTacticalGame() {
    if (!g_tacticalGame) g_tacticalGame = NewOrNull<TacticalGame>();
    return g_tacticalGame;
}

void DestroyTacticalGame() {
    if (!g_tacticalGame) return;
    g_tacticalGame->ShutDownBattle();
    if (g_tacticalGame) delete g_tacticalGame;
    g_tacticalGame = nullptr;
}

HeadlessSimulator* GetHeadlessSimulator() {
    if (!g_headlessSimulator) {
        void* memory = ::operator new(0x274);
        g_headlessSimulator = memory ? new (memory) HeadlessSimulator() : nullptr;
    }
    return g_headlessSimulator;
}

// ---- construction -----------------------------------------------------------------------------------------------

TacticalGame::TacticalGame() {
    m_unk08 = nullptr;
    m_unk694 = 1;
    ++m_busy;
    if (m_busy < 0) m_busy = 0;
    m_firstFrame = 1;
    m_outcome = BattleOutcome();
    m_abstract = 0;
    for (int i = 0; i < 38; ++i) m_shipTypeNames[i].Assign(String(kShipTypeNames[i]));
    for (int i = 0; i < 12; ++i) m_stateNames[i].Assign(String(kStateNames[i]));
    for (const TypeMapEntry& entry : kTypeMapEntries) insertTypeMapEntry(&m_typeMap, entry);
    String unusedLongList[0x80];
    String unusedShortList[0x26];
    for (const SpriteIdEntry& entry : kSpriteIds) m_spriteIds[entry.m_index] = entry.m_value;
}

TacticalGame::~TacticalGame() {
    ++m_busy;
    if (m_busy < 0) m_busy = 0;
    if (m_unk08) delete m_unk08;
    m_unk08 = nullptr;
    ShutDownBattle();
    DestroyTacticalSoundManager();
    if (g_tacticalRegistry) delete g_tacticalRegistry;
    g_tacticalRegistry = nullptr;
}

uint16_t TacticalGame::LookupTypeSprite(int16_t key) {
    for (AvlNode* node = m_typeMap.First(); node; node = node->m_successor) {
        const TypeMapNode* entry = static_cast<const TypeMapNode*>(node);
        if (static_cast<int16_t>(entry->m_key) == key) return entry->m_value;
    }
    return 0;
}

// ---- battle set-up and tear-down ----------------------------------------------------------------------------------

void TacticalGame::BeginBattle(int appState) {
    if (m_abstract) {
        GetHeadlessSimulator()->FormBattleGroups(0);
        m_busy = 0;
        return;
    }
    AppSetState(appState);
    GetObjectManager()->SetPlayerSide(GetObjectManager()->m_playerSide);
    m_busy = 0;
}

int TacticalGame::ShutDownBattle() {
    ++m_busy;
    if (m_busy < 0) m_busy = 0;
    destroysSimulatorIfAny();
    DestroyObjectManager();
    DestroyTacticalHub();
    DestroyTurnSync();
    AppDestroyTacticalWindow();
    --m_busy;
    if (m_busy < 0) m_busy = 0;
    return 1;
}

uint32_t TacticalGame::FindShipTypeIndex(uint32_t typeBits) {
    const uint32_t wanted = typeBits & 0xfff;
    for (uint32_t index = 0; index < 0x100; ++index) {
        if (static_cast<uint32_t>(m_spriteIds[index]) == wanted) return index;
    }
    return 0;
}

int TacticalGame::GetPlayerSide() { return GetObjectManager()->m_playerSide; }

void TacticalGame::CallClientSlot1() {
    FUN_0059ed70();
    g_tacticalClient->Virtual01();
}

int REB_THISCALL TacticalGameSetupBattle(TacticalGame* self, int humanIsNotAlliance, int systemField44, String name, float sideAValue,
                              float sideBValue, int sideAC, int sideBC, GameTime timeA, GameTime timeB,
                              int battleKindNotAlliance, int systemFlag8, int controlIsEmpire, int controlIsAlliance,
                              int abstractFlag) {
    destroysSimulatorIfAny();
    DestroyObjectManager();
    self->m_abstract = abstractFlag;
    if (abstractFlag == 0) {
        FUN_005be0f0();
        GetObjectManager()->m_playerSide = humanIsNotAlliance;
        if (systemFlag8 == 1) systemField44 = kForcedSystemField44;
        GetObjectManager()->SetSystemInfo(systemField44, name);
        GetObjectManager()->SetSideValue(0, sideAValue);
        GetObjectManager()->SetSideValue(1, sideBValue);
        GetObjectManager()->SetSideFlag(0, sideAC);
        GetObjectManager()->SetSideFlag(1, sideBC);
        GetObjectManager()->m_sides[0]->m_battleTime = timeA;
        GetObjectManager()->m_sides[1]->m_battleTime = timeB;
        GetObjectManager()->m_battleKindNotAlliance = battleKindNotAlliance;
        GetObjectManager()->SetControlFlags(controlIsEmpire, controlIsAlliance);
        GetObjectManager()->m_cameraDistance = kInitialCameraDistance;
    } else {
        GetHeadlessSimulator()->SetSideValue(0, sideAValue);
        GetHeadlessSimulator()->SetSideValue(1, sideBValue);
        GetHeadlessSimulator()->SetSideFlag(0, sideAC);
        GetHeadlessSimulator()->SetSideFlag(1, sideBC);
        GetHeadlessSimulator()->m_sides[0]->m_unkb0 = 0;
        GetHeadlessSimulator()->m_sides[1]->m_unkb0 = 0;
        GetHeadlessSimulator()->m_sides[0]->m_unk24 = timeA.m_value;
        GetHeadlessSimulator()->m_sides[1]->m_unk24 = timeB.m_value;
        GetHeadlessSimulator()->m_battleKind = battleKindNotAlliance;
    }
    return 1;
}

int REB_THISCALL TacticalGameSetupBattleLocal(TacticalGame* self, int humanIsNotAlliance, int systemField44, String name, float sideAValue,
                                   float sideBValue, int sideAC, int sideBC, int battleKindNotAlliance,
                                   int systemFlag8, int controlIsEmpire, int controlIsAlliance, int abstractFlag) {
    destroysSimulatorIfAny();
    DestroyObjectManager();
    self->m_abstract = abstractFlag;
    if (abstractFlag == 0) {
        FUN_005bf320();
        GetObjectManager()->m_playerSide = humanIsNotAlliance;
        if (systemFlag8 == 1) systemField44 = kForcedSystemField44;
        GetObjectManager()->SetSystemInfo(systemField44, name);
        GetObjectManager()->SetSideValue(0, sideAValue);
        GetObjectManager()->SetSideValue(1, sideBValue);
        GetObjectManager()->SetSideFlag(0, sideAC);
        GetObjectManager()->SetSideFlag(1, sideBC);
        GetObjectManager()->SetControlFlags(controlIsEmpire, controlIsAlliance);
        GetObjectManager()->m_battleKindNotAlliance = battleKindNotAlliance;
    } else {
        GetHeadlessSimulator()->SetSideValue(0, sideAValue);
        GetHeadlessSimulator()->SetSideValue(1, sideBValue);
        GetHeadlessSimulator()->SetSideFlag(0, sideAC);
        GetHeadlessSimulator()->SetSideFlag(1, sideBC);
        GetHeadlessSimulator()->m_sides[0]->m_unkb0 = 0;
        GetHeadlessSimulator()->m_sides[1]->m_unkb0 = 0;
        GetHeadlessSimulator()->m_battleKind = battleKindNotAlliance;
    }
    return 1;
}

// ---- hub forwarders ----------------------------------------------------------------------------------------------

void TacticalGame::PostHubMessage0(int arg) { FUN_005a0a50(FUN_0059ed70(), 0, arg); }

void TacticalGame::PostHubMessage3(int arg) { FUN_005a0a50(FUN_0059ed70(), 3, arg); }

void TacticalGame::PostHubMessage2(int arg) { FUN_005a0a50(FUN_0059ed70(), 2, arg); }

void TacticalGame::PostHubMessage1(int arg) { FUN_005a0a50(FUN_0059ed70(), 1, arg); }

void TacticalGame::PostHubNotice(int arg, int unused) { FUN_005a0ad0(FUN_0059ed70(), arg); }

void TacticalGame::CopyHubPlayerFlags8(uint32_t* out) { FUN_005a0bf0(FUN_0059ed70(), out); }

void TacticalGame::CopyHubPlayerFlags4(uint32_t* out) { FUN_005a0c20(FUN_0059ed70(), out); }

void TacticalGame::PostHubPair(int a, int b) { FUN_005a0b50(FUN_0059ed70(), a, b); }

int TacticalGame::ResetBattleView() {
    GetObjectManager()->ResetView();
    FUN_005c1020(0);
    m_busy = 0;
    return 1;
}

int TacticalGame::HubIsRunning() { return FUN_0059f6e0(FUN_0059ed70()); }

// ---- frame --------------------------------------------------------------------------------------------------------

int TacticalGame::Update(float unused) {
    if (m_busy != 0) return 0;
    if (m_abstract == 1) {
        if (!GetHeadlessSimulator()->Step(kSimulatorStep)) return 0;
        copyOutcome(&m_outcome, &GetHeadlessSimulator()->m_outcome);
        return 1;
    }
    if (AppIsSaveLoadPending() == 1) {
        if (GetObjectManager()->m_pauseCount == 0) {
            GetObjectManager()->Virtual03();
            AppProcessSaveLoad();
            GetObjectManager()->Virtual02();
            return 0;
        }
        AppProcessSaveLoad();
        return 0;
    }
    if (GetObjectManager()->m_finished == 0) {
        const double seconds = m_clock.ElapsedSeconds();
        const float secondsAsFloat = static_cast<float>(seconds);
        const float milliseconds = static_cast<float>(seconds * kMillisecondsPerSecond);
        m_clock.Reset();
        bool finished = false;
        if (m_firstFrame == 0) {
            Advance(milliseconds);
            finished = GetObjectManager()->m_finished != 0;
        } else {
            m_firstFrame = 0;
        }
        if (!finished) {
            while (FUN_005c1490() == 0) {
            }
            FUN_005c1470(secondsAsFloat);
            return 0;
        }
    }
    ++m_busy;
    if (m_busy < 0) m_busy = 0;
    copyOutcome(&m_outcome, &GetObjectManager()->m_outcome);
    ShutDownBattle();
    return 1;
}

int TacticalGame::Advance(float milliseconds) {
    if (m_busy != 0) return 0;
    m_busy = 1;
    GetObjectManager()->Step(milliseconds);
    --m_busy;
    if (m_busy < 0) m_busy = 0;
    return GetObjectManager()->m_finished != 0;
}

void TacticalGame::ForwardToObjectManager(int a, int b, int c) { GetObjectManager()->ForwardEvent(a, b, c); }

void TacticalGame::GetOutcome(BattleOutcome* out) { *out = m_outcome; }

int TacticalGame::AddDeathStar(int family, int id, int notCompleted, int hasFacility, float hull, float hullNow,
                               int hangarFree) {
    if (m_abstract) {
        return GetHeadlessSimulator()->AddDeathStar(family, id, notCompleted, hasFacility, hull, hullNow, hangarFree);
    }
    return GetObjectManager()->AddDeathStar(family, id, notCompleted, hasFacility, hull, hullNow, hangarFree);
}

// ---- teardown of the 3D battle's sound manager and turn-sync object (neither exists in an auto-resolved battle) ---------

void DestroyTacticalSoundManager() {
    if (g_tacticalSoundManager == nullptr) return;
    for (TacticalSoundNode* sound = g_tacticalSoundManager->FirstSound(); sound != nullptr;
         sound = g_tacticalSoundManager->FirstSound()) {
        FUN_005bab10(g_tacticalSoundManager, sound->m_handle);
    }
    SoundManager::Shutdown(0);
    delete g_tacticalSoundManager;
    g_tacticalSoundManager = nullptr;
}

void DestroyTurnSync() {
    TacticalTurnSync* sync = g_tacticalTurnSync;
    if (sync != nullptr) {
        FUN_005bbe20(sync);
        ::operator delete(sync);
    }
    g_tacticalTurnSync = nullptr;
}
