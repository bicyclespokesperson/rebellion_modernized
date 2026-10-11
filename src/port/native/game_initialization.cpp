#include "port/native/game_initialization.h"

#include "app/app_control_cmds.h"
#include "app/battle_flow.h"
#include "app/app_externals.h"
#include "app/rebapp_globals.h"
#include "audio/sound_manager.h"
#include "data/seed_list.h"
#include "data/table.h"
#include "events/game_manager_state.h"
#include "gfx/directdraw_host.h"
#include "multiplayer/multiplayer_window.h"
#include "tactical/tactical_externals.h"
#include "tactical/tactical_formation.h"
#include "tactical/tactical_math.h"
#include "ui/resource_dll.h"
#include "ui/animation_step.h"

extern float g_tacticalFacadeScaleSquared;
extern float g_tacticalGameScaleSquared;
extern float g_tacticalSideScaleSquared;

void StaticInitUnknown6b145c();
void StaticInitUnknown6b1464();
void StaticInitRectAWidth();
void StaticInitRectBWidth();
namespace reb::native {

GameInitialization::GameInitialization() = default;

void GameInitialization::Start() {
    if (started_) return;
    started_ = true;

    // Scalar defaults which the old executable encoded as partial stores.
    g_tacticalFacadeScaleSquared = 9.0f;
    g_tacticalGameScaleSquared = 9.0f;
    g_tacticalSideScaleSquared = 9.0f;
    g_shipMinTurnStep = 0x1.0c1524p-4f;

    // Geometry and UI metrics are derived from their typed source values.
    InitFlowRange2Span();
    InitFlowScaleC();
    InitFlowScaleA();
    InitFlowScaleB();
    InitFlowRangeSpan();
    LobbyStaticInit();
    StaticInitUnknown6b145c();
    StaticInitUnknown6b1464();
    StaticInitRectAWidth();
    StaticInitRectBWidth();
    g_legacyBlockA.Reset();

    // Register the two table factories; this replaces the old final count store.
    InitTableClassRegistration();
    InitSeedListClassRegistration();

    // Animation instances are typed globals; only their prototype registration
    // remains explicit startup work.
    RegisterAnimationStep04();
    RegisterAnimationStep05();
    RegisterAnimationStep14();
    RegisterAnimationStep15();
    RegisterAnimationStep11();
    RegisterAnimationStep12();
    RegisterAnimationStep13();

    // This host depends on platform wiring and must be created after Install().
    directDrawHost_ = std::make_unique<DirectDrawHost>(GetDllLocation(), 1);
    g_directDrawHost = directDrawHost_.get();
}

GameInitialization::~GameInitialization() { Shutdown(); }

void GameInitialization::Shutdown() noexcept {
    if (!started_) return;

    // Drain native-backed objects while the lazy minwin resource module map and
    // DirectSound device are still alive. Their typed destructors then see
    // empty, closed state when the process runtime tears globals down.
    g_smackSound.Stop(1);
    SoundManager::Shutdown(1);
    g_soundSources.Clear();
    g_soundCatalog.Clear();
    g_soundEventQueue.Clear();
    g_cdAudio.Close();
    g_fontTable.Clear();
    g_resourceDlls.Clear();

    g_directDrawHost = nullptr;
    directDrawHost_.reset();
    started_ = false;
}

}  // namespace reb::native
