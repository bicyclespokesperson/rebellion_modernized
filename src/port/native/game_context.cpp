#include "port/native/game_context.h"

#include "port/native/audio_context.h"
#include "port/native/lan_game.h"
#include <utility>

#include "app/app_control_cmds.h"
#include "app/app_externals.h"
#include "app/battle_flow.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "audio/sound_manager.h"
#include "data/seed_list.h"
#include "data/table.h"
#include "events/game_manager_state.h"
#include "gfx/directdraw_host.h"
#include "multiplayer/multiplayer_window.h"
#include "ui/animation_step.h"
#include "ui/resource_dll.h"

namespace reb::native
{

GameContext::GameContext() = default;

void GameContext::Start()
{
  if (started_) {
    return;
  }
  started_ = true;

  InitFlowRangeSpan();
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
  audio_ = std::make_unique<AudioContext>();
  lan_ = std::make_unique<LanGame>();
  uiResources_ = std::make_unique<UiResources>();
  directDrawHost_ = std::make_unique<DirectDrawHost>(GetDllLocation(), 1);
}

GameContext::~GameContext()
{
  Shutdown();
}

void GameContext::Shutdown() noexcept
{
  if (!started_) {
    return;
  }

  ReleaseApplication();

  lan_.reset();
  audio_.reset();
  uiResources_.reset();

  directDrawHost_.reset();
  started_ = false;
}

RebellionApplication& GameContext::CreateApplication(HINSTANCE instance, uint32_t flags, const std::string& commandLine)
{
  application_
      = std::make_unique<RebellionApplication>(instance, flags, &commandLine, *directDrawHost_, *this, *uiResources_);
  return *application_;
}

void GameContext::ReleaseApplication() noexcept
{
  if (audio_) {
    audio_->Shutdown();
  }
  application_.reset();
}

AudioContext& GameContext::GetAudioContext() noexcept
{
  return *audio_;
}

LanGame& GameContext::GetLanGame() noexcept
{
  return *lan_;
}

BattleFlow* GameContext::GetBattleFlow() const noexcept
{
  return battleFlow_.get();
}

void GameContext::SetBattleFlow(std::unique_ptr<BattleFlow> battleFlow) noexcept
{
  battleFlow_ = std::move(battleFlow);
}

} // namespace reb::native
