#include <cstdint>
#include <new>

#include "audio/sound_manager.h"
#include "events/game_clock_hooks.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/sprite_animator_decl.h"
#include "ui/sprite_stage.h"

// The animator constructor and its completion-hook setter (0x4727e0 / 0x472b20, the sprite animator module): the hook
// is called with `callbackArgument` when the script of `sequence` (a {resource id, resource slot} pair) ends.
SpriteAnimator* __thiscall FUN_004727e0(void* memory, int spriteCount);
void __thiscall FUN_00472b20(SpriteAnimator* animator, int callbackArgument, const uint32_t* sequence,
                             SpriteStage* stage, int frames, void (*callback)(int));

namespace {

constexpr uint32_t kLeftDroidId = 0xdead;
constexpr uint32_t kRightDroidId = 0xbeef;
constexpr uint32_t kDroidSlot = 9;
constexpr uint32_t kFrameMs = 0x43;
constexpr int kEarlyGameTicks = 2;
constexpr uint32_t kMouseModeRestoring = 8;
constexpr int kAnimatorSpriteCount = 1;
constexpr int kScriptStartX = 5;
constexpr int kScriptStartY = 5;
constexpr int kHookFrames = 0xf;
constexpr int kLeftDroidHookArgument = 0x12;
constexpr int kRightDroidHookArgument = 5;

struct StageArea {
    int m_x;
    int m_y;
    int m_width;
    int m_height;
};

SpriteStage* NewStage(GalaxyWindow* galaxy, const StageArea& area) {
    void* memory = ::operator new(sizeof(SpriteStage));
    if (!memory) return nullptr;
    return new (memory) SpriteStage(galaxy, galaxy->GetBackground(), area.m_x, area.m_y, area.m_width, area.m_height,
                                    kFrameMs);
}

SpriteAnimator* NewAnimator(SpriteStage* stage, uint32_t droidId) {
    void* memory = ::operator new(0x58);
    SpriteAnimator* animator = memory ? FUN_004727e0(memory, kAnimatorSpriteCount) : nullptr;
    animator->SetId(droidId);
    stage->AddAnimator(animator);
    return animator;
}

void StartIdleScript(SpriteStage* stage, SpriteAnimator* animator, uint32_t droidId, uint32_t scriptId,
                     int hookArgument, void (*hook)(int)) {
    uint32_t sequence = (kDroidSlot << 16) | scriptId;
    stage->StartAnimator(droidId, &sequence, kScriptStartX, kScriptStartY, 0);
    sequence = (kDroidSlot << 16) | scriptId;
    FUN_00472b20(animator, hookArgument, &sequence, stage, kHookFrames, hook);
}

}  // namespace

void GalaxyWindow::CreateDroidAnimations() {
    SoundManager::ReleaseAll(1);
    const int earlyGame = static_cast<uint32_t>(GameHookTickCount()) < kEarlyGameTicks ? 1 : 0;
    if (earlyGame) m_mouseMode = kMouseModeRestoring;
    LoadSideSpriteDlls(m_side, earlyGame);
    if (m_side == 1) {
        m_droidStage = NewStage(this, {0x21d, 0x151, 0x43, 0x74});
        SpriteAnimator* left = NewAnimator(m_droidStage, kLeftDroidId);
        StartIdleScript(m_droidStage, left, kLeftDroidId, 0x193, kLeftDroidHookArgument, UiRequestRedraw);
        m_droidStage2 = NewStage(this, {0x13c, 0x19b, 0x2f, 0x45});
        SpriteAnimator* right = NewAnimator(m_droidStage2, kRightDroidId);
        StartIdleScript(m_droidStage2, right, kRightDroidId, 0x192, kRightDroidHookArgument, UiRequestRedrawAlt);
    } else {
        m_droidStage = NewStage(this, {0, 0x15b, 0x6b, 0x85});
        m_droidStage2 = NewStage(this, {0x12e, 0x191, 0x65, 0x4f});
        SpriteAnimator* left = NewAnimator(m_droidStage, kLeftDroidId);
        StartIdleScript(m_droidStage, left, kLeftDroidId, 0x193, kLeftDroidHookArgument, UiRequestRedraw);
        SpriteAnimator* right = NewAnimator(m_droidStage2, kRightDroidId);
        StartIdleScript(m_droidStage2, right, kRightDroidId, 0x191, kRightDroidHookArgument, UiRequestRedrawAlt);
    }
    m_droidStage->Enable();
    m_droidStage2->Enable();
    m_droidStage->Tick();
    m_droidStage2->Tick();
}
